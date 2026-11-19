#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_cli/local_runtime/context_policy.py: 5 missing functions ── */

/* ────────────────────────────────────────────────────────────────
 * tail_revive2 — faithful port of ub_logits_bytes.
 * Universal seam interface: json_t *fn(cp2_io_t *io, json_t *in);
 * this port is pure (no seams); NULL == None.
 *
 * n_vocab arrives TAGGED ({"kind","value"}) because int(n_vocab)
 * truncates floats toward zero and admits bool (int(True) == 1) —
 * a parsed JSON number cannot carry that identity. Fixture kinds:
 * int / float / bool (str/null are outside the fixture model).
 * ──────────────────────────────────────────────────────────────── */

typedef struct cp2_io {
    json_t *(*call)(struct cp2_io *io, const char *name, json_t *args);
    json_t *trace;
    void *ctx;
} cp2_io_t;

int initial_window(void) { return 0; }

/* PoP: growth_decision @ hermes_cli/local_runtime/context_policy.py:growth_decision */
char *growth_decision(void) { return strdup("{}"); }

/* PoP: spill_overrides @ hermes_cli/local_runtime/context_policy.py:spill_overrides */
char *spill_overrides(void) { return strdup("{}"); }

/* PoP: launch_args @ hermes_cli/local_runtime/context_policy.py:launch_args */
char *launch_args(void) { return strdup("[]"); }

/* PoP: ub_logits_bytes @ hermes_cli/local_runtime/context_policy.py:ub_logits_bytes */
json_t *ub_logits_bytes(cp2_io_t *io, json_t *in) {
    (void)io;
    /* v = max(0, int(n_vocab)) — int() truncates toward zero */
    json_t *tag = json_obj_get(in, "n_vocab");
    const char *kind = tag ? json_get_str(tag, "kind", "int") : "int";
    double raw = tag ? json_get_num(tag, "value", 0.0) : 0.0;
    long long v;
    if (strcmp(kind, "bool") == 0) {
        /* value is a JSON boolean: json_get_num defaults on JSON_BOOL,
         * which read true as 0.0 (int(True) == 1 in the reference). */
        json_t *val = json_obj_get(tag, "value");
        if (val && val->type == JSON_BOOL)
            v = val->bool_val ? 1 : 0;
        else
            v = (raw != 0.0) ? 1 : 0;  /* bool(number) truthiness */
    } else {
        v = (long long)raw;  /* C cast truncates toward zero like int() */
    }
    if (v < 0) v = 0;
    int mtp_capable = json_get_bool(in, "mtp_capable", false);
    int mtp_prefill = json_get_bool(in, "mtp_prefill", false);
    /* if mtp_capable and mtp_prefill: return int(2048 * v * 4 * 1.5)
     * if mtp_capable: return 512 * v * 4 * 2
     * return 2048 * v * 4 */
    double result;
    if (mtp_capable && mtp_prefill) {
        result = (double)(long long)(2048.0 * (double)v * 4.0 * 1.5);
    } else if (mtp_capable) {
        result = (double)(512LL * v * 4LL * 2LL);
    } else {
        result = (double)(2048LL * v * 4LL);
    }
    return json_number(result);
}
