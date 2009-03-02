import waitfs
import sys
import os
import subprocess
from threading import Thread, RLock

DEBUG = True

def _debug(s):
    if DEBUG:
        print 'DEBUG (%s): %s' % (__name__, repr(s))

def canonicalize_path(p):
    return os.path.abspath(os.path.expandvars(os.path.expanduser(p)))

lasttmp = 0
def mktmppath(dir):
    global lasttmp
    dir = canonicalize_path(dir)
    # TODO race on this, have a lock for it or check contexts it came from
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
        """Returns (type, path) tuples for all inodes in path"""
        # TODO make sure path is safe
        output = self._run('stat -c \'%%F----%%n\' "%s"/*' % path)
        output = output.split('\n')[:-1]
        def extract(line):
          i = line.index('----')
          type = line[:i]
          path = line[i + 4:]
          return (type, path)
        return [extract(l) for l in output]

def instascp(hostname, remote, local):
    """hostname with abspath for remote and local dirs"""
    h = host(hostname)      # remote host
    c = waitfs.connection() # waitfs daemon
    ltor = {}               # map from local to remote paths, need on accessed
    m = {}                  # map from remote to local, need on
    mlock = RLock()         # lock to protect m accesses
    rpathtype = {}

    # we're going to have to stat remotes to decide what they
    # are or fetch that info when we listdir

    def handle_readlink(path):
        try:
            mlock.acquire()
            rpath = m[path]
        finally:
            mlock.release()
        do(path, rpath)

    def do(path, rpath):
        """Handles rpath whether its a dir or file"""
        # TODO Lock!
        rtype = rpathtype[rpath]
        if rtype == 'directory':
            f = do_dir
        elif rtype == 'regular file':
            f = do_file
        else:
            raise Exception('How do I handle files of type %s?' % rtype)
        f(path, rpath)

    def do_file(path, rpath):
        _debug('do_file %s %s' % (path, rpath))
        # TODO race condition allows typcial tmpfile vulns
        tpath = mktmppath('~')
        h.copy(rpath, tpath)
        c.setlinkfor(path)
        _debug('removing %s' % path)
        os.remove(path)
        _debug('renaming %s to %s' % (tpath, path))
        os.rename(tpath, path)
        c.setlinkfor(path)

    def do_dir(path, rpath):
        _debug('do_dir %s %s' % (path, rpath))
        tdirpath = mktmppath('~')
        os.mkdir(tdirpath)
        for rtype, rpath in h.listdir(rpath):
            _debug('setting up waitfs link for %s' % rpath)
            # TODO this code is pretty fragile
            # probably a number of path escaping vulns

            # TODO Lock this dict!?
            rpathtype[rpath] = rtype
            lr = len(remote)
            front, back = rpath[:lr], rpath[lr + 1:]
            _debug('front %s, back %s' % (front, back))
            if front != remote:
                raise Exception('bad remote path: %s' % rpath)
            fpath = os.path.join(local, back)
            try:
                mlock.acquire()
                m[fpath] = rpath
            finally:
                mlock.release()
            _debug('getting link inside of dir for %s' % fpath)
            lpath = c.getlinkfor(fpath)
            tpath = os.path.join(tdirpath, os.path.basename(rpath))
            _debug('symlink %s %s' % (lpath, tpath))
            os.symlink(lpath, tpath)
        _debug('removing %s' % path)
        os.remove(path)
        _debug('renaming %s to %s' % (tdirpath, path))
        os.rename(tdirpath, path)
        c.setlinkfor(path)

    c.start_monitor(handle_readlink)

    rpathtype[remote] = 'directory'
    m[local] = remote
    lpath = c.getlinkfor(local)
    _debug('symlink %s %s' % (lpath, local))
    os.symlink(lpath, local)

def usage():
    print >> sys.stderr, '%s <remote_host> <remote_dir> <local_dir>' % sys.argv[0]
    raise SystemExit()

def main():
    try:
        hostname, remote, local = sys.argv[1:]
        remote = canonicalize_path(remote)
        local = canonicalize_path(local)
    except:
        usage()

    if os.path.exists(local):
        print >> sys.stderr, '<local_dir> must not exist'
        usage()

    _debug('instascp %s %s %s' % (hostname, remote, local))
    instascp(hostname, remote, local)
    import time
    time.sleep(30)

if __name__ == '__main__': main()
