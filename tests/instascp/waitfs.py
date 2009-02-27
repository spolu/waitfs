import socket
from threading import Thread, RLock
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
		self.paths = {} 	# path -> (lid, lpath)
		self.lids = {}		# lid -> (path, lpath)
		self.notifications = [] # queue of handles
		self._sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
		self._sock.connect(SOCK_PATH)
		self._lock = RLock()

		self._serialno = 0
		self.cbs = {}

	def _nextserial(self):
		# TODO must have lock, all calls already do, though
		self._serialno += 1
		return self._serialno

	def _send(self, request):
		try:
			self._lock.acquire()
			
			msg = '%.4d%s' % (len(request), request)
			_debug('send: %s' % msg)
			self._sock.send(msg)
		finally:
			self._lock.release()


	def getlinkfor(self, path):
		"""Porcelain for getlink.  Caller passes in path they want
		a waitfs link for.  The connection will track the association
		from lid -> (path, linkpath) for the user."""
		def getlinkforcb(self, lid, lpath):
			try:
				self._lock.acquire()
				self.paths[path] = (lid, lpath)
				self.lids[lid] = (path, lpath)
				_debug('symlink %s %s' % (path, lpath))
				os.symlink(lpath, path)
			finally:
				self._lock.release()
		self.getlink(getlinkforcb)

	def getlink(self, cb):
		try:
			self._lock.acquire()

			# TODO this needs to have a unique id instead
			serialno = self._nextserial()
			self.cbs[serialno] = cb
			request = '%s %d' % (GETLINK_CMD, serialno)
			_debug('Request: %s' % request)
			self._send(request)
		finally:
			self._lock.release()

	def _handle_getlink_response(self, response):
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

		try:
			self._lock.acquire()
			cb = self.cbs[serialno]
			del self.cbs[serialno]
		finally:
			self._lock.release()

		cb(self, lid, lpath)

	def setlinkfor(self, path, contentpath):
		"""Atomically moves contentpath to path and notifies the waitfs
		daemon by performing setlink on the appropriate (lid, path)"""
		def setlinkforcb(self, lid, path):
			try:
				self._lock.acquire()
				del self.paths[path]
				del self.lids[lid]
			finally:
				self._lock.release()

		try:
			self._lock.acquire()
			lid, lpath = self.paths[path]
			os.rename(contentpath, path)
		finally:
			self._lock.release()

		cb = lambda: setlinkforcb(self, lid, path)
		self.setlink(lid, path, cb)

	def setlink(self, lid, path, cb):
		try:
			self._lock.acquire()
			serialno = self._nextserial()
			self.cbs[serialno] = cb
			request = '%s %d %d %s' % (SETLINK_CMD,
										 serialno,
										 lid, path)
			_debug('Request: %s' % request)
			self._send(request)
		finally:
			self._lock.release()

	def _handle_setlink_response(self, response):
		results = response.split()
		if len(results) != 3 or results[0] != SETLINK_CMD:
			raise unexpected_response(response)
		if results[2] == ERROR_CMD:
			raise failed_response(results)
		elif results[2] != OK_CMD:
			raise unexpected_response(response)

		serialno = int(results[1])
		try:
			self._lock.acquire()
			cb = self.cbs[serialno]
			del self.cbs[serialno]
		finally:
			self._lock.release()

		cb()

	def _handle_readlink_response(self, response, readlink_cb):
		_debug('Handling readlink: %s' % response)
		results = response.split()
		if len(results) != 3 or results[0] != READLINK_CMD:
			raise unexpected_response(response)

		lid, lpath = results[1:]
		lid = int(lid)

		try:
			self._lock.acquire()
			path, lpath2 = self.lids[lid]
			assert(lpath == lpath2)
		finally:
			self._lock.release()

		readlink_cb(path)

	def _handle_error_response(self, response):
		try:
			i = response.find(' ')
			msg = response[i + 1:]
		except ValueError:
			msg = 'Unknown, no reason specified by the server'
		raise failed_response(msg)

	def monitor(self, readlink_cb):
		"""Dispatch incoming data from waitfs, readlinks will be handled by the
		callable specified as readlink_cb, gets local path that was 'accessed'
		back as an argument"""
		rlcb = lambda response: self._handle_readlink_response(response,
															   readlink_cb)
		dispatch = { GETLINK_CMD: self._handle_getlink_response,
					 SETLINK_CMD: self._handle_setlink_response,
					 READLINK_CMD: rlcb,
					 ERROR_CMD: self._handle_error_response }
		import time
		c = 0
		while True:
			c += 1
			time.sleep(.1)
			if not (c % 10):
				_debug('_monitor iteration')
			try:
				self._lock.acquire()
				# read length of incoming data first
				l = int(self._sock.recv(4))
				_debug('Incoming data length: %d' % l)
				response = self._sock.recv(l)
				hdr = response.split()[0]
				_debug('Dispatching to %s from %s' % (hdr, response))
				dispatch[hdr](response)
			finally:
				self._lock.release()
		
	def popaccessed(self):
		try:
			self._lock.acquire()
			if len(self.notifications) == 0:
				return None
			else:
				lid = self.notifications.pop()
				# TODO need to lookup on lid to get path
				path, lpath = self.lids[lid]
				return path
		finally:
			self._lock.release()

