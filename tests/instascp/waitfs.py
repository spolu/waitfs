import socket
from threading import Thread, RLock, Condition
import os

SOCK_PATH = '/var/waitfs'

OPEN_CMD = 'open'
GETLINK_CMD = 'getlink'
SETLINK_CMD = 'setlink'
READLINK_CMD = 'readlink'
OK_CMD = 'ok'
ERROR_CMD = 'error'

DEBUG = True

def _debug(s):
    if DEBUG:
        print 'DEBUG (%s): %s' % (__name__, repr(s))

class failed_response(Exception):
    pass

class unexpected_response(Exception):
    pass

class connection(object):
    def __init__(self):
        self._paths = {}     # path -> (lid, lpath)
        self._lids = {}      # lid -> (path, lpath)
        self._pl_lock = RLock() # lock for lids and paths

        self._sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self._sock.connect(SOCK_PATH)
        self._sock_lock = RLock()
        self._readlink_cb = None

        self._serialno = self.serial()

        self._messages = {}
        self._messages_serial = self.serial()
        self._messages_cv = Condition()

    # TODO How do I get thread safety in generators?
    # with and wo finally on release don't seem to work
    def serial(self):
        serial = 0
        while True:
            serial += 1
            yield serial

    def _send(self, request):
        try:
            self._sock_lock.acquire()

            msg = '%.4d%s' % (len(request), request)
            _debug('send: %s' % msg)
            self._sock.send(msg)
        finally:
            self._sock_lock.release()

    def _block_until(self, pred):
        while True:
            try:
                _debug('Attempting to acquire responses_cv')
                self._messages_cv.acquire()
                for k, v in self._messages.items():
                    if pred(v):
                        _debug('Found matching response, returning')
                        del self._messages[k]
                        return v
                _debug('Going to sleep waiting for response')
                self._messages_cv.wait()
            finally:
                self._messages_cv.release()

    def getlinkfor(self, path):
        """Porcelain for getlink.  Caller passes in path they want
        a waitfs link for.  The connection will track the association
        from lid -> (path, linkpath) for the user."""
        _debug('getlinkfor %s' % path)
        lid, lpath = self.getlink()
        try:
            self._pl_lock.acquire()
            self._paths[path] = (lid, lpath)
            self._lids[lid] = (path, lpath)
        finally:
            self._pl_lock.release()
        return lpath

    def getlink(self):
        _debug('getlink')
        serialno = self._serialno.next()
        request = '%s %d' % (GETLINK_CMD, serialno)
        _debug('Request: %s' % request)
        self._send(request)

        def gl_pred(res):
            r = res.split()
            return r[0] == GETLINK_CMD and int(r[1]) == serialno
        response = self._block_until(gl_pred)

        results = response.split()
        if len(results) != 5 or results[0] != GETLINK_CMD:
            print repr(results)
            raise unexpected_response(response)
        if results[2] == ERROR_CMD:
            raise failed_response(results)
        elif results[2] != OK_CMD:
            raise unexpected_response(response)

        serialno = int(results[1])

        lid, lpath = results[3:]
        lid = int(lid)

        return lid, lpath

    def setlinkfor(self, path):
        """After an atomic move of contentpath to path, notifies the waitfs
        daemon by performing setlink on the appropriate (lid, path)"""
        _debug('setlinkfor %s' % path)
        try:
            self._pl_lock.acquire()
            lid, lpath = self._paths[path]
        finally:
            self._pl_lock.release()

        self.setlink(lid, path)

        try:
            self._pl_lock.acquire()
            del self._paths[path]
            del self._lids[lid]
        finally:
            self._pl_lock.release()

    def setlink(self, lid, path):
        _debug('setlink %d %s' % (lid, path))
        serialno = self._serialno.next()
        request = '%s %d %d %s' % (SETLINK_CMD,
                                     serialno,
                                     lid, path)
        _debug('Request: %s' % request)
        self._send(request)

        def sl_pred(res):
            r = res.split()
            return r[0] == SETLINK_CMD and int(r[1]) == serialno
        response = self._block_until(sl_pred)

        results = response.split()
        if len(results) != 3 or results[0] != SETLINK_CMD:
            raise unexpected_response(response)
        if results[2] == ERROR_CMD:
            raise failed_response(results)
        elif results[2] != OK_CMD:
            raise unexpected_response(response)

    def _handle_readlink_response(self, response):
        _debug('Handling readlink: %s' % response)
        results = response.split()
        if len(results) != 3 or results[0] != READLINK_CMD:
            raise unexpected_response(response)

        lid, lpath = results[1:]
        lid = int(lid)

        try:
            self._pl_lock.acquire()
            path, lpath2 = self._lids[lid]
            assert(lpath == lpath2)
            self.readlink_cb(path)
        except KeyError:
            _debug('WARNING: discarding readlink on unknown lid')
            _debug('WARNING: probably result of repeated readlinks')
        finally:
            self._pl_lock.release()

    def start_monitor(self, readlink_cb):
        """Dispatch incoming data from waitfs, readlinks will be handled by the
        callable specified as readlink_cb, gets local path that was 'accessed'
        back as an argument"""
        self.readlink_cb = readlink_cb
        c = self

        import time
        import select
        class Monitor(Thread):
            def __init__(self):
                super(Monitor, self).__init__()
                self.daemon = True

            def run(self):
                incmsg = ''
                incmsglen = 0
                remain = 4
                while True:
                    _debug('_monitor iteration')
                    readable, _, _ = select.select([c._sock], [], [])
                    _debug('Processing incoming message')
                    if c._sock in readable:
                        try:
                            c._sock_lock.acquire()
                            _debug('Need to recv %d more' % remain)
                            r = c._sock.recv(remain)
                            _debug('recv: %s' % repr(r))
                            incmsg += r
                            remain -= len(r)
                            if incmsglen == 0: # if we are still finding len hdr
                                if remain == 0:
                                    incmsglen = int(incmsg[:4])
                                    remain = incmsglen
                                    incmsg = ''
                                    _debug('recv inc data length: %d' % incmsglen)
                            else: # if we are reading actual msg after header
                                if remain == 0:
                                    _debug('recv inc data %s' % incmsg)
                                    try:
                                        c._messages_cv.acquire()
                                        s = c._messages_serial.next()
                                        c._messages[s] = incmsg
                                        print c._messages
                                        c._messages_cv.notifyAll()
                                    finally:
                                        c._messages_cv.release()
                                    incmsg = ''
                                    incmsglen = 0
                                    remain = 4
                        finally:
                            c._sock_lock.release()
        Monitor().start()

        class ReadLinkWorker(Thread):
            def __init__(self, response):
                super(ReadLinkWorker, self).__init__()
                self.response = response
                self.daemon = True

            def run(self):
                _debug('ReadLinkWorker invoking callback')
                c._handle_readlink_response(self.response)

        class ReadLinkDispatcher(Thread):
            def __init__(self):
                super(ReadLinkDispatcher, self).__init__()
                self.daemon = True

            def run(self):
                while True:
                    def rl_pred(res):
                        r = res.split()
                        return r[0] == READLINK_CMD
                    _debug('ReadLinkDispatcher waiting for interesting messages')
                    response = c._block_until(rl_pred)
                    _debug('ReadLinkDispatcher starting handler thread')
                    ReadLinkWorker(response).start()

        ReadLinkDispatcher().start()

