/*
 * port_api_store_error.c — pure helper (see header).
 */

#include "port_api_store_error.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *asr_error_status(const char *exc_reason, int is_runtime_store_error)
{
    const char *reason = is_runtime_store_error
        ? (exc_reason ? exc_reason : "") : "storage_unavailable";
    int status = 409, queued = 0, retry_after_1 = 0, should_retry_false = 0;

    if (strcmp(reason, "admission_queued") == 0) {
        status = 503;
        queued = 1;
        should_retry_false = 1;
    } else {
        if (strcmp(reason, "invalid_params") == 0) status = 400;
        else if (strcmp(reason, "runtime_draining") == 0) status = 503;
        else if (strcmp(reason, "storage_unavailable") == 0) status = 503;
        else if (strcmp(reason, "chain_moved") == 0) status = 503;
        if (status == 503) retry_after_1 = 1;
    }

    {
        char buf[256];
        snprintf(buf, sizeof(buf),
                 "{\"reason\":\"%s\",\"status\":%d,\"queued\":%s,"
                 "\"x_should_retry_false\":%s,\"retry_after_1\":%s}",
                 reason, status,
                 queued ? "true" : "false",
                 should_retry_false ? "true" : "false",
                 retry_after_1 ? "true" : "false");
        return strdup(buf);
    }
}
