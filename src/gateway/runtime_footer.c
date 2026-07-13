/*
 * runtime_footer.c — extracted from gateway/helpers.c monolith.
 *
 * Real implementation home for the Python module it ports (no longer a
 * name-parity stub). Public prototypes stay in include/gateway_helpers.h
 * (or hermes_gateway.h); callers are unchanged.
 */

#include "gateway_helpers.h"
#include "hermes_json.h"
#include "hermes_gateway.h"
#include "hermes_system_prompt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <pthread.h>

/* ================================================================
 *  Runtime footer — compact model/context status line
 *  Port of Python gateway/runtime_footer.py.
 * ================================================================ */

/* Collapse $HOME to ~ in a path.
 * Port of Python gateway/runtime_footer.py _home_relative_cwd().
 * AG26: Port of Python gateway/runtime_footer.py:_home_relative_cwd().
 */
char *home_relative_cwd(const char *cwd) {
    if (!cwd || !*cwd) return strdup("");
    const char *home = getenv("HOME");
    if (!home) return strdup(cwd);
    size_t home_len = strlen(home);
    if (strncmp(cwd, home, home_len) == 0) {
        const char *suffix = cwd + home_len;
        if (*suffix == '/' || *suffix == '\0') {
            char buf[1024];
            snprintf(buf, sizeof(buf), "~%s", suffix);
            return strdup(buf);
        }
    }
    return strdup(cwd);
}


/* Drop vendor/ prefix from model name.
 * Port of Python gateway/runtime_footer.py _model_short().
 * AG26: Port of Python gateway/runtime_footer.py:_model_short().
 */
char *model_short(const char *model) {
    if (!model || !*model) return strdup("");
    const char *slash = strrchr(model, '/');
    if (slash) return strdup(slash + 1);
    return strdup(model);
}

