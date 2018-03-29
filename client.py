
from __future__ import print_function, division
import json
import socket
import random
import time
import numpy as np

class Client(object):
    SYNC_ON     = b'1\r\n'
    SYNC_OFF    = b'2\r\n'
    EVENT_ON    = b'A\r\n'
    EVENT_OFF   = b'B\r\n'
    ACQ         = b'Y'

    def __init__(self, cfgfile='service.cfg',
                       host='localhost',
                       interactive=False,
                       showresponse=False,
                       timed=False):
        """creates a Client object.

        [parameters]
        cfgfile      -- the path of the configuration file to read from.
        host         -- the host name of the server.
        interactive  -- whether or not to run the instance in 'interactive' mode.
        showresponse -- whether or not to show responses from the server.
        timed        -- whether or not to measure response latencies
        """

        # read config file
        with open(cfgfile, 'r') as fp:
            self.config     = json.load(fp)
        self.socket         = socket.create_connection((host,self.config['port']))
        self.interactive    = interactive
        self.showresponse   = showresponse
        self.timed          = timed
        self.latency        = []
        self.closed         = False
        self.socket.setsockopt(socket.SOL_TCP, socket.TCP_NODELAY, 1)
        print("connected to: {}".format(self.socket.getpeername()))

    def read(self):
        r = b'0'
        while r != Client.ACQ:
            r = self.socket.recv(1)
        return r

    def sync(self, value=True):
        if self.timed == True:
            start = time.perf_counter() * 1e6 # in microsec
        self.socket.sendall(Client.SYNC_ON if value == True else Client.SYNC_OFF)
        resp = self.read()
        if self.timed == True:
            stop  = time.perf_counter() * 1e6 # in microsec
            self.latency.append(stop - start)

        if self.showresponse == True:
            print(resp)
        if self.interactive == True:
            print("sync={}".format("ON" if value == True else "OFF"))

    def event(self, value=True):
        if self.timed == True:
            start = time.perf_counter() * 1e6 # in microsec
        self.socket.sendall(Client.EVENT_ON if value == True else Client.EVENT_OFF)
        resp = self.read()
        if self.timed == True:
            stop  = time.perf_counter() * 1e6 # in microsec
            self.latency.append(stop - start)

        if self.showresponse == True:
            print(resp)
        if self.interactive == True:
            print("value={}".format("ON" if value == True else "OFF"))

    def close(self):
        if self.closed == False:
            self.socket.close()
            self.closed = True
            if self.timed == True:
                latency = np.array(self.latency, dtype=float)
                print('-'*30)
                print('latency: {:.3f}±{:.3f} usec'.format(latency.mean(), latency.std()))
                print('-'*30)

    def random(self, num=1):
        """randomly generates commands (chosen from sync/event) and sends it via the socket.

        [parameter]
        num -- the number of commands to be executed.
        """
        commands = [lambda obj: obj.sync(True),
                    lambda obj: obj.sync(False),
                    lambda obj: obj.event(True),
                    lambda obj: obj.event(False)]
        for i in range(num):
            random.choice(commands)(self)

    def __del__(self):
        if hasattr(self, 'socket'):
            self.close()
