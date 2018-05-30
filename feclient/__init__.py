
from __future__ import print_function, division
import threading
import traceback

# import random
# import time
# import numpy as np

import feclient.proc as proc

MAX_PACKET_INDEX = 255

class DependentThread(threading.Thread):
    def __init__(self):
        super().__init__()
        self._changing  = threading.Lock()
        self._running   = False

    def __setattr__(self, name, value):
        if value == '_running':
            if self._changing.acquire():
                try:
                    super().__setattr__('_running_obj', value)
                finally:
                    self._changing.release()
        else:
            super().__setattr__(name, value)

    def __getattr__(self, name):
        if value == '_running':
            # unlike __setattr__, there is no locking
            super().__getattr__('_running_obj')
        else:
            super().__getattr__(name)
    
    def signal(self):
        """signals this thread to start termination sequence."""
        self._running = False
        
class EventModel(DependentThread):
    """a model that handles single concurrent event, with its
    associated 'value' and its callbacks.

    - you can set a new value by calling `set(value)`.
    - you have to `start()` in order to run the event callback feature.
    - you can add/remove a callback by `.callbacks.append()` or `.callbacks.remove()`.
    
    a callback function must have the single-argument signature
    like `callback(arg)`.
    """

    def __init__(self):
        super().__init__()
        self.cond   = threading.Condition()
        self.value  = None
        self.callbacks = []

    def set(self, value=None):
        """updates the current value of this EventModel"""
        if self._running == True:
            with self.cond:
                self.value = value
                self.cond.notify_all()

    def signal(self):
        super().signal()
        if self.cond.acquire(timeout=1):
            self.cond.notify_all()
            self.cond.release() 
    
    def run(self):
        """waits again and again for the condition variable to be set,
        each time executing callback functions."""
        self._running = True
        while self._running == True:
            if self.cond.acquire(blocking=False):
                try:
                    if self.cond.wait(1):
                        if self._running == False:
                            break
                        else:
                            self._fire_callbacks()
                except:
                    traceback.print_exc()
                finally:
                    self.cond.notify_all()
                    self.cond.release()

    def _fire_callbacks(self):
        for callback in self.callbacks:
            try:
                callback(self.value)
            except:
                traceback.print_exc()

class Subroutine(DependentThread):
    """a thread that runs `iterate()` method each time in the loop.
    a Subroutine object includes an EventModel object; after completion
    of `iterate()`, the Subroutine instance sets the returned value
    to the Model and fires its callbacks.
    
    start the thread by calling `start()` as usual.
    
    you can add/remove callback function(s) by calling its `add_callback()`
    or `remove_callback()` methods. The associated EventModel thread gets
    started directly by the Subroutine object.

    for each subclass, the `iterate()` method is supposed to be overridden.
    """
    def __init__(self):
        super().__init__()
        self.done   = True
        self.event  = EventModel()

    def add_callback(self, callback):
        """adds a callback to the underlying EventModel object."""
        self.event.callbacks.append(callback)

    def remove_callback(self, callback):
        """removes a callback from the underlying EventModel object."""
        self.event.callbacks.remove(callback)

    def run(self):
        """runs `iterate()` and fires an event (callback) in a loop.
        the loop ends when `signal()` is called."""
        self.done    = False
        self._running = True
        self.event.start()

        while self._running == True:
            try:
                ret = self.iterate()
                if self._running == True:
                    self.event.set(ret)
            except:
                traceback.print_exc()
        self.done   = True
        # TODO: signal itself if there is an error?
        self.event.join()

    def signal(self):
        super().signal()
        self.event.signal()

    def iterate(self):
        pass

class Reader(Subroutine):
    def __init__(self, socket):
        """creates the reader for specified socket.
        
        [parameters]
        socket -- the socket object. (only UDP is allowed for the time being)
        """
        super().__init__()
        self.socket     = socket
        self.packet     = proc.new_packet()

    def iterate(self):
        try:
            return self.packet.unpack(self.socket.recv(2))
        except OSError:
            print("***socket seems to have been already closed for reading.")
            self._running = False

class Writer(object):
    def __init__(self, socket):
        self.socket     = socket
        self.packet     = proc.new_packet()
        self.index      = 0
        self.event      = EventModel()

    def start(self):
        """starts the underlying EventModel."""
        self.event.start()

    def join(self):
        """joins the underlying EventModel."""
        self.event.join()

    def signal(self):
        """signals the underlying EventModel."""
        self.event.signal()

    def add_callback(self, callback):
        """adds a callback to the underlying EventModel object."""
        self.event.callbacks.append(callback)

    def remove_callback(self, callback):
        """removes a callback from the underlying EventModel object."""
        self.event.callbacks.remove(callback)

    def send(self, ch):
        try:
            self.socket.sendall(self.packet.pack(self.index, ch))
            self.event.set((self.index, ch))
            ret = self.index
            self.index = self.index + 1 if self.index < MAX_PACKET_INDEX else 0
        except OSError:
            print("***socket seems to have been already closed for writing.")
            ret = -1
        return ret
        
class Session(object):
    """a combination of a Reader and a Writer objects that are associated with a (UDP) socket.
    
    you can monitor read/write actions by adding read/write callbacks to the Session object,
    just like `add_read_callback(callback)`. The callback function is for EventModel, so
    its signature must be `callback(value)`.
    """

    def __init__(self, cfgfile='service.cfg', host='localhost'):
        """prepares the socket and the Reader instance.
        
        [parameters]
        cfgfile -- the config file path that is passed to `open_socket`.
        host    -- the host name that is passed to `open_socket`.
        """
        self.socket         = proc.open_socket(cfgfile, host)
        self.isopen         = True
        self.reader         = Reader(self.socket)
        self.writer         = Writer(self.socket)
        self.reader.start()
        self.writer.start()

    def __enter__(self):
        return self

    def __exit__(self, *args):
        self.close()

    def close(self):
        if self.isopen == True:
            self.writer.signal()
            self.reader.signal()
            self.socket.close()
            self.writer.join()
            self.reader.join()
        self.isopen = False

    def __del__(self):
        self.close()

    def add_read_callback(self, callback):
        self.reader.add_callback(callback)

    def remove_read_callback(self, callback):
        self.reader.remove_callback(callback)

    def add_write_callback(self, callback):
        self.writer.add_callback(callback)

    def remove_write_callback(self, callback):
        self.writer.remove_callback(callback)

    def send(self, ch):
        return self.writer.send(ch)

class Command(object):
    SYNC_ON     = b'1'
    SYNC_OFF    = b'2'
    EVENT_ON    = b'A'
    EVENT_OFF   = b'D'
    ACQ         = b'Y'
    QUIT        = b'X'

