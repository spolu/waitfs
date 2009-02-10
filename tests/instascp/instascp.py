import waitfs
import sys
import os
import subprocess
from threading import Thread, RLock

DEBUG = True

def _debug(s):
	if DEBUG:
		print 'DEBUG (%s): %s' % (__name__, repr(s))

def cannonicalize_path(p):
	return os.path.abspath(os.path.expandvars(os.path.expanduser(p)))

lasttmp = 0
def mktmppath(dir):
	global lasttmp
	lasttmp += 1
	return os.path.join(dir, '_tmp_%d' % lasttmp)

class host(object):
	def __init__(self, name):
		self.name = name

	def _run(self, cmd):
		full_cmd = ['ssh', self.name, cmd]
		_debug(full_cmd)
		p = subprocess.Popen(full_cmd, stdout=subprocess.PIPE)
		output = p.communicate()[0]
		return output

	def copy(self, rpath, path):
		full_cmd = ['scp', '%s:%s' % (self.name, rpath), path]
		_debug(full_cmd)
		if subprocess.call(full_cmd) != 0:
			raise Exception('scp failed: %s' % repr(full_cmd))

	def listdir(self, path):
		# TODO make sure path is safe
		output = self._run('ls -1 "%s"' % path) 
		l = output.split('\n')
		return [os.path.join(path, e) for e in l][:-1]

def instascp(hostname, remote, local):
	"""hostname with abspath for remote and local dirs"""
	h = host(hostname)		# remote host
	c = waitfs.connection()	# waitfs daemon
	ltor = {}				# map from local to remote paths, need on accessed
	m = {}					# map from remote to local, need on
	mlock = RLock()			# lock to protect m accesses

	# we're going to have to stat remotes to decide what they
	# are or fetch that info when we listdir

	def local_accessed(path):
		try:
			mlock.acquire()
			rpath = m[path]
		finally:
			mlock.release()
		do(path, rpath)

	def do(path, rpath):
		"""Handles rpath whether its a dir or file"""
		# assume it's a file for now
		do_file(path, rpath)

	def do_file(path, rpath):
		# TODO race condition allows typcial tmpfile vulns
		tpath = mktmppath(local)
		h.copy(rpath, tpath)
		c.setlinkfor(path, tpath)

	def do_dir(path, rpath):
		for rpath in h.listdir(rpath):
			# TODO this code is pretty fragile
			# probably a number of path escaping vulns
			lr = len(remote)
			front, back = rpath[:lr], rpath[lr + 1:]
			if front != remote:
				raise Exception('bad remote path: %s' % rpath)
			fpath = os.path.join(local, back)
			try:
				mlock.acquire()
				m[fpath] = rpath
			finally:
				mlock.release()
			c.getlinkfor(fpath)

	# get the top level directory listing
	do_dir(local, remote)

	c.monitor()


def usage():
	print >> sys.stderr, '%s <remote_host> <remote_dir> <local_dir>' % sys.argv[0]
	raise SystemExit()

def main():
	try:
		hostname, remote, local = sys.argv[1:]
		remote = cannonicalize_path(remote)
		local = cannonicalize_path(local)
	except:
		usage()
	
	if not os.path.isdir(local):
		print >> sys.stderr, '<local_dir> must be a dir'
		usage()

	_debug('instascp %s %s %s' % (hostname, remote, local))
	instascp(hostname, remote, local)
	import time
	time.sleep(30)

if __name__ == '__main__': main()
