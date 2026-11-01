/* Slermes Windows compat: <regex.h> — MinGW ships none; libtre provides the
 * POSIX regcomp/regexec/regfree/REG_* surface. Link with -ltre. */
#ifndef SLERMES_COMPAT_REGEX_H
#define SLERMES_COMPAT_REGEX_H

#include <tre/regex.h>

#endif /* SLERMES_COMPAT_REGEX_H */
