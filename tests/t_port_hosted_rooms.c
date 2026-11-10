/*
 * t_port_hosted_rooms.c -- oracle harness for port_hosted_rooms.c
 */
#include "port_hosted_rooms.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void jstr(const char *s)
{
    if (!s) { fputs("null", stdout); return; }
    putchar('"');
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        if (*p == '"' || *p == '\\') { putchar('\\'); putchar(*p); }
        else putchar(*p);
    }
    putchar('"');
}

static void emit_bounded(const char *label, int kind, long long val,
                         const char *msg, long long low, int has_high, long long high)
{
    long long out = 0; char *err = NULL;
    int rc = hr_bounded_int(kind, val, msg, low, has_high, high, &out, &err);
    printf("{\"fn\":\"bounded_int\",\"case\":");
    printf("\"%s\"", label);
    printf(",\"rc\":%d,\"out\":%lld,\"err\":", rc, out);
    jstr(err);
    printf("}\n");
    free(err);
}

static void emit_canonical(const char *label, const char *json_text, int max_bytes)
{
    json_t *v = json_text ? json_parse(json_text, NULL) : NULL;
    char *out = NULL, *err = NULL;
    int rc = hr_canonical_json(v, "payload", max_bytes, &out, &err);
    printf("{\"fn\":\"canonical_json\",\"case\":");
    printf("\"%s\"", label);
    printf(",\"rc\":%d,\"out\":", rc);
    jstr(out);
    printf(",\"err\":");
    jstr(err);
    printf("}\n");
    free(out); free(err);
    if (v) json_free(v);
}

static void emit_claims(const char *label, const char *const *claims,
                        const char *const *keys, int nkeys)
{
    int n = 0;
    char **out = hr_claim_values(claims, keys, nkeys, &n);
    printf("{\"fn\":\"_claim_values\",\"case\":");
    printf("\"%s\"", label);
    printf(",\"out\":[");
    for (int i = 0; i < n; i++) { if (i) putchar(','); jstr(out[i]); }
    printf("]}\n");
    for (int i = 0; i < n; i++) free(out[i]);
    free(out);
}

static void emit_scope(const char *label, const char *const *claims)
{
    char *err = NULL;
    char *key = hr_grant_scope_key(claims, &err);
    printf("{\"fn\":\"_room_grant_scope_key\",\"case\":");
    printf("\"%s\"", label);
    printf(",\"out\":");
    jstr(key);
    printf(",\"err\":");
    jstr(err);
    printf("}\n");
    free(key); free(err);
}

static void emit_superseded(const char *label, long long row_epoch,
                            const char *row_gw, const char *gw, long long epoch)
{
    int v = hr_reservation_superseded(row_epoch, row_gw, gw, epoch);
    printf("{\"fn\":\"_reservation_superseded\",\"case\":");
    printf("\"%s\"", label);
    printf(",\"out\":%s}\n", v ? "true" : "false");
}

static void emit_identity(const char *label, const char *const *record,
                          const char *const *cols, int ncols)
{
    int n = 0; char *err = NULL;
    char **out = hr_remote_run_identity(record, cols, ncols, &n, &err);
    printf("{\"fn\":\"_remote_run_identity\",\"case\":");
    printf("\"%s\"", label);
    printf(",\"out\":[");
    for (int i = 0; i < n; i++) { if (i) putchar(','); jstr(out[i]); }
    printf("],\"err\":");
    jstr(err);
    printf("}\n");
    for (int i = 0; i < n; i++) free(out[i]);
    free(out); free(err);
}

static void emit_members(const char *label, const char *const *member_json, int n)
{
    json_t **members = (json_t **)calloc((size_t)(n > 0 ? n : 1), sizeof(json_t *));
    for (int i = 0; i < n; i++) members[i] = member_json[i] ? json_parse(member_json[i], NULL) : NULL;
    char *out = NULL, *err = NULL;
    int rc = hr_validate_members(members, n, &out, &err);
    printf("{\"fn\":\"_validate_members\",\"case\":");
    printf("\"%s\"", label);
    printf(",\"rc\":%d,\"out\":", rc);
    jstr(out);
    printf(",\"err\":");
    jstr(err);
    printf("}\n");
    free(out); free(err);
    for (int i = 0; i < n; i++) if (members[i]) json_free(members[i]);
    free(members);
}

static void emit_user_event(const char *label, const char *client_id)
{
    char *err = NULL;
    char *id = hr_user_event_id(client_id, &err);
    printf("{\"fn\":\"user_event_id\",\"case\":");
    printf("\"%s\"", label);
    printf(",\"out\":");
    jstr(id);
    printf(",\"err\":");
    jstr(err);
    printf("}\n");
    free(id); free(err);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    /* ---- bounded_int ---- */
    emit_bounded("ok",        1, 5,   "bad", 0, 0, 0);
    emit_bounded("zero",      1, 0,   "bad", 0, 0, 0);
    emit_bounded("negative",  1, -1,  "bad", 0, 0, 0);
    emit_bounded("high-ok",   1, 10,  "bad", 0, 1, 10);
    emit_bounded("high-over", 1, 11,  "bad", 0, 1, 10);
    emit_bounded("bool",      3, 1,   "bad", 0, 0, 0);
    emit_bounded("not-int",   0, 0,   "bad", 0, 0, 0);
    emit_bounded("positive",  1, 1,   "must be positive", 1, 0, 0);
    emit_bounded("msg",       1, 99,  "limit must be between 1 and 10", 1, 1, 10);

    /* ---- canonical_json ---- */
    emit_canonical("small",    "{\"b\":1,\"a\":2}", 1024);
    emit_canonical("too-large","{\"a\":\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\"}", 8);
    emit_canonical("exact",    "{\"a\":1}", 7);
    emit_canonical("null",     NULL, 1024);

    /* ---- _claim_values ---- */
    static const char *c1[] = { "room_id", "R1", "authority_epoch", "7", NULL };
    static const char *k1[] = { "room_id", "home_install_id", "authority_gateway_id",
                                "authority_epoch", "member_id", "target_install_id",
                                "target_profile", NULL };
    emit_claims("all", c1, k1, 7);

    static const char *c2[] = { "room_id", "", "member_id", "0", NULL };
    emit_claims("empty-and-zero", c2, k1, 7);

    static const char *c3[] = { NULL };
    emit_claims("no-claims", c3, k1, 7);

    /* ---- _room_grant_scope_key ---- */
    static const char *g1[] = { "room_id", "R1", "home_install_id", "H1",
                                "authority_gateway_id", "G1", "authority_epoch", "7",
                                "member_id", "M1", "target_install_id", "T1",
                                "target_profile", "P1", NULL };
    emit_scope("complete", g1);

    static const char *g2[] = { "room_id", "R1", "home_install_id", "",
                                "authority_gateway_id", "G1", "authority_epoch", "7",
                                "member_id", "M1", "target_install_id", "T1",
                                "target_profile", "P1", NULL };
    emit_scope("incomplete", g2);

    /* ---- _reservation_superseded ---- */
    emit_superseded("newer",       8, "G1", "G1", 7);
    emit_superseded("same-same",   7, "G1", "G1", 7);
    emit_superseded("same-other",  7, "G2", "G1", 7);
    emit_superseded("older",       6, "G1", "G1", 7);
    emit_superseded("same-null",   7, NULL, "G1", 7);

    /* ---- _remote_run_identity ---- */
    static const char *r1[] = { "room_id", "R1", "home_install_id", "H1",
                                 "authority_gateway_id", "G1", "authority_epoch", "7",
                                 "member_id", "M1", "target_install_id", "T1",
                                 "target_profile", "P1", "task_id", "T9",
                                 "execution_generation", "2", NULL };
    /* the module's _REMOTE_RUN_IDENTITY_COLUMNS - the live fn projects all 9 */
    static const char *cols1[] = { "room_id", "home_install_id", "authority_gateway_id",
                                   "authority_epoch", "member_id", "target_install_id",
                                   "target_profile", "task_id", "execution_generation", NULL };
    emit_identity("two", r1, cols1, 9);

    /* same record minus execution_generation: record[column] -> KeyError */
    static const char *r2[] = { "room_id", "R1", "home_install_id", "H1",
                                "authority_gateway_id", "G1", "authority_epoch", "7",
                                "member_id", "M1", "target_install_id", "T1",
                                "target_profile", "P1", "task_id", "T9", NULL };
    emit_identity("with-missing", r2, cols1, 9);

    /* ---- _validate_members ---- */
    static const char *m1[] = { "{\"id\":\"a\"}", "{\"id\":\"b\"}", NULL };
    emit_members("two", m1, 2);

    static const char *m2[] = { "{\"id\":\"a\"}", "[1,2]", NULL };
    emit_members("one-not-object", m2, 2);

    static const char *m3[] = { NULL };
    emit_members("empty", m3, 0);

    /* ---- user_event_id ---- */
    emit_user_event("simple", "evt-123");
    emit_user_event("padded", "  evt-123  ");
    emit_user_event("bad", "has space");

    return 0;
}
