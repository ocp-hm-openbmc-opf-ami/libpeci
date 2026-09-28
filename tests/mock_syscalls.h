/* ****************************************************************
 *
 * libpeci — System Call Mocks
 * mock_syscalls.h
 *
 * Provides mock implementations of system calls used by peci.c
 * (open, close, ioctl, read, openat, fstatat, readlink, scandir,
 *  nanosleep) via linker --wrap mechanism.
 *
 * Test code controls mock behaviour through global variables
 * declared here.
 *
 *****************************************************************/
#pragma once

#include <dirent.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C"
{
#endif

/* ── ioctl mock control ── */
extern int mock_ioctl_return;      /* return value from ioctl */
extern int mock_ioctl_errno;       /* errno to set on failure */
extern int mock_ioctl_call_count;  /* number of ioctl calls */

/* Callback for ioctl — if non-NULL, called with (cmd, argp).
 * Can be used to populate output structs. */
typedef int (*mock_ioctl_cb_t)(unsigned long cmd, void* argp);
extern mock_ioctl_cb_t mock_ioctl_callback;

/* ── open/openat mock control ── */
extern int mock_open_return;       /* fd to return from open/openat */
extern int mock_open_errno;        /* errno on failure */
extern int mock_open_call_count;

/* ── close mock control ── */
extern int mock_close_return;
extern int mock_close_errno;
extern int mock_close_call_count;

/* ── read mock control ── */
extern const char* mock_read_data;    /* data to copy into buf */
extern ssize_t mock_read_return;      /* return value */
extern int mock_read_errno;

/* ── fstatat mock control ── */
extern int mock_fstatat_return;
extern int mock_fstatat_errno;
extern dev_t mock_fstatat_rdev;       /* st_rdev to populate */

/* ── readlink mock control ── */
extern const char* mock_readlink_data;
extern ssize_t mock_readlink_return;
extern int mock_readlink_errno;

/* ── scandir mock control ── */
extern int mock_scandir_return;       /* number of entries, or -1 */
extern struct dirent** mock_scandir_namelist;

/* ── nanosleep mock ── */
extern int mock_nanosleep_return;

/* ── getenv mock control ── */
extern const char* mock_getenv_return;

/* ── mock activation ── */
/* When mock_active is 0, wrappers delegate to real __real_* functions.
 * This allows gcov's atexit handler to write .gcda files. */
extern int mock_active;

/* Reset all mock state to defaults */
void mock_reset_all(void);

#ifdef __cplusplus
}
#endif
