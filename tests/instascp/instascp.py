import waitfs
import os

fake = 'foo'
replacement = '/bin/ls'

def main():
	c = waitfs.connection()
	lid, path = c.getlink()
	print 'Created waitfs target', lid, path

	os.symlink(path, fake)

	# need some means to get callbacks from waitfs

	c.setlink(lid, replacement)
	print 'Access should now be redirected by waitfs'

if __name__ == '__main__': main()
