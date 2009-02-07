import socket
from threading import Thread, RLock
import os

SOCK_PATH = '/var/waitfs'

OPEN_CMD = 'open'
GETLINK_CMD = 'getlink'
SETLINK_CMD = 'setlink'
ACCESSED_CMD = 'accessed'
OK_CMD = 'ok'
ERROR_CMD = 'error'

DEBUG = True

def _debug(s):
	if DEBUG:
		print 'DEBUG (%s): %s' % (__name__, repr(s))

class error(Exception):
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

	def _recv(self, wait_for):
		"""Blocking readline, reads "one" response from the server.
		If the prefix of the message is not "wait_for" it will
		dispatch the message as a callback and readline again until
		the expected response header appears.  If something besides
		a callback prefix or wait_for prefix appears it is an error."""
		try:
			self._lock.acquire()

			self._sock.setblocking(wait_for != None)
			response = self._sock.recv(1024)
			_debug('Result: %s' % response)
			if wait_for == None:
				if not response.startswith(ACCESSED_CMD):
					raise unexpected_response(repr(response))
				fields = response.split()
				handle = int(fields[1])
				self.notifications.append(handle)
			else:
				if response.startswith(wait_for):
					if response.startswith(ACCESSED_CMD):
						fields = response.split()
						handle = int(fields[2])
						self.notifications.append(handle)
					elif response.startswith(wait_for):
						return response
					else:
						raise unexpected_response(repr(response))
		finally:
			self._lock.release()

	def _send(self, request):
		try:
			self._lock.acquire()
			
			self._sock.send(request)
		finally:
			self._lock.release()


	def getlinkfor(self, path):
		"""Porcelain for getlink.  Caller passes in path they want
		a waitfs link for.  The connection will track the association
		from lid -> (path, linkpath) for the user."""
		try:
			self._lock.acquire()

			lid, lpath = self.getlink()
			self.paths[path] = (lid, lpath)
			self.lids[lid] = (path, lpath)
			_debug('symlink %s %s' % (path, lpath))
			os.symlink(lpath, path)
		finally:
			self._lock.release()

	def getlink(self):
		try:
			self._lock.acquire()

			request = '%s\n' % GETLINK_CMD
			_debug('Request: %s' % request)
			self._send(request)

			response = self._recv(GETLINK_CMD)

			results = response.split()
			if len(results) != 4 or results[0] != GETLINK_CMD:
				raise unexpected_response(response)
			if results[1] == ERROR_CMD:
				raise error(results)
			elif results[1] != OK_CMD:
				raise unexpected_response(response)

			lid, lpath = results[2:]
			lid = int(lid)

			return lid, lpath
		finally:
			self._lock.release()

	def setlinkfor(self, path, contentpath):
		"""Atomically moves contentpath to path and notifies the waitfs
		daemon by performing setlink on the appropriate (lid, path)"""
		try:
			self._lock.acquire()

			self._lock.acquire()
			lid, lpath = self.paths[path]
			os.rename(contentpath, path)
			self.setlink(lid, path)
			del self.paths[path]
			del self.lids[lid]
		finally:
			self._lock.release()

	def setlink(self, lid, path):
		try:
			self._lock.acquire()
			request = '%s %d %s' % (SETLINK_CMD, lid, path)
			_debug('Request: %s' % request)
			self._send(request)

			response = self._recv(SETLINK_CMD)

			results = response.split()
			if len(results) != 2 or results[0] != SETLINK_CMD:
				raise unexpected_response(response)
			if results[1] == ERROR_CMD:
				raise error(results)
			elif results[1] != OK_CMD:
				raise unexpected_response(response)
		finally:
			self._lock.release()

	def start_monitor(self):
		self.monitor = Thread(target=self._monitor)
		self.monitor.start()

	def _monitor(self):
		import time
		c = 1
		while True:
			time.sleep(.1)
			c += 1
			_debug('_monitor iteration')
			try:
				self._lock.acquire()
				_debug('_monitor iteration - locked')
				if c % 10 != 0:
					i = c / 10 + 1
					self.notifications.append(i)
					_debug('Faking access to: %d' % i)
				self._recv(None) # causes _recv to just queue cbs
			finally:
				self._lock.release()
				_debug('_monitor iteration - unlocked')
		
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

