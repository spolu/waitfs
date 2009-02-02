import waitfs
import sys
import os
import subprocess
from threading import Thread, RLock

fake = 'foo'
replacement = '/bin/ls'

DEBUG = True

def _debug(s):
	if DEBUG:
		print 'DEBUG (%s): %s' % (__name__, repr(s))

class host(object):
	def __init__(self, name):
		self.name = name

	def _run(self, cmd):
		full_cmd = ['ssh', self.name, cmd]
		_debug(full_cmd)
		p = subprocess.Popen(full_cmd, stdout=subprocess.PIPE)
		output = p.communicate()[0]
		return output

	def listdir(self, path):
		# TODO make sure path is safe
		output = self._run('ls -1 "%s"' % path) 
		l = output.split('\n')
		return [os.path.join(path, e) for e in l][:-1]


def instascp(hostname, remote, local):
	"""hostname with abspath for remote and local dirs"""
	h = host(hostname)		# remote host
	c = waitfs.connection()		# waitfs daemon
	m = {}				# map from local to remote paths
	mlock = RLock()			# lock to protect m accesses

	def do_dir(rdir):
		for rpath in h.listdir(rdir):
			# TODO this code is pretty fragile
			# probably a number of path escaping vulns
			lr = len(remote)
			front, back = rpath[:lr], rpath[lr + 1:]
			if front != remote:
				raise Exception('bad remote path: %s' % rpath)
			path = os.path.join(local, back)
			try:
				mlock.acquire()
				m[path] = rpath
			finally:
				mlock.release()
			c.getlinkfor(path)

	do_dir(remote)


def usage():
	print >> sys.stderr, '%s <remote_host> <remote_dir> <local_dir>' % sys.argv[0]
	raise SystemExit()

def cannonicalize_path(p):
	return os.path.abspath(os.path.expandvars(os.path.expanduser(p)))

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



if __name__ == '__main__': main()
