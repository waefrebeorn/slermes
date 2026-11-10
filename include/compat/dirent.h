/* Slermes Windows compat: <dirent.h>
 * MinGW provides opendir/readdir but its struct dirent has no d_type. This
 * header forwards to the real one and supplies only the DT_* constants, so a
 * stray DT_DIR still compiles; membership tests must use
 * hermes_dirent_is_dir() (hermes_win32.h) because the type is not knowable
 * without a stat(). */
#ifndef SLERMES_COMPAT_DIRENT_H
#define SLERMES_COMPAT_DIRENT_H

#include_next <dirent.h>

#ifndef DT_UNKNOWN
#define DT_UNKNOWN 0
#define DT_FIFO    1
#define DT_CHR     2
#define DT_DIR     4
#define DT_BLK     6
#define DT_REG     8
#define DT_LNK    10
#define DT_SOCK   12
#endif

#endif /* SLERMES_COMPAT_DIRENT_H */
