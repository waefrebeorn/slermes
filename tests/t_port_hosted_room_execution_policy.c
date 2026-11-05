/*
 * t_port_hosted_room_execution_policy.c — Oracle harness for
 * gateway/hosted_room_execution_policy.py (validation + digest half).
 */
#include "port_hosted_room_execution_policy.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void emit_json_str(const char *s)
{
    if (!s) { fputs("null", stdout); return; }
    fputc('"', stdout);
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        switch (*p) {
        case '"':  fputs("\\\"", stdout); break;
        case '\\': fputs("\\\\", stdout); break;
        case '\n': fputs("\\n", stdout); break;
        case '\r': fputs("\\r", stdout); break;
        case '\t': fputs("\\t", stdout); break;
        default:
            if (*p < 0x20) printf("\\u%04x", *p);
            else fputc((char)*p, stdout);
        }
    }
    fputc('"', stdout);
}

static void emit_digest(const char *name, const char *profile,
                        const char *const *toolsets, int n,
                        const char *mode, long long iters)
{
    char *d = hep_policy_digest(profile, toolsets, n, mode, iters);
    printf("{\"family\":\"digest\",\"case\":\"%s\",\"profile\":", name);
    emit_json_str(profile);
    printf(",\"toolsets\":[");
    for (int i = 0; i < n; i++) {
        if (i) fputs(",", stdout);
        emit_json_str(toolsets[i]);
    }
    printf("],\"mode\":");
    emit_json_str(mode);
    printf(",\"iters\":%lld,\"digest\":", iters);
    emit_json_str(d);
    printf("}\n");
    free(d);
}

static void emit_validate(const char *name, const char *doc)
{
    hep_policy_t p;
    char *err = NULL;
    int rc = hep_policy_from_mapping(doc, &p, &err);
    printf("{\"family\":\"validate\",\"case\":\"%s\",\"doc\":", name);
    emit_json_str(doc);
    printf(",\"ok\":%d,\"err\":", rc == 0 ? 1 : 0);
    emit_json_str(rc == 0 ? NULL : (err ? err : "?"));
    printf(",\"mapping\":");
    if (rc == 0) {
        json_t *m = hep_policy_as_mapping(&p);
        char *s = m ? json_serialize(m) : NULL;
        fputs(s ? s : "null", stdout);
        free(s);
        if (m) json_free(m);
        hep_policy_free(&p);
    } else {
        fputs("null", stdout);
    }
    printf("}\n");
    free(err);
}

/* Build a policy document with a correct digest for (profile, toolsets...). */
static char *make_doc(const char *profile, const char *const *toolsets, int n,
                      const char *mode, long long iters,
                      const char *digest_override, const char *extra)
{
    char *digest = digest_override ? strdup(digest_override)
                                   : hep_policy_digest(profile, toolsets, n, mode, iters);
    size_t cap = 512 + (digest ? strlen(digest) : 0);
    for (int i = 0; i < n; i++) cap += strlen(toolsets[i]) + 8;
    if (extra) cap += strlen(extra) + 8;
    char *doc = (char *)malloc(cap);
    size_t o = 0;
    o += (size_t)snprintf(doc + o, cap - o,
        "{\"version\":1,\"target_profile\":\"%s\",\"enabled_toolsets\":[",
        profile);
    for (int i = 0; i < n; i++)
        o += (size_t)snprintf(doc + o, cap - o, "%s\"%s\"", i ? "," : "", toolsets[i]);
    o += (size_t)snprintf(doc + o, cap - o,
        "],\"approval_mode\":\"%s\",\"max_iterations\":%lld,\"policy_digest\":\"%s\"%s}",
        mode, iters, digest ? digest : "", extra ? extra : "");
    free(digest);
    return doc;
}

int main(void)
{
    static const char *TS_BASIC[] = { "bot_room", "web" };
    static const char *TS_UNSORTED[] = { "web", "bot_room", "files" };
    static const char *TS_SORTED3[] = { "bot_room", "files", "web" };
    static const char *TS_NO_ROOM[] = { "web", "files" };
    static const char *TS_DUP[] = { "bot_room", "bot_room" };
    static const char *TS_ONE[] = { "bot_room" };

    setvbuf(stdout, NULL, _IONBF, 0);

    emit_digest("digest_basic", "room1", TS_BASIC, 2, "manual", 100);
    /* hep_policy_digest takes an already-sorted list, like Python's unsigned dict. */
    emit_digest("digest_three_toolsets", "room1", TS_SORTED3, 3, "smart", 9007199254740991LL);
    emit_digest("digest_off", "p-2", TS_ONE, 1, "off", 1);

    char *doc;

    doc = make_doc("room1", TS_BASIC, 2, "manual", 100, NULL, NULL);
    emit_validate("valid_basic", doc); free(doc);

    doc = make_doc("room1", TS_UNSORTED, 3, "smart", 42, NULL, NULL);
    emit_validate("valid_sorted_output", doc); free(doc);

    doc = make_doc("room1", TS_BASIC, 2, "MANUAL ", 100, NULL, NULL);
    emit_validate("valid_mode_normalised", doc); free(doc);

    doc = make_doc("room1", TS_ONE, 1, "off", 9007199254740991LL, NULL, NULL);
    emit_validate("valid_max_iters", doc); free(doc);

    doc = make_doc("bad profile", TS_BASIC, 2, "manual", 100, NULL, NULL);
    emit_validate("bad_profile", doc); free(doc);

    doc = make_doc("-leading", TS_BASIC, 2, "manual", 100, NULL, NULL);
    emit_validate("bad_profile_leading", doc); free(doc);

    doc = make_doc("room1", TS_BASIC, 2, "manual", 0, NULL, NULL);
    emit_validate("iters_zero", doc); free(doc);

    doc = make_doc("room1", TS_BASIC, 2, "manual", -5, NULL, NULL);
    emit_validate("iters_negative", doc); free(doc);

    doc = make_doc("room1", TS_BASIC, 2, "manual", 9007199254740992LL, NULL, NULL);
    emit_validate("iters_too_big", doc); free(doc);

    doc = make_doc("room1", TS_NO_ROOM, 2, "manual", 100, NULL, NULL);
    emit_validate("no_bot_room", doc); free(doc);

    doc = make_doc("room1", TS_DUP, 2, "manual", 100, NULL, NULL);
    emit_validate("duplicate_toolsets", doc); free(doc);

    doc = make_doc("room1", TS_BASIC, 2, "yolo", 100, NULL, NULL);
    emit_validate("bad_mode", doc); free(doc);

    doc = make_doc("room1", TS_BASIC, 2, "manual", 100,
                   "0000000000000000000000000000000000000000000000000000000000000000",
                   NULL);
    emit_validate("digest_mismatch", doc); free(doc);

    {
        char *digest = hep_policy_digest("room1", TS_BASIC, 2, "manual", 100);
        char upper[128];
        snprintf(upper, sizeof(upper), "%s", digest ? digest : "");
        for (char *p = upper; *p; p++)
            if (*p >= 'a' && *p <= 'f') *p = (char)(*p - 'a' + 'A');
        doc = make_doc("room1", TS_BASIC, 2, "manual", 100, upper, NULL);
        emit_validate("digest_uppercase_ok", doc); free(doc);
        free(digest);
    }

    /* field-set failures */
    emit_validate("missing_field",
        "{\"version\":1,\"target_profile\":\"room1\",\"enabled_toolsets\":[\"bot_room\"],\"approval_mode\":\"manual\",\"max_iterations\":10}");
    emit_validate("extra_field",
        "{\"version\":1,\"target_profile\":\"room1\",\"enabled_toolsets\":[\"bot_room\"],\"approval_mode\":\"manual\",\"max_iterations\":10,\"policy_digest\":\"x\",\"surprise\":1}");
    emit_validate("version_two",
        "{\"version\":2,\"target_profile\":\"room1\",\"enabled_toolsets\":[\"bot_room\"],\"approval_mode\":\"manual\",\"max_iterations\":10,\"policy_digest\":\"x\"}");
    emit_validate("empty_toolsets",
        "{\"version\":1,\"target_profile\":\"room1\",\"enabled_toolsets\":[],\"approval_mode\":\"manual\",\"max_iterations\":10,\"policy_digest\":\"x\"}");
    emit_validate("not_an_object", "[]");
    return 0;
}
