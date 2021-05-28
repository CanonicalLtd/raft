#include "uv_os.h"

#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <uv.h>

#include "assert.h"
#include "err.h"

#ifdef __linux__
#include <sys/eventfd.h>
#include <sys/vfs.h>
#include "syscall.h"
#elif defined(__FreeBSD__) || defined(__APPLE__)
#include <sys/param.h>
#include <sys/mount.h>
#endif

/* Default permissions when creating a directory. */
#define DEFAULT_DIR_PERM 0700

int UvOsOpen(const char *path, int flags, int mode, uv_file *fd)
{
    struct uv_fs_s req;
    int rv;
    rv = uv_fs_open(NULL, &req, path, flags, mode, NULL);
    if (rv < 0) {
        return rv;
    }
    *fd = rv;
    return 0;
}

int UvOsClose(uv_file fd)
{
    struct uv_fs_s req;
    return uv_fs_close(NULL, &req, fd, NULL);
}

#ifdef _WIN32
int UvOsFallocate(uv_file fd, off_t offset, off_t len)
{
    // This is not really correct. A better implementation would probably
    // be to open the file, seek to offset and write zeros.
    int rv;
    rv = ftruncate(fd, offset+len);
    if (rv != 0) {
      return rv;
    }
    rv = UvOsFsync(fd);
    if (rv != 0) {
        return rv;
    }
    return 0;
}
#else

/* Emulate fallocate(). Mostly taken from glibc's implementation. */
static int uvOsFallocateEmulation(int fd, off_t offset, off_t len)
{
    ssize_t increment;
    struct statfs f;
    int rv;

    rv = fstatfs(fd, &f);
    if (rv != 0) {
        return errno;
    }

    if (f.f_bsize == 0) {
        increment = 512;
    } else if (f.f_bsize < 4096) {
        increment = f.f_bsize;
    } else {
        increment = 4096;
    }

    for (offset += (len - 1) % increment; len > 0; offset += increment) {
        len -= increment;
        rv = (int)pwrite(fd, "", 1, offset);
        if (rv != 1) {
            if (errno == ENOSPC)
                return UV_ENOSPC;
            return errno;
        }
    }

    return 0;
}

int UvOsFallocate(uv_file fd, off_t offset, off_t len)
{
    int rv;
#ifdef __linux__
    rv = posix_fallocate(fd, offset, len);
#else // MacOS
    fstore_t store = {F_ALLOCATECONTIG, F_PEOFPOSMODE, offset, len, -1};
    rv = fcntl(fd, F_PREALLOCATE, &store);
    if (-1 == rv) {
        // Perhaps we are too fragmented, allocate non-continuous
        store.fst_flags = F_ALLOCATEALL;
        rv = fcntl(fd, F_PREALLOCATE, &store);
        if (-1 == rv) {
            rv = EOPNOTSUPP;
            if (errno == ENOSPC)
                return UV_ENOSPC;
        }
    } else {
        rv = ftruncate(fd, len);
    }
#endif
    if (rv != 0) {
        /* From the manual page:
         *
         *   posix_fallocate() returns zero on success, or an error number on
         *   failure.  Note that errno is not set.
         */
        if (rv != EOPNOTSUPP) {
            return -rv;
        }
        /* This might be a libc implementation (e.g. musl) that doesn't
         * implement a transparent fallback if fallocate() is not supported
         * by the underlying file system. */
        rv = uvOsFallocateEmulation(fd, offset, len);
    }
    return rv;
}
#endif

int UvOsTruncate(uv_file fd, off_t offset)
{
    struct uv_fs_s req;
    return uv_fs_ftruncate(NULL, &req, fd, offset, NULL);
}

int UvOsFsync(uv_file fd)
{
    struct uv_fs_s req;
    return uv_fs_fsync(NULL, &req, fd, NULL);
}

int UvOsFdatasync(uv_file fd)
{
    struct uv_fs_s req;
    return uv_fs_fdatasync(NULL, &req, fd, NULL);
}

int UvOsStat(const char *path, uv_stat_t *sb)
{
    struct uv_fs_s req;
    int rv;
    rv = uv_fs_stat(NULL, &req, path, NULL);
    if (rv != 0) {
        return rv;
    }
    memcpy(sb, &req.statbuf, sizeof *sb);
    return 0;
}

int UvOsWrite(uv_file fd,
              const uv_buf_t bufs[],
              unsigned int nbufs,
              int64_t offset)
{
    struct uv_fs_s req;
    return uv_fs_write(NULL, &req, fd, bufs, nbufs, offset, NULL);
}

int UvOsUnlink(const char *path)
{
    struct uv_fs_s req;
    return uv_fs_unlink(NULL, &req, path, NULL);
}

int UvOsRename(const char *path1, const char *path2)
{
    struct uv_fs_s req;
    return uv_fs_rename(NULL, &req, path1, path2, NULL);
}

void UvOsJoin(const char *dir, const char *filename, char *path)
{
    assert(UV__DIR_HAS_VALID_LEN(dir));
    assert(UV__FILENAME_HAS_VALID_LEN(filename));
    strcpy(path, dir);
#ifdef _WIN32
    strcat(path, "\\");
#else
    strcat(path, "/");
#endif
    strcat(path, filename);
}

#ifdef __linux__
int UvOsIoSetup(unsigned nr, aio_context_t *ctxp)
{
    int rv;
    rv = io_setup(nr, ctxp);
    if (rv == -1) {
        return -errno;
    }
    return 0;
}

int UvOsIoDestroy(aio_context_t ctx)
{
    int rv;
    rv = io_destroy(ctx);
    if (rv == -1) {
        return -errno;
    }
    return 0;
}

int UvOsIoSubmit(aio_context_t ctx, long nr, struct iocb **iocbpp)
{
    int rv;
    rv = io_submit(ctx, nr, iocbpp);
    if (rv == -1) {
        return -errno;
    }
    assert(rv == nr); /* TODO: can something else be returned? */
    return 0;
}

int UvOsIoGetevents(aio_context_t ctx,
                    long min_nr,
                    long max_nr,
                    struct io_event *events,
                    struct timespec *timeout)
{
    int rv;
    do {
        rv = io_getevents(ctx, min_nr, max_nr, events, timeout);
    } while (rv == -1 && errno == EINTR);

    if (rv == -1) {
        return -errno;
    }
    assert(rv >= min_nr);
    assert(rv <= max_nr);
    return rv;
}

int UvOsEventfd(unsigned int initval, int flags)
{
    int rv;
    /* At the moment only UV_FS_O_NONBLOCK is supported */
    assert(flags == UV_FS_O_NONBLOCK);
    flags = EFD_NONBLOCK | EFD_CLOEXEC;
    rv = eventfd(initval, flags);
    if (rv == -1) {
        return -errno;
    }
    return rv;
}

int UvOsSetDirectIo(uv_file fd)
{
    int flags; /* Current fcntl flags */
    int rv;
    flags = fcntl(fd, F_GETFL);
    rv = fcntl(fd, F_SETFL, flags | UV_FS_O_DIRECT);
    if (rv == -1) {
        return -errno;
    }
    return 0;
}
#endif
