#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#define FUSE_USE_VERSION 26
#include <fuse.h>

static const char *test_str = "Hello World!\n";
static const char *test_path = "/0101010101010101";
static const char *orig_path = "/tmp/test/test_file";

static int test_getattr(const char *path, struct stat *stbuf)
{
  int res = 0;

  memset(stbuf, 0, sizeof(struct stat));
  if(strcmp(path, "/") == 0) {
    stbuf->st_mode = S_IFDIR | 0755;
    stbuf->st_nlink = 2;
  }
  else if(strcmp(path, test_path) == 0) {
    stbuf->st_mode = S_IFLNK | 0755;
    stbuf->st_nlink = 1;
    stbuf->st_size = strlen(test_str);
  }
  else
    res = -ENOENT;

  return res;
}

static int test_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                         off_t offset, struct fuse_file_info *fi)
{
  (void) offset;
  (void) fi;

  if(strcmp(path, "/") != 0)
    return -ENOENT;

  filler(buf, ".", NULL, 0);
  filler(buf, "..", NULL, 0);
  filler(buf, test_path + 1, NULL, 0);

  return 0;
}

static int test_readlink (const char *path, char * buf, size_t bufsize)
{
  int fd;

  if(strcmp(path, test_path) != 0)
    return -ENOENT;
  
  unlink (orig_path);

  sleep (2); // Simulate downloading stall

  if ((fd = open (orig_path,
		  O_CREAT | O_WRONLY)) < 0)
    return -errno;

  fchmod (fd, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  write (fd, test_str, strlen (test_str));
  close (fd);

  strncpy (buf, orig_path, bufsize);

  return 0;
}

static struct fuse_operations test_oper = {
  .getattr = test_getattr,
  .readdir = test_readdir,
  .readlink = test_readlink
};

int main(int argc, char *argv[])
{
  return fuse_main(argc, argv, &test_oper, NULL);
}
