import socket
from threading import Thread, RLock
import os

SOCK_PATH = '/var/waitfs'

OPEN_CMD = 'open'
GETLINK_CMD = 'getlink'
SETLINK_CMD = 'setlink'
OK_CMD = 'ok'
ERROR_CMD = 'error'

DEBUG = True

def _debug(s):
	if DEBUG:
		print 'DEBUG (%s): %s' % (__name__, repr(s))

def synchronized(func):
	def wrapper(self,*__args,**__kw):
		try:
			rlock = self._sync_lock
		except AttributeError:
			rlock = self.__dict__.setdefault('_sync_lock',RLock())
			rlock.acquire()
		try:
			return func(self,*__args,**__kw)
		finally:
			rlock.release()
		wrapper.__name__ = func.__name__
		wrapper.__dict__ = func.__dict__
		wrapper.__doc__ = func.__doc__
		return wrapper

class error(Exception):
	pass

class unexpected_response(Exception):
	pass

class connection(object):
	def __init__(self):
		self.paths = {}
		self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
		self.sock.connect(SOCK_PATH)

	# TODO This seems to be broken, must fix or acquire locks manually
	#@synchronized
	def getlinkfor(self, path):
		"""Porcelain for getlink.  Caller passes in path they want
		a waitfs link for.  The connection will track the association
		from lid -> (path, linkpath) for the user."""
		lid, lpath = self.getlink()
		self.paths[path] = (lid, lpath)
		_debug('symlink %s %s' % (path, lpath))
		os.symlink(lpath, path)

	#@synchronized
	def getlink(self):
		request = '%s\n' % GETLINK_CMD
		_debug('Request: %s' % request)
		self.sock.send(request)

		response = self.sock.recv(1024)
		_debug('Result: %s' % response)

		results = response.split()
		if len(results) != 3:
			raise unexpected_response(response)
		if results[0] == ERROR_CMD:
			raise error(results[0])
		elif results[0] != OK_CMD:
			raise unexpected_response(response)

		lid, lpath = results[1:]
		lid = int(lid)

		return lid, lpath

	@synchronized
	def setlinkfor(self, path, contentpath):
		"""Atomically moves contentpath to path and notifies the waitfs
		daemon by performing setlink on the appropriate (lid, path)"""
		self._sync_lock.acquire()
		lid, lpath = self.paths[path]
		os.rename(contentpath, path)
		self.setlink(lid, path)
		del self.paths[path]

	@synchronized
	def setlink(self, lid, path):
		request = '%s %d %s' % (SETLINK_CMD, lid, path)
		_debug('Request: %s' % request)
		self.sock.send(request)

		response = self.sock.recv(1024)
		_debug('Result: %s' % response)

		results = response.split()
		if len(results) != 1:
			raise unexpected_response(response)
		if results[0] == ERROR_CMD:
			raise error(results[0])
		elif results[0] != OK_CMD:
			raise unexpected_response(response)

		return

	def start_callback_monitor(self, cb):
		self.cb = cb
		l = lambda c: self._callback_monitor(cb)
		self.monitor = Thread(target=l)

	def _callback_monitor(self, cb):
		while True:
			time.sleep(.1)
			try:
				self._sync_lock.acquire()
			finally:
				self._sync_lock.release()
		

