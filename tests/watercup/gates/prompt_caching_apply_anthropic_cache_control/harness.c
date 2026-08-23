/* gate: prompt_caching_apply_anthropic_cache_control — traced copy of
 * src/agent/prompt_caching_plan.c::pca_apply_anthropic_cache_control
 * (LIVE twin: agent/prompt_caching.py::apply_anthropic_cache_control).
 *
 * The pc_* helper implementations below are VERBATIM copies of the real port's
 * statics (they are file-static in prompt_caching_plan.c, so direct linking is
 * not clean); only WCUP rungs are added to the apply body.
 *
 * Branch ladder (must match twin.py EXACTLY, same rung order):
 *   B0: empty/non-array messages            -> returned as-is
 *   V : breakpoints_used
 *   B1: first message role == system
 *   per-message candidate scan: L tick, then
 *     B2: msg is an object
 *     B3: role != system
 *     B4: can carry marker
 *   B5: eligible count < remaining  (start clamped to 0)
 *   per applied marker: L tick
 *   V : encoded result (role + M/- marker presence per message)
 *
 * Input format: <native 0|1>|<prefix or '->'|<messages-json>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "libjson/json.h"
#include "hermes_json.h"
#include "watercup.h"

static const char *int_str(int v);
static void encode_summary(json_t *messages, char *buf, size_t cap);

/* ---- verbatim statics from src/agent/prompt_caching_plan.c ---- */
static json_t *pc_build_marker(const char *ttl)
{
    json_t *m = json_object();
    if (!m) return NULL;
    json_set(m, "type", json_string("ephemeral"));
    if (ttl && strcmp(ttl, "1h") == 0)
        json_set(m, "ttl", json_string("1h"));
    return m;
}
static bool pc_can_carry_marker(const json_t *msg, bool native_anthropic)
{
    if (!msg || msg->type != JSON_OBJECT) return false;
    if (native_anthropic) return true;
    json_t *content = json_obj_get(msg, "content");
    if (!content || content->type == JSON_NULL) return false;
    if (content->type == JSON_STRING && content->str_val && content->str_val[0] == '\0')
        return false;
    if (content->type == JSON_ARRAY) {
        if (content->c.count == 0) return false;
        json_t *last = content->c.items[content->c.count - 1];
        return last && last->type == JSON_OBJECT;
    }
    if (content->type == JSON_STRING) return true;
    return false;
}
static void pc_apply_cache_marker(json_t *msg, json_t *cache_marker,
                                  bool native_anthropic)
{
    if (!msg || msg->type != JSON_OBJECT || !cache_marker) return;
    json_t *role = json_obj_get(msg, "role");
    const char *role_str = (json_is_string(role) && role->str_val) ? role->str_val : "";
    json_t *content = json_obj_get(msg, "content");
    if (strcmp(role_str, "tool") == 0 && native_anthropic) {
        json_set(msg, "cache_control", json_copy(cache_marker));
        return;
    }
    bool content_none_or_empty = (!content || content->type == JSON_NULL ||
                                  (content->type == JSON_STRING &&
                                   (!content->str_val || content->str_val[0] == '\0')));
    if (content_none_or_empty) {
        if (strcmp(role_str, "tool") == 0 && !native_anthropic) return;
        if (strcmp(role_str, "assistant") == 0 && !native_anthropic) return;
        json_set(msg, "cache_control", json_copy(cache_marker));
        return;
    }
    if (content && content->type == JSON_STRING) {
        json_t *part = json_object();
        json_set(part, "type", json_string("text"));
        json_set(part, "text", json_copy(content));
        json_set(part, "cache_control", json_copy(cache_marker));
        json_t *arr = json_array();
        json_array_append(arr, part);
        json_set(msg, "content", arr);
        return;
    }
    if (content && content->type == JSON_ARRAY && content->c.count > 0) {
        json_t *last = content->c.items[content->c.count - 1];
        if (last && last->type == JSON_OBJECT)
            json_set(last, "cache_control", json_copy(cache_marker));
    }
}
static int pc_apply_system_cache_markers(json_t *msg, json_t *marker,
                                         const char *static_system_prefix,
                                         bool native_anthropic,
                                         bool mark_suffix,
                                         bool fallback_to_whole)
{
    if (!msg || msg->type != JSON_OBJECT) return 0;
    json_t *content = json_obj_get(msg, "content");
    if (content && content->type == JSON_STRING && content->str_val &&
        static_system_prefix && static_system_prefix[0]) {
        size_t plen = strlen(static_system_prefix);
        if (strncmp(content->str_val, static_system_prefix, plen) == 0) {
            const char *suffix = content->str_val + plen;
            if (suffix[0] != '\0') {
                json_t *parts = json_array();
                json_t *p1 = json_object();
                json_set(p1, "type", json_string("text"));
                json_set(p1, "text", json_string(static_system_prefix));
                json_set(p1, "cache_control", json_copy(marker));
                json_array_append(parts, p1);
                json_t *p2 = json_object();
                json_set(p2, "type", json_string("text"));
                json_set(p2, "text", json_string(suffix));
                if (mark_suffix)
                    json_set(p2, "cache_control", json_copy(marker));
                json_array_append(parts, p2);
                json_set(msg, "content", parts);
                return mark_suffix ? 2 : 1;
            }
        }
    }
    if (!fallback_to_whole) return 0;
    pc_apply_cache_marker(msg, marker, native_anthropic);
    return 1;
}

/* ---- traced apply body (faithful copy of pca_apply_anthropic_cache_control) ---- */
static void wc_apply(json_t *api_messages, const char *cache_ttl,
                     bool native_anthropic, const char *static_system_prefix) {
    WCUP_BEGIN("pca_apply_anthropic_cache_control");
    int emitted = 0;
    if (!api_messages || api_messages->type != JSON_ARRAY ||
        api_messages->c.count == 0) {
        WCUP_BRANCH(1);                                          /* B0 */
        goto out;
    }
    WCUP_BRANCH(0);
    emitted = 1;
    {
        json_t *messages = json_array();
        size_t i;
        int breakpoints_used = 0;
        for (i = 0; i < api_messages->c.count; i++)
            json_array_append(messages, json_copy(api_messages->c.items[i]));
        json_t *marker = pc_build_marker(cache_ttl);

        {
            json_t *m0 = messages->c.items[0];
            json_t *r0 = m0 && m0->type == JSON_OBJECT ? json_obj_get(m0, "role") : NULL;
            if (json_is_string(r0) && r0->str_val && strcmp(r0->str_val, "system") == 0) {
                WCUP_BRANCH(1);                                  /* B1 */
                breakpoints_used = pc_apply_system_cache_markers(
                    m0, marker, static_system_prefix, native_anthropic,
                    /*mark_suffix=*/true, /*fallback_to_whole=*/true);
            } else {
                WCUP_BRANCH(0);                                  /* B1 */
            }
        }
        WCUP_VALSTR(int_str(breakpoints_used));

        {
            int remaining = 4 - breakpoints_used;
            int *non_sys = malloc(sizeof(int) * messages->c.count);
            size_t ns_count = 0;
            for (i = 0; i < messages->c.count; i++) {
                json_t *msg = messages->c.items[i];
                WCUP_LOOP_TICK();
                if (!msg || msg->type != JSON_OBJECT) {
                    WCUP_BRANCH(0);                              /* B2 */
                    continue;
                }
                WCUP_BRANCH(1);                                  /* B2 */
                {
                    json_t *role = json_obj_get(msg, "role");
                    int is_non_sys = json_is_string(role) && role->str_val &&
                                     strcmp(role->str_val, "system") != 0;
                    WCUP_BRANCH(is_non_sys);                     /* B3 */
                    if (is_non_sys) {
                        int can = pc_can_carry_marker(msg, native_anthropic);
                        WCUP_BRANCH(can);                        /* B4 */
                        if (can) non_sys[ns_count++] = (int)i;
                    }
                }
            }
            {
                int start = (int)ns_count - remaining;
                int s;
                WCUP_BRANCH(start < 0 ? 1 : 0);                  /* B5 */
                if (start < 0) start = 0;
                for (s = start; s < (int)ns_count; s++) {
                    WCUP_LOOP_TICK();
                    pc_apply_cache_marker(messages->c.items[non_sys[s]], marker,
                                          native_anthropic);
                }
            }
            free(non_sys);
        }
        {
            char enc[512];
            encode_summary(messages, enc, sizeof enc);
            WCUP_VALSTR(enc);
        }
        json_free(messages);
        json_free(marker);
    }
out:
    if (!emitted) WCUP_VALSTR("");
    {
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("pca_apply_anthropic_cache_control");
    }
}

static const char *int_str(int v) { static char b[16]; snprintf(b, sizeof b, "%d", v); return b; }

static void encode_summary(json_t *messages, char *buf, size_t cap) {
    size_t n = messages->c.count, k = 0;
    buf[0] = '\0';
    for (size_t i = 0; i < n && k + 24 < cap; i++) {
        json_t *msg = messages->c.items[i];
        const char *role = "?";
        int marked = 0;
        if (msg && msg->type == JSON_OBJECT) {
            json_t *r = json_obj_get(msg, "role");
            if (json_is_string(r) && r->str_val) role = r->str_val;
            if (json_obj_get(msg, "cache_control")) marked = 1;
            else {
                json_t *content = json_obj_get(msg, "content");
                if (content && content->type == JSON_ARRAY) {
                    for (size_t j = 0; j < content->c.count; j++) {
                        json_t *part = content->c.items[j];
                        if (part && part->type == JSON_OBJECT &&
                            json_obj_get(part, "cache_control")) { marked = 1; break; }
                    }
                }
            }
        }
        k += snprintf(buf + k, cap - k, "%s%s:%c", i ? "|" : "", role,
                      marked ? 'M' : '-');
    }
}

int main(int argc, char **argv) {
    int i;
    if (argc < 3) {
        fprintf(stderr, "usage: %s pca_apply_anthropic_cache_control <native|prefix|msgs-json> ...\n", argv[0]);
        return 2;
    }
    for (i = 2; i < argc; i++) {
        char *arg = strdup(argv[i]);
        char *p1 = strchr(arg, '|');
        char *p2 = NULL;
        if (p1) { *p1 = '\0'; p2 = strchr(p1 + 1, '|'); }
        bool native = arg[0] == '1';
        char *prefix = NULL;
        char *jtxt = "{}";
        if (p2) {
            *p2 = '\0';
            prefix = p1 + 1;
            jtxt = p2 + 1;
            if (strcmp(prefix, "-") == 0) prefix = NULL;
        }
        char *jerr = NULL;
        json_t *msgs = json_parse(jtxt, &jerr);
        wc_apply(msgs, "5m", native, prefix);
        if (msgs) json_free(msgs);
        free(arg);
    }
    return 0;
}
