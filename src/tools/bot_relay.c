/*
 * bot_relay.c — C11 port of tools/bot_relay.py.
 *
 * Envelope/turn-lock machinery for the Desktop bot relay: roster-backed
 * remote-target resolution, outbox envelope queueing with fail-fast offline
 * refusal, reply persistence, and the per-profile cross-process turn lock
 * (flock probe + bounded retry — no deadlock among waiters).
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include "bot_relay.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "uuid.h"
#include "port_config_py_helpers.h"
#include "libregex/hermes_regex.h"

/* ROSTER_FRESH_SECONDS @ tools/bot_relay.py:60 */
static const double BR_ROSTER_FRESH_SECONDS = 600.0;

/* ── small string helpers ────────────────────────────────────────────────── */

static char *br_strdup(const char *s)
{
    return s ? strdup(s) : NULL;
}

/* Python str.strip() on ASCII whitespace. */
static char *br_strip(const char *s)
{
    if (!s) return strdup("");
    const char *b = s;
    const char *e = s + strlen(s);
    while (*b && isspace((unsigned char)*b)) b++;
    while (e > b && isspace((unsigned char)e[-1])) e--;
    char *out = (char *)malloc((size_t)(e - b) + 1);
    if (!out) return NULL;
    memcpy(out, b, (size_t)(e - b));
    out[e - b] = '\0';
    return out;
}

static const char *br_obj_str(const json_t *obj, const char *key)
{
    if (!json_is_object(obj)) return NULL;
    json_t *v = json_obj_get((json_t *)obj, key);
    return json_is_string(v) ? v->str_val : NULL;
}

/* Python str(x or "") semantics for possibly-missing fields. */
static const char *br_or_empty(const char *s) { return s ? s : ""; }

/* ── paths / dirs ────────────────────────────────────────────────────────── */

/* PoP: bot_relay_relay_root @ tools/bot_relay.py:relay_root */
const char *bot_relay_relay_root(const char *root, char *buf, size_t n)
{
    if (!root || !buf) return NULL;
    int w = snprintf(buf, n, "%s/%s", root, BR_RELAY_DIR_NAME);
    return (w > 0 && (size_t)w < n) ? buf : NULL;
}

static void br_mkdir_p(const char *path)
{
    if (!path || !*path) return;
    char tmp[4096];
    snprintf(tmp, sizeof tmp, "%s", path);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') { *p = '\0'; mkdir(tmp, 0755); *p = '/'; }
    }
    mkdir(tmp, 0755);
}

/* PoP: bot_relay_ensure_dirs @ tools/bot_relay.py:_ensure_dirs */
const char *bot_relay_ensure_dirs(const char *root, char *buf, size_t n)
{
    if (!bot_relay_relay_root(root, buf, n)) return NULL;
    char sub[4200];
    static const char *dirs[] = { BR_OUTBOX_DIR, BR_CLAIMED_DIR, BR_REPLIES_DIR };
    for (size_t i = 0; i < sizeof dirs / sizeof dirs[0]; i++) {
        int w = snprintf(sub, sizeof sub, "%s/%s", buf, dirs[i]);
        if (w > 0 && (size_t)w < sizeof sub) br_mkdir_p(sub);
    }
    return buf;
}

/* _normalize_roster_row @ tools/bot_relay.py:_normalize_roster_row */
static json_t *br_normalize_roster_row(const json_t *row)
{
    if (!json_is_object(row)) return NULL;
    const char *handle = br_obj_str(row, "handle");
    const char *profile = br_obj_str(row, "profile");
    const char *conn = br_obj_str(row, "connection_id");
    if (!handle || !*handle || !profile || !*profile || !conn || !*conn)
        return NULL;
    json_t *out = json_copy(row);
    return out;
}

/* PoP: bot_relay_read_remote_roster @ tools/bot_relay.py:read_remote_roster */
json_t *bot_relay_read_remote_roster(const char *root)
{
    char rroot[4096], path[4352];
    if (!bot_relay_relay_root(root, rroot, sizeof rroot)) return json_array();
    int w = snprintf(path, sizeof path, "%s/%s", rroot, BR_ROSTER_FILE);
    if (w <= 0 || (size_t)w >= sizeof path) return json_array();

    FILE *f = fopen(path, "rb");
    if (!f) return json_array();               /* FileNotFoundError → [] */
    char *raw = NULL;
    size_t cap = 0, len = 0;
    for (;;) {
        if (len + 4096 + 1 > cap) { cap = cap ? cap * 2 : 8192; raw = (char *)realloc(raw, cap); if (!raw) break; }
        size_t got = fread(raw + len, 1, 4096, f);
        len += got;
        if (got < 4096) break;
    }
    fclose(f);
    if (!raw) return json_array();
    raw[len] = '\0';

    json_t *data = json_parse(raw, NULL);
    free(raw);
    if (!data) return json_array();

    json_t *agents = json_is_object(data) ? json_obj_get(data, "agents") : NULL;
    json_t *out = json_array();
    if (json_is_array(agents)) {
        size_t count = json_array_size(agents);
        for (size_t i = 0; i < count; i++) {
            json_t *norm = br_normalize_roster_row(json_get(agents, (int)i));
            if (norm) json_append(out, norm);   /* json_append steals norm */
        }
    }
    json_free(data);
    return out;
}

/* ── target resolution ───────────────────────────────────────────────────── */

/* PoP: resolve_remote_target @ tools/bot_relay.py:resolve_remote_target */
json_t *resolve_remote_target(const char *raw_target, const json_t *roster)
{
    char *want = br_strip(raw_target);
    if (!want) return NULL;
    size_t start = 0;
    while (want[start] == '@') start++;          /* lstrip("@") */
    if (start) memmove(want, want + start, strlen(want) - start + 1);

    char *conn = NULL;
    char *at = strchr(want, '@');
    if (at) {
        *at = '\0';
        char *cw = br_strip(want);
        if (cw) { free(want); want = cw; }
        conn = br_strip(at + 1);
    }
    if (!*want || (conn && !*conn)) {
        free(want); free(conn);
        return NULL;
    }

    /* lowercase copies of want and conn for case-insensitive compare */
    char wl[256], cl[256];
    snprintf(wl, sizeof wl, "%s", want);
    for (char *p = wl; *p; p++) *p = (char)tolower((unsigned char)*p);
    if (conn) {
        snprintf(cl, sizeof cl, "%s", conn);
        for (char *p = cl; *p; p++) *p = (char)tolower((unsigned char)*p);
    }

    json_t *matches[64];
    size_t nmatch = 0;
    if (json_is_array(roster)) {
        size_t count = json_array_size(roster);
        for (size_t i = 0; i < count && nmatch < 64; i++) {
            json_t *row = json_get((json_t *)roster, (int)i);
            const char *h = br_obj_str(row, "handle");
            const char *pf = br_obj_str(row, "profile");
            const char *cid = br_obj_str(row, "connection_id");
            if (!h || !pf || !cid) continue;
            char hl[256];
            snprintf(hl, sizeof hl, "%s", h);
            for (char *p = hl; *p; p++) *p = (char)tolower((unsigned char)*p);
            int hit = (strcmp(wl, hl) == 0);
            if (!hit) {
                char pl[256];
                snprintf(pl, sizeof pl, "%s", pf);
                for (char *p = pl; *p; p++) *p = (char)tolower((unsigned char)*p);
                hit = (strcmp(wl, pl) == 0);
            }
            if (!hit) continue;
            if (conn) {
                char il[256];
                snprintf(il, sizeof il, "%s", cid);
                for (char *p = il; *p; p++) *p = (char)tolower((unsigned char)*p);
                if (strcmp(cl, il) != 0) continue;
            }
            matches[nmatch++] = row;
        }
    }

    json_t *result;
    if (nmatch == 0) {
        result = NULL;
    } else if (nmatch > 1) {
        result = json_string("ambiguous");
    } else {
        result = json_copy(matches[0]);
    }
    free(want);
    free(conn);
    return result;
}

/* PoP: remote_target_forms @ tools/bot_relay.py:remote_target_forms */
json_t *remote_target_forms(const json_t *roster)
{
    json_t *forms = json_array();
    if (!json_is_array(roster)) return forms;

    /* by_handle counts (lowercased handle → occurrences). */
    struct { char key[256]; size_t n; } counts[128];
    size_t ncounts = 0;
    size_t total = json_array_size(roster);
    for (size_t i = 0; i < total; i++) {
        json_t *row = json_get((json_t *)roster, (int)i);
        const char *h = br_obj_str(row, "handle");
        if (!h) continue;
        char low[256];
        snprintf(low, sizeof low, "%s", h);
        for (char *p = low; *p; p++) *p = (char)tolower((unsigned char)*p);
        size_t j;
        for (j = 0; j < ncounts; j++)
            if (strcmp(counts[j].key, low) == 0) { counts[j].n++; break; }
        if (j == ncounts && ncounts < 128) {
            snprintf(counts[ncounts].key, sizeof counts[ncounts].key, "%s", low);
            counts[ncounts].n = 1;
            ncounts++;
        }
    }

    for (size_t i = 0; i < total; i++) {
        json_t *row = json_get((json_t *)roster, (int)i);
        const char *h = br_obj_str(row, "handle");
        const char *cid = br_obj_str(row, "connection_id");
        if (!h) continue;
        char low[256];
        snprintf(low, sizeof low, "%s", h);
        for (char *p = low; *p; p++) *p = (char)tolower((unsigned char)*p);
        size_t cnt = 0;
        for (size_t j = 0; j < ncounts; j++)
            if (strcmp(counts[j].key, low) == 0) { cnt = counts[j].n; break; }
        if (cnt > 1 && cid) {
            char form[512];
            snprintf(form, sizeof form, "%s@%s", h, cid);
            json_append(forms, json_string(form));
        } else {
            json_append(forms, json_string(h));
        }
    }
    return forms;
}

/* ── liveness + enqueue ──────────────────────────────────────────────────── */

/* _target_liveness @ tools/bot_relay.py:_target_liveness
 * Tri-state: 1 online / 0 definitively offline / -1 unknown. */
static int br_target_liveness(const char *root, const json_t *target)
{
    char rroot[4096], path[4352];
    if (!bot_relay_relay_root(root, rroot, sizeof rroot)) return -1;
    int w = snprintf(path, sizeof path, "%s/%s", rroot, BR_ROSTER_FILE);
    if (w <= 0 || (size_t)w >= sizeof path) return -1;

    struct stat st;
    if (stat(path, &st) != 0) return -1;        /* no roster ever synced */
    double age = (double)time(NULL) - (double)st.st_mtime;
    if (age > BR_ROSTER_FRESH_SECONDS) return -1;

    json_t *roster = bot_relay_read_remote_roster(root);
    int verdict = -1;
    if (json_is_array(roster) && json_array_size(roster) > 0) {
        const char *tcid = br_or_empty(br_obj_str(target, "connection_id"));
        const char *tpf  = br_or_empty(br_obj_str(target, "profile"));
        size_t count = json_array_size(roster);
        for (size_t i = 0; i < count; i++) {
            json_t *row = json_get(roster, (int)i);
            const char *rcid = br_or_empty(br_obj_str(row, "connection_id"));
            const char *rpf  = br_or_empty(br_obj_str(row, "profile"));
            if (strcmp(tcid, rcid) != 0 || strcmp(tpf, rpf) != 0) continue;
            json_t *online = json_obj_get(row, "online");
            if (json_is_bool(online))
                verdict = online->bool_val ? 1 : 0;
            break;                              /* found the row */
        }
        if (verdict == -1) {
            /* scanned fully without a row match → fresh roster dropped it */
            int found = 0;
            for (size_t i = 0; i < count; i++) {
                json_t *row = json_get(roster, (int)i);
                const char *rcid = br_or_empty(br_obj_str(row, "connection_id"));
                const char *rpf  = br_or_empty(br_obj_str(row, "profile"));
                if (strcmp(tcid, rcid) == 0 && strcmp(tpf, rpf) == 0) { found = 1; break; }
            }
            if (!found) verdict = 0;
        }
    }
    json_free(roster);
    return verdict;
}

/* Atomic write: tempfile in dir, fsync-free os.replace equivalent. */
static int br_atomic_write(const char *dir, const char *prefix,
                           const char *final_path, const char *payload)
{
    char tmpl[4608];
    int w = snprintf(tmpl, sizeof tmpl, "%s/%sXXXXXX", dir, prefix);
    if (w <= 0 || (size_t)w >= sizeof tmpl) return -1;
    char tmp[4700];
    snprintf(tmp, sizeof tmp, "%s", tmpl);
    int fd = mkstemp(tmp);
    if (fd < 0) return -1;
    size_t len = strlen(payload);
    ssize_t put = write(fd, payload, len);
    int werr = (put < 0 || (size_t)put != len);
    if (close(fd) != 0) werr = 1;
    if (werr || rename(tmp, final_path) != 0) { unlink(tmp); return -1; }
    return 0;
}

/* PoP: enqueue_envelope @ tools/bot_relay.py:enqueue_envelope */
json_t *enqueue_envelope(const char *root, const json_t *target,
                         const char *message, const char *sender_profile,
                         const char *sender_handle, bot_relay_error_t *err)
{
    if (err) { err->reason[0] = '\0'; err->message[0] = '\0'; }

    if (br_target_liveness(root, target) == 0) {
        const char *who = br_obj_str(target, "handle");
        if (!who || !*who) who = br_obj_str(target, "profile");
        const char *where = br_obj_str(target, "connection_label");
        if (!where || !*where) where = br_obj_str(target, "connection_id");
        if (err) {
            snprintf(err->reason, sizeof err->reason, "runtime_offline");
            snprintf(err->message, sizeof err->message,
                     "@%s on %s is offline right now — the message was NOT queued. "
                     "Try again once that machine reconnects to the Desktop.",
                     (who && *who) ? who : "?",
                     (where && *where) ? where : "?");
        }
        return NULL;                            /* EnvelopeRefusedError */
    }

    char base[4096], outbox[4200], epath[4300];
    if (!bot_relay_ensure_dirs(root, base, sizeof base)) return NULL;
    if (snprintf(outbox, sizeof outbox, "%s/%s", base, BR_OUTBOX_DIR) >= (int)sizeof outbox)
        return NULL;

    char *uid = uuid_v4();
    char hex[33]; size_t hn = 0;
    if (uid) {
        for (const char *p = uid; *p && hn < 32; p++) if (*p != '-') hex[hn++] = *p;
        free(uid);
    }
    hex[hn] = '\0';

    json_t *env = json_object();
    json_set(env, "id", json_string(hex));
    json_set(env, "created_at", json_int((long)time(NULL)));
    json_set(env, "from_profile", json_string(br_or_empty(sender_profile)));
    json_set(env, "from_handle", json_string(br_or_empty(sender_handle)));
    json_set(env, "target_connection", json_string(br_or_empty(br_obj_str(target, "connection_id"))));
    json_set(env, "target_profile", json_string(br_or_empty(br_obj_str(target, "profile"))));
    json_set(env, "target_handle", json_string(br_or_empty(br_obj_str(target, "handle"))));
    json_set(env, "message", json_string(message ? message : ""));

    char *payload = json_dumps(env, 0);
    if (!payload) { json_free(env); return NULL; }
    if (snprintf(epath, sizeof epath, "%s/%s.json", outbox, hex) < (int)sizeof epath)
        br_atomic_write(outbox, ".env-", epath, payload);
    free(payload);
    return env;
}

/* ── failure-reason classification (tools/bot_failure_reasons._RULES) ────── */

/* Ordered (pattern, code) rules — first match wins; auth beats quota. */
static const struct { const char *pattern; const char *code; } BR_RULES[] = {
    { "authentication_error|invalid api key"
      "|(?:error code:?\\s*|status(?:\\s*code)?:?\\s*|http\\s*)(?:401|403)\\b",
      "provider_auth_or_access" },
    { "(?:error code:?\\s*|status(?:\\s*code)?:?\\s*|http\\s*)402\\b"
      "|out of funds|quota|balance",
      "provider_quota_limit" },
    { "(?:error code:?\\s*|status(?:\\s*code)?:?\\s*|http\\s*)429\\b|rate.?limit",
      "provider_rate_limit" },
    { "(?:error code:?\\s*|status(?:\\s*code)?:?\\s*|http\\s*)5\\d{2}\\b"
      "|server error|overloaded",
      "provider_server_error" },
    { "context length|context_overflow|maximum context", "context_overflow" },
    { "no llm provider configured|missing config|no access token",
      "missing_config" },
    { "model .*(not found|does not exist)|model_not_found", "model_unavailable" },
};

/* PoP: bot_failure_is_auto_retryable @ tools/bot_failure_reasons.py:is_auto_retryable */
/* True when `reason` is safe to retry automatically (AUTO_RETRYABLE set). */
int bot_failure_is_auto_retryable(const char *reason)
{
    if (!reason) return 0;
    static const char *const AUTO_RETRYABLE[] = {
        "runtime_offline", "delivery_timeout",
        "provider_rate_limit", "provider_server_error", NULL,
    };
    for (int i = 0; AUTO_RETRYABLE[i]; i++)
        if (strcmp(reason, AUTO_RETRYABLE[i]) == 0) return 1;
    return 0;
}

/* PoP: bot_failure_retry_action @ tools/bot_failure_reasons.py:retry_action */
/* Map a failure reason to the bot-turn retry action:
 * transient -> "resume"; context_overflow -> "compress_then_resume";
 * everything else -> "none" (never auto-retry auth/quota/config). */
const char *bot_failure_retry_action(const char *reason)
{
    if (bot_failure_is_auto_retryable(reason)) return "resume";
    if (reason && strcmp(reason, "context_overflow") == 0)
        return "compress_then_resume";
    return "none";
}

/*
 * (promoted from the former inlined br_classify_agent_error — same rules,
 * PoP: bot_failure_classify_agent_error @ tools/bot_failure_reasons.py:classify_agent_error
 */
void bot_failure_classify_agent_error(const char *text, char *out, size_t n)
{
    const char *raw = text ? text : "";
    const char *b = raw;
    while (*b && isspace((unsigned char)*b)) b++;
    if (!*b) { snprintf(out, n, "unknown"); return; }
    for (size_t i = 0; i < sizeof BR_RULES / sizeof BR_RULES[0]; i++) {
        hregex_t *re = regex_compile(BR_RULES[i].pattern, REGEX_FLAG_ICASE);
        if (!re) continue;
        regex_match_t *m = regex_search(re, raw);
        regex_free(re);
        if (m) { regex_match_free(m); snprintf(out, n, "%s", BR_RULES[i].code); return; }
    }
    snprintf(out, n, "unknown");
}

/* PoP: write_reply @ tools/bot_relay.py:write_reply */
int write_reply(const char *root, const char *envelope_id, const char *reply,
                const char *error, const char *reason,
                char *path_buf, size_t n)
{
    char base[4096], replies[4200], rpath[4300];
    if (!bot_relay_ensure_dirs(root, base, sizeof base)) return -1;
    if (snprintf(replies, sizeof replies, "%s/%s", base, BR_REPLIES_DIR) >= (int)sizeof replies)
        return -1;

    char safe[256];
    char *sid = br_strip(envelope_id);
    snprintf(safe, sizeof safe, "%s", sid ? sid : "");
    free(sid);

    /* re.match(r"^[0-9a-f]{32}$", safe) — else ValueError. */
    if (strlen(safe) != 32) return -1;
    for (const char *p = safe; *p; p++)
        if (!isdigit((unsigned char)*p) && (*p < 'a' || *p > 'f')) return -1;

    const char *err = error ? error : "";
    char code[64];
    if (reason && *reason) {
        snprintf(code, sizeof code, "%s", reason);
    } else if (*err) {
        bot_failure_classify_agent_error(err, code, sizeof code);
    } else {
        code[0] = '\0';
    }

    json_t *payload = json_object();
    json_set(payload, "id", json_string(safe));
    json_set(payload, "at", json_int((long)time(NULL)));
    json_set(payload, "reply", json_string(reply ? reply : ""));
    json_set(payload, "error", json_string(err));
    json_set(payload, "reason", json_string(code));

    char *text = json_dumps(payload, 0);
    json_free(payload);
    if (!text) return -1;
    if (snprintf(rpath, sizeof rpath, "%s/%s.json", replies, safe) >= (int)sizeof rpath) {
        free(text);
        return -1;
    }
    int rc = br_atomic_write(replies, ".rep-", rpath, text);
    free(text);
    if (rc != 0) return -1;
    if (path_buf) {
        if (snprintf(path_buf, n, "%s", rpath) >= (int)n) return -1;
    }
    return 0;
}

/* PoP: local_delivery_command @ tools/bot_relay.py:local_delivery_command */
json_t *local_delivery_command(const char *profile, const char *query_file)
{
    static const char *argv[] = {
        "hermes", "-p", NULL, "chat", "--in", "~", "-c", "Bot Chat",
        "--create-if-missing", "-Q", "--query-file", NULL,
    };
    json_t *cmd = json_array();
    for (size_t i = 0; i < sizeof argv / sizeof argv[0]; i++) {
        const char *a = argv[i];
        if (!a) a = (i == 2) ? profile : query_file;
        json_append(cmd, json_string(a ? a : ""));
    }
    return cmd;
}

/* PoP: turn_wait_seconds @ tools/bot_relay.py:turn_wait_seconds */
double turn_wait_seconds(void)
{
    json_t *cfg = config_py_load_config_readonly();
    if (cfg) {
        json_t *val = config_py_get_config_value(cfg, "bot_mode.turn_wait_seconds", NULL);
        if (val && json_is_number(val)) {
            double v = val->num_val;
            json_free(cfg);
            return v > 0.0 ? v : 0.0;
        }
        json_free(cfg);
    }
    return BR_TURN_WAIT_SECONDS_FALLBACK;
}

/* PoP: turn_lock_path @ tools/bot_relay.py:turn_lock_path */
const char *turn_lock_path(const char *root, const char *profile,
                           char *buf, size_t n)
{
    if (!root || !buf) return NULL;
    char rroot[4096];
    if (!bot_relay_relay_root(root, rroot, sizeof rroot)) return NULL;

    char safe[65];
    size_t si = 0;
    const char *p = profile ? profile : "";
    for (; *p && si < 64; p++)
        safe[si++] = (isalnum((unsigned char)*p) || *p == '_' || *p == '-')
                         ? *p : '_';
    safe[si] = '\0';
    if (!safe[0]) { safe[0] = '_'; safe[1] = '\0'; }

    int w = snprintf(buf, n, "%s/%s/%s.lock", rroot, BR_LOCKS_DIR, safe);
    return (w > 0 && (size_t)w < n) ? buf : NULL;
}

/* PoP: acquire_turn_lock @ tools/bot_relay.py:acquire_turn_lock */
int acquire_turn_lock(const char *root, const char *profile,
                      double timeout_seconds, char *path_buf, size_t n)
{
    char lpath[4300];
    if (!turn_lock_path(root, profile, lpath, sizeof lpath)) return -1;
    if (path_buf) {
        if (snprintf(path_buf, n, "%s", lpath) >= (int)n) return -1;
    }

    /* path.parent.mkdir(parents=True, exist_ok=True) */
    char parent[4200];
    snprintf(parent, sizeof parent, "%s", lpath);
    char *slash = strrchr(parent, '/');
    if (slash) { *slash = '\0'; br_mkdir_p(parent); }

    int fd = open(lpath, O_RDWR | O_CREAT, 0600);
    if (fd < 0) return -1;

    double budget = timeout_seconds < 0.0
                        ? turn_wait_seconds()
                        : (timeout_seconds > 0.0 ? timeout_seconds : 0.0);

    struct timespec t0;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    double deadline = (double)t0.tv_sec + (double)t0.tv_nsec / 1e9 + budget;

    for (;;) {
        if (flock(fd, LOCK_EX | LOCK_NB) == 0)
            return fd;                          /* held; caller releases */
        struct timespec now_ts;
        clock_gettime(CLOCK_MONOTONIC, &now_ts);
        double now = (double)now_ts.tv_sec + (double)now_ts.tv_nsec / 1e9;
        if (now >= deadline) {
            close(fd);                          /* TurnBusyError(profile, ...) */
            return -1;
        }
        double remaining = deadline - now;
        double sleep_s = remaining < 0.1 ? remaining : 0.1;
        if (sleep_s < 0.005) sleep_s = 0.005;
        struct timespec ts = { (time_t)sleep_s,
                               (long)((sleep_s - (double)(time_t)sleep_s) * 1e9) };
        nanosleep(&ts, NULL);
    }
}

/* Generator finally: flock(LOCK_UN) then close. */
void release_turn_lock(int fd)
{
    if (fd < 0) return;
    flock(fd, LOCK_UN);
    close(fd);
}
