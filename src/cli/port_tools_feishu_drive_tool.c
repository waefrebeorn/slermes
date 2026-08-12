/*
 * port_tools_feishu_drive_tool.c — C port of tools/feishu_drive_tool.py / tools/feishu_doc_tool.py
 *
 * Note: feishu_doc_tool.py functions (set_client, get_client, _check_feishu)
 * map to this file since the feishu drive tool provides the client infrastructure.
 */

#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Thread-local client storage from feishu_drive_tool */
typedef struct {
    void *client;
} feishu_local_t;

static feishu_local_t _feishu_local = {NULL};

/* PoP: cli_tools_feishu_drive_tool_set_client @ tools/feishu_drive_tool.py:set_client */

/* Port of Python tools/feishu_doc_tool.py:set_client */
/* Store a lark client for the current thread. */
void cli_tools_feishu_drive_tool_set_client(void *client)
{
    _feishu_local.client = client;
    hermes_log(LOG_DEBUG, "feishu", "client set");
}

/* PoP: cli_tools_feishu_drive_tool_get_client @ tools/feishu_drive_tool.py:get_client */

/* Port of Python tools/feishu_doc_tool.py:get_client */
/* Return the lark client for the current thread, or NULL. */
void *cli_tools_feishu_drive_tool_get_client(void)
{
    return _feishu_local.client;
}

/* PoP: cli_tools_feishu_drive_tool__check_feishu @ tools/feishu_drive_tool.py:_check_feishu */

/* Port of Python tools/feishu_doc_tool.py:_check_feishu */
/* Check if the lark_oapi (Feishu SDK) is importable/available. */
int cli_tools_feishu_drive_tool__check_feishu(void)
{
    /* In a full Python implementation, this checks importlib.util.find_spec.
     * In C, we check for the feishu SDK shared library / venv package. */
    /* Try to check for the feishu SDK by looking for the library file */
    const char *feishu_paths[] = {
        "/usr/lib/lark_oapi.so",
        "/usr/local/lib/lark_oapi.so",
        NULL
    };

    for (int i = 0; feishu_paths[i]; i++) {
        FILE *f = fopen(feishu_paths[i], "r");
        if (f) {
            fclose(f);
            return 1; /* SDK found */
        }
    }

    /* Native venv site-packages probe (lark_oapi is a pure-Python wheel):
     * check $VIRTUAL_ENV/lib/pythonX.Y/site-packages/lark_oapi. */
    const char *ve = getenv("VIRTUAL_ENV");
    const char *hp = getenv("HOME");
    const char *pyhome = getenv("PYTHONHOME");
    const char *cfg_py = getenv("HERMES_PYTHON");
    const char *roots[4] = { ve, hp, pyhome, cfg_py };
    for (int r = 0; r < 4 && roots[r]; r++) {
        char p[4096];
        /* venv layout: <root>/lib/pythonX.Y/site-packages/lark_oapi */
        if (ve == roots[r]) {
            snprintf(p, sizeof(p),
                     "%s/lib/python3.11/site-packages/lark_oapi/__init__.py", roots[r]);
            if (access(p, R_OK) == 0) return 1;
            snprintf(p, sizeof(p),
                     "%s/lib/python3.12/site-packages/lark_oapi/__init__.py", roots[r]);
            if (access(p, R_OK) == 0) return 1;
        } else {
            /* user/system layout: <root>/lib/python3.11/site-packages or .local */
            snprintf(p, sizeof(p),
                     "%s/lib/python3.11/site-packages/lark_oapi/__init__.py", roots[r]);
            if (access(p, R_OK) == 0) return 1;
            snprintf(p, sizeof(p),
                     "%s/.local/lib/python3.11/site-packages/lark_oapi/__init__.py",
                     roots[r]);
            if (access(p, R_OK) == 0) return 1;
        }
    }
    return 0; /* SDK not available */
}
