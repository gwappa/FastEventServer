import argparse
import threading
import traceback
import time
import signal

import numpy as np

import feclient
import feclient.proc as proc

class ContinuedTimer:
    def __init__(self, interval_sec, callback):
        self.interval = interval_sec
        self.callback = callback
        self._running = False

    def timeout(self, num, frame):
        try:
            self.callback()
        except:
            traceback.print_exc()

    def start(self):
        signal.signal(signal.SIGALRM, self.timeout)
        signal.setitimer(signal.ITIMER_REAL, self.interval, self.interval)

    def cancel(self):
        signal.setitimer(signal.ITIMER_REAL, 0, 0)

class LatencyTest:
    def __init__(self, logfile, cfg='service.cfg', number=1000, freq=1000, verbose=False):
        self.number = number
        self.interval_sec = 1.0/freq

        self.cfg     = cfg
        self.logfile = logfile
        self.timer   = ContinuedTimer(self.interval_sec, self._dispatch)
        self.verbose = verbose

        # session-wise placeholders
        self._offset        = 0
        self._session       = None
        self._number        = None
        self._verbose       = None
        self._matcher       = None
        self._timestamps    = None
        self._update        = threading.Condition()
        self._log           = threading.Condition()
        self._lastreception = None
        self._readdelay     = threading.Lock()
        self._donewriting   = False

    def run(self, number=None, logfile=None, verbose=None):
        self._session = None
        self._number  = None
        self._verbose = None
        self._matcher = None
        self._timestamps = None

        if number is None:
            number = self.number
        if logfile is None:
            logfile = self.logfile
        if verbose is None:
            verbose = self.verbose

        self._offset        = 0
        self._number        = number
        self._verbose       = verbose

        self._matcher       = {}
        for i in range(256):
            self._matcher[i] = -1
        self._timestamps    = np.empty((2,number), dtype=float)
        self._timestamps[:] = np.nan

        with feclient.Session(self.cfg) as sess:
            if verbose == True:
                print("connected.")
            self._session = sess
            sess.add_write_callback(self._request)
            sess.add_read_callback(self._response)
            if verbose == True:
                print("starting requests...")
            self.timer.start()
            while (self._donewriting == False) and (self._offset < self._number):
                with self._update:
                    self._update.wait()
                    self._update.notify()
            self.timer.cancel()
            print("done writing...")
            self._donewriting = True
            print("finishing...")
            sess.remove_read_callback(self._response)
            self._wait_for_reading(sess)

        if verbose == True:
            print("done.")
        np.savez(logfile, timestamps=self._timestamps[:self._offset])
        print("client log written to: {}".format(logfile))

    def _wait_for_reading(self, session):
        session.send(b'\x80')
        while True:
            # intentionally breaking the thread-safety
            target = 2 if self._lastreception is None else self._lastreception + 2
            delay = target - time.perf_counter()
            if delay < .01:
                break
            if self._verbose == True:
                print("waiting for {:.1f} secs...".format(delay))
            time.sleep(delay)

    def _dispatch(self):
        if self._session.send(b'A') == -1:
            self._donewriting = True

    def _request(self, arg):
        if (self._donewriting == True) or (self._offset >= self._number):
            return
        idx, cmd = arg
        with self._log:
            self._timestamps[0,self._offset] = time.perf_counter()
            self._matcher[idx] = self._offset
            self._log.notify()
        if self._verbose == True:
            print("written: #{}".format(idx), flush=True)
        with self._update:
            self._offset += 1
            self._update.notify_all()

    def _response(self, arg):
        idx, cmd = arg
        if self._verbose == True:
            print("read: #{}".format(idx), flush=True)
            
        with self._log:
            while (self._donewriting == False) and (self._matcher[idx] == -1):
                self._log.wait()
            if self._matcher[idx] == -1:
                self._log.notify_all()
                return
            offset = self._matcher[idx]
            timestamp = time.perf_counter()
            # intentionally breaking the thread-safety
            self._lastreception = timestamp
            self._timestamps[1,offset] = timestamp
            self._matcher[idx] = -1

def main():
    parser = argparse.ArgumentParser(prog='feclient.test.latency', description='tests the latency of the I/O between the server.')
    parser.add_argument('logfile', help='the log file to write the output to.')
    parser.add_argument('-i', '--input', dest='cfg', default='service.cfg',
            help='the configuration file of the service. defaults to "service.cfg".')
    parser.add_argument('-N', '--number', type=int, default=1000,
            help='the number of requests to be made. defaults to 1000')
    parser.add_argument('-F', '--freq', type=int, default=5000,
            help='the approximate frequency of request dispatch per second. defaults to 5000 (i.e. 5kHz)')
    parser.add_argument('-v', '--verbose', action='store_true',
            help='enables verbose output.')
    args = vars(parser.parse_args())
    LatencyTest(**args).run()

if __name__ == '__main__':
    main()

