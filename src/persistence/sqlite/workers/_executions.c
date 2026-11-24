/*
 * _executions.c -- real C11 port of the worker registration/adoption authority
 * of upstream Hermes persistence/sqlite/workers/_executions.py: every fence the
 * store applies before a worker row is written (parameter shape, adoption
 * digest, runtime epoch, generation, admission conflicts, idle and unknown
 * admission probes) and the adopt handshake (producer claim, terminal fence,
 * admission-worker same-epoch rule, linked admission transition).
 *
 * PoP: w20_executions_2cbbe5_register @ persistence/sqlite/workers/_executions.py:register_worker_execution
 * PoP: w20_executions_2cbbe5_adopt @ persistence/sqlite/workers/_executions.py:adopt_worker_execution
 *
 * The SQL itself (holder._execute_write, conn.execute and the store's
 * transaction) stays upstream; the fixture supplies the rows each statement
 * would have returned, so the decisions are the same ones.
 */
#include "hermes_json.h"
#include "noc_w20_executions_2cbbe5.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- SHA-256 (FIPS 180-4) for _secret_digest ------------------------------ */

typedef struct {
    unsigned int state[8];
    unsigned long long total;
    unsigned char block[64];
    size_t used;
} ex2_sha256;

static const unsigned int EX2_K[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u,
    0x923f82a4u, 0xab1c5ed5u, 0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
    0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u, 0xe49b69c1u, 0xefbe4786u,
    0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u,
    0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
    0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u, 0xa2bfe8a1u, 0xa81a664bu,
    0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au,
    0x5b9cca4fu, 0x682e6ff3u, 0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
};

static unsigned int ex2_rotr(unsigned int x, unsigned int n) {
    return (x >> n) | (x << (32u - n));
}

static void ex2_sha256_block(ex2_sha256 *ctx, const unsigned char *p) {
    unsigned int w[64], a, b, c, d, e, f, g, h, t1, t2;
    int i;
    for (i = 0; i < 16; i++) {
        w[i] = ((unsigned int)p[i * 4] << 24) | ((unsigned int)p[i * 4 + 1] << 16) |
               ((unsigned int)p[i * 4 + 2] << 8) | (unsigned int)p[i * 4 + 3];
    }
    for (i = 16; i < 64; i++) {
        unsigned int s0 = ex2_rotr(w[i - 15], 7) ^ ex2_rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        unsigned int s1 = ex2_rotr(w[i - 2], 17) ^ ex2_rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    a = ctx->state[0]; b = ctx->state[1]; c = ctx->state[2]; d = ctx->state[3];
    e = ctx->state[4]; f = ctx->state[5]; g = ctx->state[6]; h = ctx->state[7];
    for (i = 0; i < 64; i++) {
        unsigned int s1 = ex2_rotr(e, 6) ^ ex2_rotr(e, 11) ^ ex2_rotr(e, 25);
        unsigned int ch = (e & f) ^ ((~e) & g);
        unsigned int s0 = ex2_rotr(a, 2) ^ ex2_rotr(a, 13) ^ ex2_rotr(a, 22);
        unsigned int maj = (a & b) ^ (a & c) ^ (b & c);
        t1 = h + s1 + ch + EX2_K[i] + w[i];
        t2 = s0 + maj;
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }
    ctx->state[0] += a; ctx->state[1] += b; ctx->state[2] += c; ctx->state[3] += d;
    ctx->state[4] += e; ctx->state[5] += f; ctx->state[6] += g; ctx->state[7] += h;
}

static void ex2_sha256_hex(const unsigned char *data, size_t n, char out[65]) {
    static const char HEX[] = "0123456789abcdef";
    ex2_sha256 ctx;
    unsigned char sum[32];
    int i;

    ctx.state[0] = 0x6a09e667u; ctx.state[1] = 0xbb67ae85u;
    ctx.state[2] = 0x3c6ef372u; ctx.state[3] = 0xa54ff53au;
    ctx.state[4] = 0x510e527fu; ctx.state[5] = 0x9b05688cu;
    ctx.state[6] = 0x1f83d9abu; ctx.state[7] = 0x5be0cd19u;
    ctx.total = 0;
    ctx.used = 0;
    ctx.total += (unsigned long long)n;
    while (n) {
        size_t room = 64 - ctx.used;
        size_t take = n < room ? n : room;
        memcpy(ctx.block + ctx.used, data, take);
        ctx.used += take;
        data += take;
        n -= take;
        if (ctx.used == 64) { ex2_sha256_block(&ctx, ctx.block); ctx.used = 0; }
    }
    {
        unsigned long long bits = ctx.total * 8u;
        ctx.block[ctx.used++] = 0x80;
        if (ctx.used > 56) {
            while (ctx.used < 64) ctx.block[ctx.used++] = 0;
            ex2_sha256_block(&ctx, ctx.block);
            ctx.used = 0;
        }
        while (ctx.used < 56) ctx.block[ctx.used++] = 0;
        for (i = 0; i < 8; i++) ctx.block[56 + i] = (unsigned char)(bits >> ((7 - i) * 8));
        ex2_sha256_block(&ctx, ctx.block);
    }
    for (i = 0; i < 8; i++) {
        sum[i * 4]     = (unsigned char)(ctx.state[i] >> 24);
        sum[i * 4 + 1] = (unsigned char)(ctx.state[i] >> 16);
        sum[i * 4 + 2] = (unsigned char)(ctx.state[i] >> 8);
        sum[i * 4 + 3] = (unsigned char)(ctx.state[i]);
    }
    for (i = 0; i < 32; i++) {
        out[i * 2]     = HEX[sum[i] >> 4];
        out[i * 2 + 1] = HEX[sum[i] & 0x0f];
    }
    out[64] = '\0';
}

/* --- small helpers ------------------------------------------------------- */

static char *ex2_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

static int ex2_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
    case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count != 0 ? 1 : 0;
    }
    return 0;
}

/* `type(x) is int`: a JSON bool is not an int. */
static int ex2_py_int(const json_t *v, long long *out) {
    double d;
    if (!v || v->type != JSON_NUMBER) return 0;
    d = v->num_val;
    if (!isfinite(d) || floor(d) != d) return 0;
    if (d < -9223372036854775808.0 || d >= 9223372036854775808.0) return 0;
    if (out) *out = (long long)d;
    return 1;
}

/* `_text(value)`: a non-empty str no longer than 1024 chars. */
static int ex2_text_ok(const json_t *v) {
    if (!v || v->type != JSON_STRING || !v->str_val || !v->str_val[0]) return 0;
    return strlen(v->str_val) <= 1024;
}

static char *ex2_digest(const json_t *secret) {
    char hex[65];
    if (!ex2_text_ok(secret)) return NULL;
    ex2_sha256_hex((const unsigned char *)secret->str_val, strlen(secret->str_val), hex);
    return ex2_dup(hex);
}

static json_t *ex2_refused(const char *reason) {
    json_t *out = json_object();
    json_set(out, "status", json_string("refused"));
    json_set(out, "reason", json_string(reason ? reason : ""));
    return out;
}

static json_t *ex2_ok(json_t *value) {
    json_t *out = json_object();
    json_set(out, "status", json_string("ok"));
    json_set(out, "value", value ? value : json_null());
    return out;
}

static json_t *ex2_raised(const char *type, const char *message) {
    json_t *out = json_object();
    json_set(out, "status", json_string("raised"));
    json_set(out, "type", json_string(type ? type : "Exception"));
    json_set(out, "message", json_string(message ? message : ""));
    return out;
}

/* _worker_public(row): dict(row) minus the adoption digest. */
static json_t *ex2_public(const json_t *row) {
    json_t *out;
    size_t i;
    if (!row || row->type != JSON_OBJECT) return json_null();
    out = json_object();
    for (i = 0; i < row->c.count; i++) {
        const char *key = row->c.keys[i];
        if (!key || !strcmp(key, "adoption_digest")) continue;
        json_set(out, key, json_copy(row->c.items[i]));
    }
    return out;
}

static json_t *ex2_rows_copy(const json_t *rows) {
    json_t *out = json_object();
    size_t i;
    if (!rows || rows->type != JSON_OBJECT) return out;
    for (i = 0; i < rows->c.count; i++) {
        if (!rows->c.keys[i]) continue;
        json_set(out, rows->c.keys[i], json_copy(rows->c.items[i]));
    }
    return out;
}

static int ex2_list_has(const json_t *list, const char *value) {
    size_t i;
    if (!list || list->type != JSON_ARRAY || !value) return 0;
    for (i = 0; i < list->c.count; i++) {
        json_t *item = list->c.items[i];
        if (item && item->type == JSON_STRING && item->str_val && !strcmp(item->str_val, value)) return 1;
    }
    return 0;
}

/* _worker_assignment(): the row, or the AuthorityRefusal reason. */
static const char *ex2_assignment(json_t *sessions, json_t *rows, const char *execution_id,
                                  const char *session_id, int generation_is_int,
                                  long long generation, json_t **row_out) {
    json_t *row = execution_id ? json_obj_get(rows, execution_id) : NULL;
    json_t *session;

    if (!row || row->type != JSON_OBJECT) return "not_found";
    {
        const char *row_session = json_get_str(row, "session_id", NULL);
        if (!row_session || !session_id || strcmp(row_session, session_id)) return "permission_denied";
    }
    if (!generation_is_int) return "stale_generation";
    if (json_get_num(row, "generation", 0.0) != (double)generation) return "stale_generation";
    session = session_id ? json_obj_get(sessions, session_id) : NULL;
    if (!session || session->type != JSON_OBJECT) return "not_found";
    if (json_get_num(session, "runtime_generation", 0.0) != (double)generation) return "stale_generation";
    *row_out = row;
    return NULL;
}

static int ex2_epoch_holds(const json_t *db, int epoch_is_int, long long epoch) {
    json_t *row = json_obj_get(db, "epoch_row");
    if (!epoch_is_int) return 0;
    if (!row || row->type != JSON_NUMBER) return 0;
    return row->num_val == (double)epoch;
}

static int ex2_kind_ok(const char *kind) {
    static const char *const KINDS[] = {"cron", "child", "compute", "kanban", NULL};
    int i;
    if (!kind) return 0;
    for (i = 0; KINDS[i]; i++) if (!strcmp(kind, KINDS[i])) return 1;
    return 0;
}

/* any(fetchone() for sid in targets) over one admission-status probe. */
static int ex2_admission_hit(const json_t *db, const char *probe_key, const json_t *lineage,
                             const char *session_id) {
    json_t *probe = json_obj_get(db, probe_key);
    size_t i;
    if (!probe || probe->type != JSON_ARRAY) return 0;
    if (lineage && lineage->type == JSON_ARRAY && lineage->c.count) {
        for (i = 0; i < lineage->c.count; i++) {
            json_t *sid = lineage->c.items[i];
            if (sid && sid->type == JSON_STRING && ex2_list_has(probe, sid->str_val)) return 1;
        }
        return 0;
    }
    return ex2_list_has(probe, session_id);
}

struct json_t *w20_executions_2cbbe5_register(const struct json_t *request) {
    json_t *db = json_obj_get(request, "db");
    json_t *sessions = db ? json_obj_get(db, "sessions") : NULL;
    json_t *rows = ex2_rows_copy(db ? json_obj_get(db, "executions") : NULL);
    json_t *execution_id_value = json_obj_get(request, "execution_id");
    json_t *session_id_value = json_obj_get(request, "session_id");
    json_t *generation_value = json_obj_get(request, "generation");
    json_t *epoch_value = json_obj_get(request, "epoch");
    const char *kind = json_get_str(request, "kind", NULL);
    const char *execution_id, *session_id;
    long long generation = 0, epoch = 0;
    int generation_is_int, epoch_is_int;
    char *digest;
    json_t *out = NULL;
    json_t *old, *lineage, *states, *session;

    if (!ex2_text_ok(execution_id_value) || !ex2_text_ok(session_id_value)) {
        json_free(rows);
        return ex2_refused("invalid_params");
    }
    if (!ex2_kind_ok(kind)) {
        json_free(rows);
        return ex2_refused("invalid_params");
    }
    digest = ex2_digest(json_obj_get(request, "adoption_secret"));
    if (!digest) {
        json_free(rows);
        return ex2_refused("invalid_params");
    }
    execution_id = execution_id_value->str_val;
    session_id = session_id_value->str_val;
    generation_is_int = ex2_py_int(generation_value, &generation);
    epoch_is_int = ex2_py_int(epoch_value, &epoch);

    if (!db || db->type != JSON_OBJECT) {
        free(digest);
        json_free(rows);
        return ex2_raised("AttributeError", "'NoneType' object has no attribute 'execute'");
    }
    if (!ex2_epoch_holds(db, epoch_is_int, epoch)) {
        out = ex2_refused("stale_epoch");
        goto done;
    }
    session = sessions ? json_obj_get(sessions, session_id) : NULL;
    if (!session || session->type != JSON_OBJECT) {
        out = ex2_refused("not_found");
        goto done;
    }
    if (!generation_is_int ||
        json_get_num(session, "runtime_generation", 0.0) != (double)generation) {
        out = ex2_refused("stale_generation");
        goto done;
    }
    old = json_obj_get(rows, execution_id);
    if (old && old->type == JSON_OBJECT) {
        const char *old_session = json_get_str(old, "session_id", NULL);
        const char *old_kind = json_get_str(old, "kind", NULL);
        const char *old_digest = json_get_str(old, "adoption_digest", NULL);
        int same = (old_session && !strcmp(old_session, session_id)) &&
                   (old_kind && !strcmp(old_kind, kind)) &&
                   (old_digest && !strcmp(old_digest, digest)) &&
                   (json_get_num(old, "generation", 0.0) == (double)generation) &&
                   (json_get_num(old, "owner_epoch", 0.0) == (double)epoch);
        if (!same) {
            out = ex2_refused("admission_conflict");
            goto done;
        }
        out = ex2_ok(ex2_public(old));
        goto done;
    }
    lineage = json_obj_get(db, "lineage");
    if (json_get_bool(request, "require_idle", 0) &&
        ex2_admission_hit(db, "admissions_active", lineage, session_id)) {
        out = ex2_refused("stale_generation");
        goto done;
    }
    states = json_obj_get(db, "worker_states");
    if (states && states->type == JSON_ARRAY && states->c.count) {
        out = ex2_refused("stale_generation");
        goto done;
    }
    if (ex2_admission_hit(db, "admissions_unknown", lineage, session_id)) {
        out = ex2_refused("unknown_execution");
        goto done;
    }
    {
        json_t *inserted = json_object();
        json_t *row = NULL;
        const char *reason;
        json_set(inserted, "execution_id", json_string(execution_id));
        json_set(inserted, "session_id", json_string(session_id));
        json_set(inserted, "kind", json_string(kind));
        json_set(inserted, "owner_epoch", json_number((double)epoch));
        json_set(inserted, "generation", json_number((double)generation));
        json_set(inserted, "status", json_string("registered"));
        json_set(inserted, "adoption_digest", json_string(digest));
        if (!json_get_bool(db, "insert_missing", 0)) {
            json_set(rows, execution_id, json_copy(inserted));
        }
        json_free(inserted);
        reason = ex2_assignment(sessions, rows, execution_id, session_id,
                                generation_is_int, generation, &row);
        if (reason) {
            out = ex2_refused(reason);
            goto done;
        }
        out = ex2_ok(ex2_public(row));
        goto done;
    }

done:
    free(digest);
    json_free(rows);
    return out;
}

struct json_t *w20_executions_2cbbe5_adopt(const struct json_t *request) {
    json_t *db = json_obj_get(request, "db");
    json_t *sessions = db ? json_obj_get(db, "sessions") : NULL;
    json_t *rows = ex2_rows_copy(db ? json_obj_get(db, "executions") : NULL);
    const char *execution_id = json_get_str(request, "execution_id", NULL);
    const char *session_id = json_get_str(request, "session_id", NULL);
    long long generation = 0, epoch = 0;
    int generation_is_int, epoch_is_int;
    char *digest;
    json_t *out = NULL;
    json_t *row = NULL;
    const char *reason;

    digest = ex2_digest(json_obj_get(request, "adoption_secret"));
    if (!digest) {
        json_free(rows);
        return ex2_refused("invalid_params");
    }
    generation_is_int = ex2_py_int(json_obj_get(request, "generation"), &generation);
    epoch_is_int = ex2_py_int(json_obj_get(request, "epoch"), &epoch);

    if (!db || db->type != JSON_OBJECT) {
        free(digest);
        json_free(rows);
        return ex2_raised("AttributeError", "'NoneType' object has no attribute 'execute'");
    }
    if (!ex2_epoch_holds(db, epoch_is_int, epoch)) {
        out = ex2_refused("stale_epoch");
        goto done;
    }
    reason = ex2_assignment(sessions, rows, execution_id, session_id, generation_is_int,
                            generation, &row);
    if (reason) {
        out = ex2_refused(reason);
        goto done;
    }
    if (!strcmp(json_get_str(row, "status", ""), "terminal")) {
        out = ex2_refused("stale_generation");
        goto done;
    }
    {
        const char *row_digest = json_get_str(row, "adoption_digest", NULL);
        if (!row_digest || strcmp(row_digest, digest)) {
            out = ex2_refused("permission_denied");
            goto done;
        }
    }
    if (execution_id && !strncmp(execution_id, "admission-worker:", 17) &&
        json_get_num(row, "owner_epoch", 0.0) != (double)epoch) {
        out = ex2_refused("stale_epoch");
        goto done;
    }
    {
        json_t *linked = json_obj_get(db, "linked");
        if (linked && linked->type == JSON_OBJECT) {
            if (json_get_num(db, "admission_rowcount", 1.0) != 1.0) {
                out = ex2_refused("stale_generation");
                goto done;
            }
        }
    }
    if (json_get_num(db, "execution_rowcount", 1.0) != 1.0) {
        out = ex2_refused("stale_generation");
        goto done;
    }
    json_set(row, "owner_epoch", json_number((double)epoch));
    json_set(row, "status", json_string("running"));
    reason = ex2_assignment(sessions, rows, execution_id, session_id, generation_is_int,
                            generation, &row);
    if (reason) {
        out = ex2_refused(reason);
        goto done;
    }
    out = ex2_ok(ex2_public(row));

done:
    free(digest);
    json_free(rows);
    return out;
}
