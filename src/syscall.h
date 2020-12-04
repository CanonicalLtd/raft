/* Wrappers for system calls not yet defined in libc. */

#ifndef SYSCALL_H_
#define SYSCALL_H_

#if HAVE_LINUX_AIO_ABI_H
#include <linux/aio_abi.h>
#include <signal.h>
#include <time.h>
#endif

#if HAVE_LINUX_IO_URING_H
#include <linux/io_uring.h>
#endif

#if HAVE_LINUX_AIO_ABI_H
/* AIO */
int io_setup(unsigned nrEvents, aio_context_t *ctxIdp);

int io_destroy(aio_context_t ctxId);

int io_submit(aio_context_t ctxId, long nr, struct iocb **iocbpp);

int io_getevents(aio_context_t ctxId,
                 long minNr,
                 long nr,
                 struct io_event *events,
                 struct timespec *timeout);
#endif

#if HAVE_LINUX_IO_URING_H
/* uring */
int ioUringRegister(int fd,
                    unsigned int opcode,
                    const void *arg,
                    unsigned int nrArgs);

int ioUringSetup(unsigned int entries, struct io_uring_params *p);

int ioUringEnter(int fd,
                 unsigned int toSubmit,
                 unsigned int minComplete,
                 unsigned int flags,
                 sigset_t *sig);
#endif

#endif /* SYSCALL_ */
