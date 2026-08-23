/* gate: error_surface_build_error_surface_from_result — faithful traced copy of
 * src/agent/error_surface.c::es_build_error_surface_from_result
 * (LIVE twin: agent/error_surface.py::build_error_surface_from_result).
 *
 * Input: JSON object string (provider/model fixed to "" on both sides).
 *
 * Branch ladder (must match twin.py EXACTLY):
 *   B1 result not an object            -> V("none")
 *   V(error_text), V(reason)
 *   B2 no error text AND no reason     -> V("none")
 *   B3 disk-full marker in error text  -> V(surface)
 *   B4 billing_block truthy OR reason billing/billing_unverified -> V(surface)
 *   B5 reason empty -> { B5a stream-drop fragment -> V(surface)
 *                       B5b else provider/unknown -> V(surface) }
 *   B6 reason->layer map hit?
 *   B7 (miss) transport reason AND custom endpoint -> endpoint layer
 *   B8 (miss) stream drop -> streaming layer, else provider layer
 *   B9 failure_retryable bool present?
 *   V(surface "layer|code|retryable")
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include "watercup.h"
#include "json.h"

static const char *const REASON_TO_LAYER[][2] = {
    {"auth", "auth"}, {"auth_permanent", "auth"},
    {"billing", "billing"}, {"billing_unverified", "billing"},
};
static const char *const TRANSPORT_REASONS[] = {"timeout", "ssl_cert_verification"};
static const char *const NON_RETRYABLE_REASONS[] = {
    "auth", "auth_permanent", "billing", "billing_unverified",
    "content_policy_blocked", "provider_policy_blocked", "model_not_found",
    "format_error", "ssl_cert_verification",
};
static const char *const CUSTOM_ENDPOINT_PROVIDERS[] = {
    "custom", "local", "llama.cpp", "llamacpp", "ollama", "lmstudio", "vllm",
};
static const char *const STREAM_DROP_FRAGMENTS[] = {
    "stream connection", "peer closed connection", "incomplete chunked read",
    "connection broken", "stream ended prematurely", "sse", "mid-stream",
};
static const char *const DISK_FULL_MARKERS[] = {
    "no space left on device", "not enough space", "database or disk is full",
    "disk full", "full disk", "enospc",
};

static bool in_set(const char *s, const char *const *set, size_t n) {
    if (!s) return false;
    for (size_t i = 0; i < n; i++)
        if (strcmp(s, set[i]) == 0) return true;
    return false;
}

static bool stream_drop(const char *msg) {
    if (!msg) return false;
    char lo[1024];
    snprintf(lo, sizeof lo, "%s", msg);
    for (char *p = lo; *p; p++) *p = (char)tolower((unsigned char)*p);
    for (size_t i = 0; i < sizeof(STREAM_DROP_FRAGMENTS)/sizeof(char*); i++)
        if (strstr(lo, STREAM_DROP_FRAGMENTS[i])) return true;
    return false;
}

static bool disk_full(const char *err) {
    if (!err || !*err) return false;
    char lo[1024];
    snprintf(lo, sizeof lo, "%s", err);
    for (char *p = lo; *p; p++) *p = (char)tolower((unsigned char)*p);
    for (size_t i = 0; i < sizeof(DISK_FULL_MARKERS)/sizeof(char*); i++)
        if (strstr(lo, DISK_FULL_MARKERS[i])) return true;
    return false;
}

/* provider fixed "" for this gate; mirror only the custom-endpoint check */
static bool is_custom_endpoint(const char *provider) {
    if (!provider || !*provider) return false;
    char lo[128];
    snprintf(lo, sizeof lo, "%s", provider);
    for (char *p = lo; *p; p++) *p = (char)tolower((unsigned char)*p);
    if (in_set(lo, CUSTOM_ENDPOINT_PROVIDERS,
               sizeof(CUSTOM_ENDPOINT_PROVIDERS)/sizeof(char*))) return true;
    return strncmp(lo, "custom:", 7) == 0;
}

static void emit_surface(wcup_t *wc, const char *layer, const char *code, bool retryable) {
    char buf[512];
    snprintf(buf, sizeof buf, "%s|%s|%s", layer, code, retryable ? "True" : "False");
    wcup_valstr(wc, buf);
}

static void wc_trace_one(const char *result_json) {
    WCUP_BEGIN("build_error_surface_from_result");
    json_t *result = json_parse(result_json ? result_json : "", NULL);
    if (!result || result->type != JSON_OBJECT) {          /* B1 */
        WCUP_BRANCH(1);
        WCUP_VALSTR("none");
        goto out;
    }
    WCUP_BRANCH(0);
    {
        json_t *jerr = json_obj_get(result, "error");
        const char *error_text = (jerr && jerr->type == JSON_STRING && jerr->str_val)
                                 ? jerr->str_val : "";
        json_t *jreason = json_obj_get(result, "failure_reason");
        char reason[256] = "";
        if (jreason && jreason->type == JSON_STRING && jreason->str_val) {
            const char *r = jreason->str_val;
            size_t a = 0;
            while (r[a] == ' ' || r[a] == '\t' || r[a] == '\n' || r[a] == '\r') a++;
            size_t o = 0;
            for (; r[a] && o + 1 < sizeof reason; a++)
                reason[o++] = r[a];
            while (o > 0 && (reason[o-1]==' '||reason[o-1]=='\t'||
                             reason[o-1]=='\n'||reason[o-1]=='\r')) o--;
            reason[o] = '\0';
        }
        WCUP_VALSTR(error_text);
        WCUP_VALSTR(reason);

        if (!error_text[0] && !reason[0]) {                 /* B2 */
            WCUP_BRANCH(1);
            WCUP_VALSTR("none");
            goto out;
        }
        WCUP_BRANCH(0);

        if (error_text[0] && disk_full(error_text)) {       /* B3 */
            WCUP_BRANCH(1);
            emit_surface(&_wc, "disk", "disk_full", false);
            goto out;
        }
        WCUP_BRANCH(0);

        json_t *jbill = json_obj_get(result, "billing_block");
        bool billing_block = jbill && !(jbill->type == JSON_NULL) &&
                             !(jbill->type == JSON_BOOL && jbill->bool_val == 0);
        if (billing_block || strcmp(reason, "billing") == 0 ||
            strcmp(reason, "billing_unverified") == 0) {    /* B4 */
            WCUP_BRANCH(1);
            emit_surface(&_wc, "billing", reason[0] ? reason : "billing", false);
            goto out;
        }
        WCUP_BRANCH(0);

        if (!reason[0]) {                                   /* B5 */
            WCUP_BRANCH(1);
            if (stream_drop(error_text)) {                  /* B5a */
                WCUP_BRANCH(1);
                emit_surface(&_wc, "streaming", "stream_drop", true);
            } else {
                WCUP_BRANCH(0);
                emit_surface(&_wc, "provider", "unknown", true);
            }
            goto out;
        }
        WCUP_BRANCH(0);

        const char *layer = NULL;
        for (size_t i = 0; i < sizeof(REASON_TO_LAYER)/sizeof(REASON_TO_LAYER[0]); i++)
            if (strcmp(reason, REASON_TO_LAYER[i][0]) == 0) { layer = REASON_TO_LAYER[i][1]; break; }
        bool mapped = layer != NULL;
        WCUP_BRANCH(mapped);                                /* B6 */
        if (!mapped) {
            if (in_set(reason, TRANSPORT_REASONS, 2) && is_custom_endpoint("")) { /* B7 */
                WCUP_BRANCH(1);
                layer = "endpoint";
            } else {
                WCUP_BRANCH(0);
                if (stream_drop(error_text)) {              /* B8 */
                    WCUP_BRANCH(1);
                    layer = "streaming";
                } else {
                    WCUP_BRANCH(0);
                    layer = "provider";
                }
            }
        }

        json_t *jretry = json_obj_get(result, "failure_retryable");
        bool has_retry_bool = jretry && jretry->type == JSON_BOOL;
        WCUP_BRANCH(has_retry_bool);                        /* B9 */
        bool retryable = has_retry_bool ? jretry->bool_val != 0
                         : !in_set(reason, NON_RETRYABLE_REASONS,
                                   sizeof(NON_RETRYABLE_REASONS)/sizeof(char*));
        emit_surface(&_wc, layer, reason, retryable);
    }
out:
    {
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("build_error_surface_from_result");
    }
    if (result) json_free(result);
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s wc_es <result-json>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++)
        wc_trace_one(argv[i]);
    return 0;
}
