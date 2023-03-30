#pragma once

// #define HIJACK_FS_SYSCALLS
#ifdef HIJACK_FS_SYSCALLS

extern "C" {
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>

/** NOTE: commented out below to avoid declaration conflicts with fcntl.h
 *    This is okay as long as fcntl.h is included, which is done by our
 * shared-memory-based IPC implementation.
 */
/** File open/close related syscalls */
// int open(const char *pathname, int flags, mode_t mode);
// int open64(const char *pathname, int flags, mode_t mode);
// int creat(const char *pathname, int flags, mode_t mode);
// int creat64(const char *pathname, int flags, mode_t mode);
// int openat(int dirfd, const char *pathname, int flags, mode_t mode);
// int openat64(int dirfd, const char *pathname, int flags, mode_t mode);
// int __openat_2(int dirfd, const char *pathname, int flags, mode_t mode)
//     __attribute__((alias("openat")));
// int dup(int oldfd);
// int dup2(int oldfd, int newfd);
// int close(int fd);
// FILE *fopen(const char *path, const char *mode);
// FILE *fopen64(const char *path, const char *mode);
// int fclose(FILE *fp);
// /** File read/write related syscalls */
// ssize_t read(int fd, void *buf, size_t count);
// ssize_t write(int fd, const void *buf, size_t count);
// ssize_t pread(int fd, void *buf, size_t count, off_t offset);
// ssize_t pwrite(int fd, const void *buf, size_t count, off_t offset);
// size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
// size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
// off_t lseek(int fd, off_t offset, int whence);
}

#include <memory>
#include <mutex>

#include "logging.hpp"
namespace midas {
class FSShim {
public:
  FSShim();

  static inline FSShim *global_shim();

  // origial syscalls
  int (*open)(const char *pathname, int flags, mode_t mode);
  int (*open64)(const char *pathname, int flags, mode_t mode);
  int (*creat)(const char *pathname, int flags, mode_t mode);
  int (*creat64)(const char *pathname, int flags, mode_t mode);
  int (*openat)(int dirfd, const char *pathname, int flags, mode_t mode);
  int (*openat64)(int dirfd, const char *pathname, int flags, mode_t mode);
  int (*dup)(int fd);
  int (*dup2)(int newfd, int oldfd);
  int (*close)(int fd);
  FILE *(*fopen)(const char *path, const char *mode);
  FILE *(*fopen64)(const char *path, const char *mode);
  int (*fclose)(FILE *fp);

  ssize_t (*read)(int fd, void *buf, size_t count);
  ssize_t (*write)(int fd, const void *buf, size_t count);
  ssize_t (*pread)(int fd, void *buf, size_t count, off_t offset);
  ssize_t (*pwrite)(int fd, const void *buf, size_t count, off_t offset);
  size_t (*fread)(void *ptr, size_t size, size_t nmemb, FILE *stream);
  size_t (*fwrite)(const void *ptr, size_t size, size_t nmemb, FILE *stream);
  off_t (*lseek)(int fd, off_t offset, int whence);

private:
  void capture_syscalls();
};
} // namespace midas

#include "impl/fs_shim.ipp"

#endif // HIJACK_FS_SYSCALLS