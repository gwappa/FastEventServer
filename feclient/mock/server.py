from __future__ import print_function, division
import argparse
import os
import signal
import time
import json
import threading
import numpy as np

import feclient.proc as proc

reqcond         = threading.Condition()
rescond         = threading.Condition()
request         = None
response        = None
morerequest     = True
moreresponse    = True

delay           = threading.Event()

def alarm_delay(signum, frame):
    global delay
    signal.setitimer(signal.ITIMER_REAL, 0)
    delay.set()

def reader(sock):
    global reqcond, request, morerequest
    while moreresponse:
        try:
            req = sock.recvfrom(2)
            if req is None:
                continue
            with reqcond:
                request = req
                reqcond.notify()
        except OSError:
            print("***socket closed for reading.")
            with reqcond:
                request = None
                morerequest = False
                reqcond.notify()
                return

def responder(sock):
    global rescond, response
    while moreresponse:
        with rescond:
            if response is None:
                if moreresponse == False:
                    return
                rescond.wait()
                if response is None:
                    print("***process finished for writing.")
                    rescond.notify()
                    return
            resp = response
            response = None
            rescond.notify()

        try:
            sock.sendto(*resp)
        except OSError:
            print("***socket closed for writing.")
            return


def run(cfg=None, capacity=10000, outfile=None, lag_us=1000, verbose=False):
    if cfg is None:
        cfg = 'service.cfg'
    if outfile is None:
        outfile = 'mockserver.log.npz'

    global reqcond, rescond, request, response, morerequest, moreresponse

    sock, port = proc.bind_socket(cfg)
    moreresponse = True
    rthread    = threading.Thread(name='reader', target=reader, args=(sock,))
    wthread    = threading.Thread(name='writer', target=responder, args=(sock,))
    signal.signal(signal.SIGALRM, alarm_delay)
    lag_s      = lag_us / 1000000

    packet  = proc.new_packet()
    offset  = 0
    index   = 0
    timestamps  = np.empty(capacity, dtype=float)
    indices     = np.empty(capacity, dtype=int)
    print("start listening on port {} (capacity={}, lag={:d}us)...".format(port, capacity, lag_us))

    rthread.start()
    wthread.start()

    while (offset + index) < capacity:
        with reqcond:
            if request is None:
                if morerequest == True:
                    reqcond.wait()
                    if request is None:
                        break
                else:
                    break
            data, addr = request
            request = None
            reqcond.notify()

        timestamps[offset+index] = time.perf_counter()
        idx, cmd    = packet.unpack(data)
        if ord(cmd) == 3:
            print("***quit signal received")
            break
        if verbose == True:
            print("#{:03d}: '{}' -> {}".format(idx, cmd.decode('ascii'), offset+index))
        indices[offset+index]    = idx
        if idx != index:
            offset += (index - idx)
            index   = idx + 1 # point to the next index
        else:
            index  += 1 # point to the next index

        # arbitrary delay
        if lag_us > 0:
            delay.clear()
            signal.setitimer(signal.ITIMER_REAL, lag_s)
            delay.wait()

        with rescond:
            response = data, addr
            rescond.notify()

    with rescond:
        moreresponse = False
        response     = None
        rescond.notify_all()

    filled = offset + index
    if filled == capacity:
        print("reached capacity ({}). bye.".format(capacity))
    else:
        print("finishes with {} responses.".format(filled))

    wthread.join()
    sock.close()
    rthread.join()

    np.savez(outfile, timestamps=timestamps[:filled], indices=indices[:filled])
    print("mock sever log written to: {}".format(outfile))

def main():
    parser = argparse.ArgumentParser(prog='mock.server.py', 
            description='runs a mock FastEvent server.')
    parser.add_argument('outfile', default='mockserver.log.npz',
            help='the file to write the log output to.')
    parser.add_argument('-i', '--input', dest='cfg', default=None,
            help='the configuration file. defaults to "service.cfg".')
    parser.add_argument('-c', '--capacity', type=int, default=10000,
                        help='the capacity of the log space. defaults to 10000 transactions.')
    parser.add_argument('-l', '--lag_us', type=int, default=0, 
            help='the lag from reception of the request to the response dispatch.')
    parser.add_argument('-v', '--verbose', action='store_true',
                        help='enables log output to stdout.')
    args = vars(parser.parse_args())
    run(**args)

if __name__ == '__main__':
    main()
