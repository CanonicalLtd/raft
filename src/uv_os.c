#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdio.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/vfs.h>
#include <unistd.h>

#include "assert.h"
#include "uv_os.h"
#include "tracer.h"

void uvJoin(const uvDir dir, const uvFilename filename, uvPath path)
{
    strcpy(path, dir);
    strcat(path, "/");
    strcat(path, filename);
}

void uvDirname(const uvPath path, uvDir dir)
{
    strncpy(dir, path, UV__DIR_MAX_LEN);
    dirname(dir);
}

int uvEnsureDir(struct raft_tracer *tracer, const uvDir dir)
{
    struct stat sb;
    int rv;

    /* Check that the given path doesn't exceed our static buffer limit */
    assert(strnlen(dir, UV__DIR_MAX_LEN + 1) <= UV__DIR_MAX_LEN);

    /* Make sure we have a directory we can write into. */
    rv = stat(dir, &sb);
    if (rv == -1) {
        if (errno == ENOENT) {
            rv = mkdir(dir, 0700);
            if (rv != 0) {
                return errno;
            }
        } else {
            return errno;
        }
    } else if ((sb.st_mode & S_IFMT) != S_IFDIR) {
        return ENOTDIR;
    }

    return 0;
}

int uvSyncDir(const uvDir dir)
{
    int fd;
    int rv;
    fd = open(dir, O_RDONLY | O_DIRECTORY);
    if (fd == -1) {
        return errno;
    }
    rv = fsync(fd);
    close(fd);
    if (rv == -1) {
        return errno;
    }
    return 0;
}

int uvScanDir(const uvDir dir, struct dirent ***entries, int *n_entries)
{
    int rv;
    rv = scandir(dir, entries, NULL, alphasort);
    if (rv == -1) {
        return errno;
    }
    *n_entries = rv;
    return 0;
}

int uvOpenFile(const uvDir dir, const uvFilename filename, int flags, int *fd)
{
    uvPath path;
    uvJoin(dir, filename, path);
    *fd = open(path, flags, S_IRUSR | S_IWUSR);
    if (*fd == -1) {
        return errno;
    }
    return 0;
}

int uvStatFile(const uvDir dir, const uvFilename filename, struct stat *sb)
{
    uvPath path;
    int rv;
    uvJoin(dir, filename, path);
    rv = stat(path, sb);
    if (rv == -1) {
        return errno;
    }
    return 0;
}

int uvMakeFile(const uvDir dir,
               const uvFilename filename,
               struct raft_buffer *bufs,
               unsigned n_bufs)
{
    int flags = O_WRONLY | O_CREAT | O_EXCL;
    int fd;
    int rv;
    size_t size;
    unsigned i;
    size = 0;
    for (i = 0; i < n_bufs; i++) {
        size += bufs[i].len;
    }
    rv = uvOpenFile(dir, filename, flags, &fd);
    if (rv != 0) {
        return RAFT_IOERR;
    }
    rv = writev(fd, (const struct iovec *)bufs, n_bufs);
    if (rv != (int)(size)) {
        rv = errno;
        goto err_after_file_open;
    }
    rv = fsync(fd);
    if (rv == -1) {
        goto err_after_file_open;
    }
    rv = close(fd);
    if (rv == -1) {
        rv = errno;
        goto err;
    }
    return 0;

err_after_file_open:
    close(fd);
err:
    return rv;
}

int uvUnlinkFile(const char *dir, const char *filename)
{
    uvPath path;
    int rv;
    uvJoin(dir, filename, path);
    rv = unlink(path);
    if (rv == -1) {
        return errno;
    }
    return 0;
}

int uvTruncateFile(const uvDir dir, const uvFilename filename, size_t offset)
{
    uvPath path;
    int fd;
    int rv;
    uvJoin(dir, filename, path);
    fd = open(path, O_RDWR);
    if (fd == -1) {
        return errno;
    }
    rv = ftruncate(fd, offset);
    if (rv == -1) {
        close(fd);
        return errno;
    }
    rv = fsync(fd);
    if (rv == -1) {
        close(fd);
        return errno;
    }
    close(fd);
    return 0;
}

int uvRenameFile(const uvDir dir,
                 const uvFilename filename1,
                 const uvFilename filename2)
{
    uvPath path1;
    uvPath path2;
    int rv;
    uvJoin(dir, filename1, path1);
    uvJoin(dir, filename2, path2);
    /* TODO: double check that filename2 does not exist. */
    rv = rename(path1, path2);
    if (rv == -1) {
        return errno;
    }
    rv = uvSyncDir(dir);
    if (rv != 0) {
        return rv;
    }
    return 0;
}

int uvIsEmptyFile(const uvDir dir, const uvFilename filename, bool *empty)
{
    uvPath path;
    struct stat sb;
    int rv;
    uvJoin(dir, filename, path);
    rv = stat(path, &sb);
    if (rv == -1) {
        return errno;
    }
    *empty = sb.st_size == 0 ? true : false;
    return 0;
}

int uvReadFully(const int fd, void *buf, const size_t n)
{
    int rv;
    rv = read(fd, buf, n);
    if (rv == -1) {
        return errno;
    }
    assert(rv >= 0);
    if ((size_t)rv < n) {
        return ENODATA;
    }
    return 0;
}

int uvWriteFully(const int fd, void *buf, const size_t n)
{
    int rv;
    rv = write(fd, buf, n);
    if (rv == -1) {
        return errno;
    }
    assert(rv >= 0);
    if ((size_t)rv < n) {
        return ENODATA;
    }
    return 0;
}

int uvIsFilledWithTrailingZeros(const int fd, bool *flag)
{
    off_t size;
    off_t offset;
    char *data;
    size_t i;
    int rv;

    /* Save the current offset. */
    offset = lseek(fd, 0, SEEK_CUR);

    /* Figure the size of the rest of the file. */
    size = lseek(fd, 0, SEEK_END);
    if (size == -1) {
        return errno;
    }
    size -= offset;

    /* Reposition the file descriptor offset to the original offset. */
    offset = lseek(fd, offset, SEEK_SET);
    if (offset == -1) {
        return errno;
    }

    data = malloc(size);
    if (data == NULL) {
        return ENOMEM;
    }

    rv = uvReadFully(fd, data, size);
    if (rv != 0) {
        return rv;
    }

    for (i = 0; i < (size_t)size; i++) {
        if (data[i] != 0) {
            *flag = false;
            goto done;
        }
    }

    *flag = true;

done:
    free(data);

    return 0;
}

bool uvIsAtEof(const int fd)
{
    off_t offset;
    off_t size;
    offset = lseek(fd, 0, SEEK_CUR); /* Get the current offset */
    size = lseek(fd, 0, SEEK_END);   /* Get file size */
    lseek(fd, offset, SEEK_SET);     /* Restore current offset */
    return offset == size;           /* Compare current offset and size */
}

/* Check if direct I/O is possible on the given fd. */
static int probeDirectIO(int fd, size_t *size)
{
    int flags;             /* Current fcntl flags. */
    struct statfs fs_info; /* To check the file system type. */
    void *buf;             /* Buffer to use for the probe write. */
    int rv;

    flags = fcntl(fd, F_GETFL);
    rv = fcntl(fd, F_SETFL, flags | O_DIRECT);

    if (rv == -1) {
        if (errno != EINVAL) {
            /* UNTESTED: the parameters are ok, so this should never happen. */
            return errno;
        }
        rv = fstatfs(fd, &fs_info);
        if (rv == -1) {
            /* UNTESTED: in practice ENOMEM should be the only failure mode */
            return errno;
        }
        switch (fs_info.f_type) {
            case 0x01021994: /* TMPFS_MAGIC */
            case 0x2fc12fc1: /* ZFS magic */
                *size = 0;
                return 0;
            default:
                /* UNTESTED: this is an unsupported file system. */
                return EINVAL;
        }
    }

    /* Try to peform direct I/O, using various buffer size. */
    *size = 4096;
    while (*size >= 512) {
        buf = aligned_alloc(*size, *size);
        if (buf == NULL) {
            /* UNTESTED: define a configurable allocator that can fail? */
            return ENOMEM;
        }
        memset(buf, 0, *size);
        rv = write(fd, buf, *size);
        free(buf);
        if (rv > 0) {
            /* Since we fallocate'ed the file, we should never fail because of
             * lack of disk space, and all bytes should have been written. */
            assert(rv == (int)(*size));
            return 0;
        }
        assert(rv == -1);
        if (errno != EIO && errno != EOPNOTSUPP) {
            /* UNTESTED: this should basically fail only because of disk errors,
             * since we allocated the file with posix_fallocate. */

            /* FIXME: this is a workaround because shiftfs doesn't return EINVAL
             * in the fnctl call above, for example when the underlying fs is
             * ZFS. */
            if (errno == EINVAL && *size == 4096) {
                *size = 0;
                return 0;
            }

            return errno;
        }
        *size = *size / 2;
    }

    *size = 0;
    return 0;
}

#if defined(RWF_NOWAIT)
/* Check if async I/O is possible on the given fd. */
static int probeAsyncIO(int fd, size_t size, bool *ok)
{
    void *buf;                  /* Buffer to use for the probe write */
    aio_context_t ctx = 0;      /* KAIO context handle */
    struct iocb iocb;           /* KAIO request object */
    struct iocb *iocbs = &iocb; /* Because the io_submit() API sucks */
    struct io_event event;      /* KAIO response object */
    int rv;

    /* Setup the KAIO context handle */
    rv = uvIoSetup(1, &ctx);
    if (rv == -1) {
        /* UNTESTED: in practice this should fail only with ENOMEM */
        return errno;
    }

    /* Allocate the write buffer */
    buf = aligned_alloc(size, size);
    if (buf == NULL) {
        /* UNTESTED: define a configurable allocator that can fail? */
        return ENOMEM;
    }
    memset(buf, 0, size);

    /* Prepare the KAIO request object */
    memset(&iocb, 0, sizeof iocb);
    iocb.aio_lio_opcode = IOCB_CMD_PWRITE;
    *((void **)(&iocb.aio_buf)) = buf;
    iocb.aio_nbytes = size;
    iocb.aio_offset = 0;
    iocb.aio_fildes = fd;
    iocb.aio_reqprio = 0;
    iocb.aio_rw_flags |= RWF_NOWAIT | RWF_DSYNC;

    /* Submit the KAIO request */
    rv = uvIoSubmit(ctx, 1, &iocbs);
    if (rv == -1) {
        /* UNTESTED: in practice this should fail only with ENOMEM */
        free(buf);
        uvIoDestroy(ctx);
        /* On ZFS 0.8 this is not properly supported yet. */
        if (errno == EOPNOTSUPP) {
            *ok = false;
            return 0;
        }
        return errno;
    }

    /* Fetch the response: will block until done. */
    do {
        rv = uvIoGetevents(ctx, 1, 1, &event, NULL);
    } while (rv == -1 && errno == EINTR);

    assert(rv == 1);

    /* Release the write buffer. */
    free(buf);

    /* Release the KAIO context handle. */
    uvIoDestroy(ctx);

    if (event.res > 0) {
        assert(event.res == (int)size);
        *ok = true;
    } else {
        /* UNTESTED: this should basically fail only because of disk errors,
         * since we allocated the file with posix_fallocate and the block size
         * is supposed to be correct. */
        assert(event.res != EAGAIN);
        *ok = false;
    }

    return 0;
}
#endif /* RWF_NOWAIT */

int uvProbeIoCapabilities(const uvDir dir, size_t *direct, bool *async)
{
    uvFilename filename; /* Filename of the probe file */
    uvPath path;         /* Full path of the probe file */
    int fd;              /* File descriptor of the probe file */
    int rv;

    /* Create a temporary probe file. */
    strcpy(filename, ".probe-XXXXXX");
    uvJoin(dir, filename, path);
    fd = mkstemp(path);
    if (fd == -1) {
        rv = errno;
        goto err;
    }
    rv = posix_fallocate(fd, 0, 4096);
    if (rv != 0) {
        goto err_after_file_open;
    }
    unlink(path);

    /* Check if we can use direct I/O. */
    rv = probeDirectIO(fd, direct);
    if (rv != 0) {
        goto err_after_file_open;
    }

#if !defined(RWF_NOWAIT)
    /* We can't have fully async I/O, since io_submit might potentially block.
     */
    *async = false;
#else
    /* If direct I/O is not possible, we can't perform fully asynchronous
     * I/O, because io_submit might potentially block. */
    if (*direct == 0) {
        *async = false;
        goto out;
    }
    rv = probeAsyncIO(fd, *direct, async);
    if (rv != 0) {
        goto err_after_file_open;
    }
#endif /* RWF_NOWAIT */

#if defined(RWF_NOWAIT)
out:
#endif /* RWF_NOWAIT */
    close(fd);
    return 0;

err_after_file_open:
    close(fd);
err:
    assert(rv != 0);
    return rv;
}

int uvSetDirectIo(int fd)
{
    int flags; /* Current fcntl flags */
    int rv;
    flags = fcntl(fd, F_GETFL);
    rv = fcntl(fd, F_SETFL, flags | O_DIRECT);
    if (rv == -1) {
        return errno;
    }
    return 0;
}

char strErrorBuf[2048];

const char *osStrError(int rv)
{
    return strerror_r(rv, strErrorBuf, sizeof strErrorBuf);
}

int uvIoSetup(unsigned nr, aio_context_t *ctxp)
{
    return syscall(__NR_io_setup, nr, ctxp);
}

int uvIoDestroy(aio_context_t ctx)
{
    return syscall(__NR_io_destroy, ctx);
}

int uvIoSubmit(aio_context_t ctx, long nr, struct iocb **iocbpp)
{
    return syscall(__NR_io_submit, ctx, nr, iocbpp);
}

int uvIoGetevents(aio_context_t ctx,
                  long min_nr,
                  long max_nr,
                  struct io_event *events,
                  struct timespec *timeout)
{
    return syscall(__NR_io_getevents, ctx, min_nr, max_nr, events, timeout);
}
