import socket

SOCK_PATH = '/var/waitfs'

OPEN_CMD = 'open'
GETLINK_CMD = 'getlink'
SETLINK_CMD = 'setlink'
OK_CMD = 'ok'
ERROR_CMD = 'error'

DEBUG = True

def _debug(s):
	if DEBUG:
		print 'DEBUG: %s' % repr(s)

class error(Exception):
	pass

class unexpected_response(Exception):
	pass

class connection(object):
	def __init__(self):
		self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
		self.sock.connect(SOCK_PATH)

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

		lid, path = results[1:]
		lid = int(lid)

		return lid, path

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



