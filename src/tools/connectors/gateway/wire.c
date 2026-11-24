
/*
 * wire.c - real C11 port of upstream Hermes tools/connectors/gateway/wire.py.
 * Only the module's pure helper and its module-level tables are behaviour; every
 * other name in the module is a pydantic wire-shape model with no logic of its own.
 * PoP: w19_wire_cd861d_execute_error_family @ tools/connectors/gateway/wire.py:execute_error_family
 * PoP: w19_wire_cd861d_execute_error_status @ tools/connectors/gateway/wire.py:EXECUTE_ERROR_CODES
 * PoP: w19_wire_cd861d_file_max_bytes @ tools/connectors/gateway/wire.py:CONNECTOR_FILE_MAX_BYTES
 */
#include "hermes_json.h"
#include "noc_w19_wire_cd861d.h"
#include <string.h>

/* Module-level literal constants mirrored from the Python source. */
static const char *const tools_connectors_gateway_wire_CONST_CONNECTORS_PATH = "v1/connectors";

/* EXECUTE_ERROR_CODES: code -> (family, HTTP status). */
static const struct wire_exec_code {
    const char *code;
    const char *family;
    int status;
} WIRE_EXEC_CODES[] = {
    { "TOOL_NOT_ALLOWED",      "product", 403 },
    { "CONNECTION_REQUIRED",   "product", 409 },
    { "CONNECTION_EXPIRED",    "product", 409 },
    { "ALIAS_REQUIRED",        "product", 422 },
    { "ALIAS_UNKNOWN",         "product", 422 },
    { "INVALID_ARGUMENTS",     "product", 422 },
    { "TOOL_NOT_FOUND",        "product", 404 },
    { "PROVIDER_ERROR",        "backend", 502 },
    { "PROVIDER_RATE_LIMITED", "backend", 429 },
    { "PROVIDER_TIMEOUT",      "backend", 504 },
    { "POLICY_UNAVAILABLE",    "backend", 503 },
    { "GATEWAY_ERROR",         "backend", 500 },
    { NULL, NULL, 0 }
};

/* FILE_ERROR_FAMILIES: agent-side staging failures, family only. */
static const struct wire_file_code {
    const char *code;
    const char *family;
} WIRE_FILE_CODES[] = {
    { "FILE_NOT_ALLOWED",   "product" },
    { "FILE_NOT_FOUND",     "product" },
    { "FILE_TOO_LARGE",     "product" },
    { "FILE_UPLOAD_FAILED", "backend" },
    { NULL, NULL }
};

static const struct wire_exec_code *find_exec(const char *code) {
    if (!code) return NULL;
    for (int i = 0; WIRE_EXEC_CODES[i].code; i++)
        if (!strcmp(WIRE_EXEC_CODES[i].code, code)) return &WIRE_EXEC_CODES[i];
    return NULL;
}

const char *w19_wire_cd861d_execute_error_family(const char *code, const char *family) {
    /* if family in ("product", "backend"): return family */
    if (family && (!strcmp(family, "product") || !strcmp(family, "backend")))
        return !strcmp(family, "product") ? "product" : "backend";
    const struct wire_exec_code *known = find_exec(code);
    if (known) return known->family;
    if (code)
        for (int i = 0; WIRE_FILE_CODES[i].code; i++)
            if (!strcmp(WIRE_FILE_CODES[i].code, code)) return WIRE_FILE_CODES[i].family;
    return NULL;  /* known[0], else FILE_ERROR_FAMILIES.get(code) -> None for an unknown code */
}

int w19_wire_cd861d_execute_error_status(const char *code) {
    const struct wire_exec_code *known = find_exec(code);
    return known ? known->status : -1;
}

long long w19_wire_cd861d_file_max_bytes(void) {
    (void)tools_connectors_gateway_wire_CONST_CONNECTORS_PATH;
    return 20LL * 1024LL * 1024LL;
}
