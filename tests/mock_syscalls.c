/* ****************************************************************
 *
 * libpeci — System Call Mock Implementations
 * mock_syscalls.c
 *
 * Implements __wrap_* functions for system calls intercepted
 * via linker --wrap.  Test code sets the mock_* globals to
 * control behaviour.
 *
 *****************************************************************/
#include "mock_syscalls.h"

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/* Real function declarations — provided by linker --wrap */
extern int __real_open(const char* pathname, int flags, ...);
extern int __real_open64(const char* pathname, int flags, ...);
extern int __real_openat(int dirfd, const char* pathname, int flags, ...);
extern int __real_openat64(int dirfd, const char* pathname, int flags, ...);
extern int __real_close(int fd);
extern ssize_t __real_read(int fd, void* buf, size_t count);
extern int __real_ioctl(int fd, unsigned long request, ...);
extern int __real_fstatat(int dirfd, const char* pathname,
                         struct stat* statbuf, int flags);
extern int __real_fstatat64(int dirfd, const char* pathname,
                           struct stat* statbuf, int flags);
extern ssize_t __real_readlink(const char* pathname, char* buf, size_t bufsiz);
extern int __real_scandir(const char* dirp, struct dirent*** namelist,
                         int (*filter)(const struct dirent*),
                         int (*compar)(const struct dirent**,
                                      const struct dirent**));
extern int __real_scandir64(const char* dirp, struct dirent*** namelist,
                           int (*filter)(const struct dirent*),
                           int (*compar)(const struct dirent**,
                                        const struct dirent**));
extern int __real_nanosleep(const struct timespec* req, struct timespec* rem);
extern char* __real_getenv(const char* name);
extern void __real_syslog(int priority, const char* format, ...);

/* ── Global mock state ── */

int mock_active = 0; /* 0 = pass-through to real syscalls */

int mock_ioctl_return = 0;
int mock_ioctl_errno = 0;
int mock_ioctl_call_count = 0;
mock_ioctl_cb_t mock_ioctl_callback = NULL;

int mock_open_return = 3; /* valid fd */
int mock_open_errno = 0;
int mock_open_call_count = 0;

int mock_close_return = 0;
int mock_close_errno = 0;
int mock_close_call_count = 0;

const char* mock_read_data = NULL;
ssize_t mock_read_return = 0;
int mock_read_errno = 0;

int mock_fstatat_return = 0;
int mock_fstatat_errno = 0;
dev_t mock_fstatat_rdev = 0;

const char* mock_readlink_data = NULL;
ssize_t mock_readlink_return = -1;
int mock_readlink_errno = EINVAL;

int mock_scandir_return = -1;
struct dirent** mock_scandir_namelist = NULL;

int mock_nanosleep_return = 0;

const char* mock_getenv_return = NULL;

void mock_reset_all(void)
{
    mock_ioctl_return = 0;
    mock_ioctl_errno = 0;
    mock_ioctl_call_count = 0;
    mock_ioctl_callback = NULL;

    mock_open_return = 3;
    mock_open_errno = 0;
    mock_open_call_count = 0;

    mock_close_return = 0;
    mock_close_errno = 0;
    mock_close_call_count = 0;

    mock_read_data = NULL;
    mock_read_return = 0;
    mock_read_errno = 0;

    mock_fstatat_return = 0;
    mock_fstatat_errno = 0;
    mock_fstatat_rdev = 0;

    mock_readlink_data = NULL;
    mock_readlink_return = -1;
    mock_readlink_errno = EINVAL;

    mock_scandir_return = -1;
    mock_scandir_namelist = NULL;

    mock_nanosleep_return = 0;

    mock_getenv_return = NULL;

    mock_active = 1;
}

/* ── Wrapped system calls ── */

int __wrap_ioctl(int fd, unsigned long request, ...)
{
    va_list ap;
    va_start(ap, request);
    void* argp = va_arg(ap, void*);
    va_end(ap);

    if (!mock_active)
    {
        return __real_ioctl(fd, request, argp);
    }

    mock_ioctl_call_count++;

    if (mock_ioctl_callback)
    {
        return mock_ioctl_callback(request, argp);
    }

    if (mock_ioctl_return != 0)
    {
        errno = mock_ioctl_errno;
        return mock_ioctl_return;
    }

    return 0;
}

static int mock_open_impl(void)
{
    mock_open_call_count++;

    if (mock_open_return < 0)
    {
        errno = mock_open_errno ? mock_open_errno : ENOENT;
        return -1;
    }

    return mock_open_return;
}

int __wrap_open(const char* pathname, int flags, ...)
{
    if (!mock_active)
    {
        va_list ap;
        va_start(ap, flags);
        int mode = va_arg(ap, int);
        va_end(ap);
        return __real_open(pathname, flags, mode);
    }
    return mock_open_impl();
}

int __wrap_open64(const char* pathname, int flags, ...)
{
    if (!mock_active)
    {
        va_list ap;
        va_start(ap, flags);
        int mode = va_arg(ap, int);
        va_end(ap);
        return __real_open64(pathname, flags, mode);
    }
    return mock_open_impl();
}

int __wrap_openat(int dirfd, const char* pathname, int flags, ...)
{
    if (!mock_active)
    {
        va_list ap;
        va_start(ap, flags);
        int mode = va_arg(ap, int);
        va_end(ap);
        return __real_openat(dirfd, pathname, flags, mode);
    }
    return mock_open_impl();
}

int __wrap_openat64(int dirfd, const char* pathname, int flags, ...)
{
    if (!mock_active)
    {
        va_list ap;
        va_start(ap, flags);
        int mode = va_arg(ap, int);
        va_end(ap);
        return __real_openat64(dirfd, pathname, flags, mode);
    }
    return mock_open_impl();
}

int __wrap_close(int fd)
{
    if (!mock_active)
    {
        return __real_close(fd);
    }

    mock_close_call_count++;

    if (mock_close_return != 0)
    {
        errno = mock_close_errno;
        return -1;
    }

    return 0;
}

ssize_t __wrap_read(int fd, void* buf, size_t count)
{
    if (!mock_active)
    {
        return __real_read(fd, buf, count);
    }
    if (mock_read_data && mock_read_return > 0)
    {
        size_t to_copy =
            (size_t)mock_read_return < count ? (size_t)mock_read_return : count;
        memcpy(buf, mock_read_data, to_copy);
        return (ssize_t)to_copy;
    }

    if (mock_read_return < 0)
    {
        errno = mock_read_errno;
    }

    return mock_read_return;
}

static int mock_fstatat_impl(struct stat* statbuf)
{
    if (mock_fstatat_return != 0)
    {
        errno = mock_fstatat_errno;
        return -1;
    }

    memset(statbuf, 0, sizeof(*statbuf));
    statbuf->st_rdev = mock_fstatat_rdev;
    return 0;
}

int __wrap_fstatat(int dirfd, const char* pathname, struct stat* statbuf,
                   int flags)
{
    if (!mock_active)
    {
        return __real_fstatat(dirfd, pathname, statbuf, flags);
    }
    return mock_fstatat_impl(statbuf);
}

int __wrap_fstatat64(int dirfd, const char* pathname, struct stat* statbuf,
                     int flags)
{
    if (!mock_active)
    {
        return __real_fstatat64(dirfd, pathname, statbuf, flags);
    }
    return mock_fstatat_impl(statbuf);
}

ssize_t __wrap_readlink(const char* pathname, char* buf, size_t bufsiz)
{
    if (!mock_active)
    {
        return __real_readlink(pathname, buf, bufsiz);
    }

    if (mock_readlink_return < 0)
    {
        errno = mock_readlink_errno;
        return -1;
    }

    if (mock_readlink_data)
    {
        size_t len = strlen(mock_readlink_data);
        if (len > bufsiz)
            len = bufsiz;
        memcpy(buf, mock_readlink_data, len);
        return (ssize_t)len;
    }

    return mock_readlink_return;
}

static int mock_scandir_impl(struct dirent*** namelist)
{
    if (mock_scandir_return < 0)
    {
        errno = ENOENT;
        return -1;
    }

    *namelist = mock_scandir_namelist;
    return mock_scandir_return;
}

int __wrap_scandir(const char* dirp, struct dirent*** namelist,
                   int (*filter)(const struct dirent*),
                   int (*compar)(const struct dirent**, const struct dirent**))
{
    if (!mock_active)
    {
        return __real_scandir(dirp, namelist, filter, compar);
    }
    return mock_scandir_impl(namelist);
}

int __wrap_scandir64(const char* dirp, struct dirent*** namelist,
                     int (*filter)(const struct dirent*),
                     int (*compar)(const struct dirent**, const struct dirent**))
{
    if (!mock_active)
    {
        return __real_scandir64(dirp, namelist, filter, compar);
    }
    return mock_scandir_impl(namelist);
}

int __wrap_nanosleep(const struct timespec* req, struct timespec* rem)
{
    if (!mock_active)
    {
        return __real_nanosleep(req, rem);
    }
    return mock_nanosleep_return;
}

char* __wrap_getenv(const char* name)
{
    if (!mock_active)
    {
        return __real_getenv(name);
    }
    return (char*)mock_getenv_return;
}

/* syslog is a no-op in tests */
void __wrap_syslog(int priority, const char* format, ...)
{
    (void)priority;
    (void)format;
}
