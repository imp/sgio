/*
 * Copyright 2016 Cyril Plisko. All rights reserved.
 * Use is subject to license terms.
 */

#define _GNU_SOURCE

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

typedef struct {
    int fd;
    uint64_t flags;
    size_t blocksize;
    size_t nblocks;
    off_t offset;
} sgiom_t;

#define SGIO_ACTIVE (1ULL << 63)

typedef enum {
    SGIO_READ = 0,
    SGIO_WRITE = 1,
} sgio_rdwr_t;

static sgiom_t sgiom[1];

static int
add_sgio(int fd)
{
    return -1;
}

static int
rem_sgio(int fd)
{
    return -1;
}

static sgiom_t *
lookup_sgio(int fd)
{
    for (int i = 0; i < sizeof(sgiom) / sizeof(sgiom_t); i++) {
        if (fd == sgiom[i].fd) {
            return &sgiom[i];
        }
    }

    return NULL;
}

static bool
sgio_capable(const char *path)
{
    return false;
}

static ssize_t
sgio_rdwr(sgiom_t *sgm, sgio_rdwr_t write, const struct iovec *iov, int iovcnt)
{
    return -1;
}

#define WRAPSYSCALL(ptr, name) \
    if (ptr == NULL) ptr = dlsym(RTLD_NEXT, name); \
    if (ptr == NULL) return -1;

int
open(const char *path, int flags, ...)
{
    static int (*open_)(const char *path, int flags, ...);

    WRAPSYSCALL(open_, "open");

    va_list ap;
    mode_t mode;
    int fd;

    if (flags & O_CREAT) {
        va_start(ap, flags);
        mode = va_arg(ap, mode_t);
        va_end(ap);
        fd = open_(path, flags, mode);
    } else {
        fd = open_(path, flags);
    }

    if (sgio_capable(path)) {
        add_sgio(fd);
    }

    return fd;
}

int
close(int fd)
{
    static int (*close_)(int) = NULL;

    WRAPSYSCALL(close_, "close");

    int rc = close_(fd);

    if (lookup_sgio(fd) != NULL) {
        rem_sgio(fd);
    }

    return rc;
}

ssize_t
pread(int fd, void *buf, size_t count, off_t offset)
{
    static ssize_t (*pread_)(int, void  *, size_t, off_t) = NULL;

    WRAPSYSCALL(pread_, "pread");

    return pread_(fd, buf, count, offset);
}

ssize_t
read(int fd, void *buf, size_t count)
{
    static ssize_t (*read_)(int, void  *, size_t) = NULL;

    WRAPSYSCALL(read_, "read");

    sgiom_t *sgio = lookup_sgio(fd);
    if (sgio == NULL) {
        return read_(fd, buf, count);
    } else {
        const struct iovec iov = {
            .iov_base = buf,
            .iov_len = count
        };
        return sgio_rdwr(sgio, SGIO_READ, &iov, 1);
    }
}

ssize_t
readv(int fd, const struct iovec *iov, int iovcnt)
{
    static ssize_t (*readv_)(int, const struct iovec *, int) = NULL;

    WRAPSYSCALL(readv_, "readv");

    sgiom_t *sgio = lookup_sgio(fd);
    if (sgio == NULL) {
        return readv_(fd, iov, iovcnt);
    } else {
        return sgio_rdwr(sgio, SGIO_READ, iov, iovcnt);
    }
}

ssize_t
pwrite(int fd, const void *buf, size_t count, off_t offset)
{
    static ssize_t (*pwrite_)(int, const void  *, size_t, off_t) = NULL;

    WRAPSYSCALL(pwrite_, "pwrite");

    return pwrite_(fd, buf, count, offset);
}

ssize_t
write(int fd, const void *buf, size_t count)
{
    static ssize_t (*write_)(int, const void  *, size_t) = NULL;

    WRAPSYSCALL(write_, "write");

    sgiom_t *sgio = lookup_sgio(fd);
    if (sgio == NULL) {
        return write_(fd, buf, count);
    } else {
        const struct iovec iov = {
            .iov_base = buf,
            .iov_len = count
        };
        return sgio_rdwr(sgio, SGIO_WRITE, &iov, 1);
    }
}

ssize_t
writev(int fd, const struct iovec *iov, int iovcnt)
{
    static ssize_t (*writev_)(int, const struct iovec *, int) = NULL;

    WRAPSYSCALL(writev_, "writev");

    sgiom_t *sgio = lookup_sgio(fd);
    if (sgio == NULL) {
        return writev_(fd, iov, iovcnt);
    } else {
        return sgio_rdwr(sgio, SGIO_WRITE, iov, iovcnt);
    }
}
