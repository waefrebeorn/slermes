/*
 * port_tools_feishu_drive_tool.c — C port of tools/feishu_drive_tool.py / tools/feishu_doc_tool.py
 *
 * Note: feishu_doc_tool.py functions (set_client, get_client, _check_feishu)
 * map to this file since the feishu drive tool provides the client infrastructure.
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    /* In a full Python implementation, this checks importlib.util.find_spec("lark_oapi").
     * In C, we check if the feishu SDK shared library is available. */
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

    /* Also check via Python import (fallback) */
    int ret = system("python3 -c \"import importlib.util; exit(0 if importlib.util.find_spec('lark_oapi') else 1)\" 2>/dev/null");
    if (ret == 0) return 1;

    return 0; /* SDK not available */
}
