/*
 * port_gateway_status.c
 *
 * Pure helpers ported from gateway/status.py (115 defs, previously 0 ported).
 *
 *   gs_scope_hash              <- _scope_hash
 *   gs_start_times_agree       <- _start_times_agree
 *   gs_fingerprints_match      <- start_time_fingerprints_match
 *   gs_lock_owner_label        <- scoped_lock_owner_label
 *   gs_same_hermes_home        <- _same_hermes_home
 *   gs_retained_state          <- retained_gateway_state
 *
 * The runtime-status writer / process liveness / PID probing half stays Python.
 */

#include "port_gateway_status.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "hash.h"
#include "hermes_regex.h"

/* gateway/status.py:START_TIME_DRIFT_TOLERANCE - both fingerprint scales are
 * x100 (Linux /proc ticks, psutil centiseconds), so 200 means 2 s. */
#define GS_DRIFT_TOLERANCE 200

/* gateway/status.py:_start_times_agree's window: 1 ms in SECONDS. */
#define GS_START_TIME_WINDOW 0.001

/* gateway/status.py:WATCHDOG_EXIT_REASONS */
static const char *const GS_WATCHDOG_REASONS[] = {
    "loop_liveness_watchdog", "shutdown_watchdog"
};
#define GS_WATCHDOG_N (sizeof(GS_WATCHDOG_REASONS) / sizeof(GS_WATCHDOG_REASONS[0]))

/* gateway/status.py:_PROFILE_LABEL_RE = ^[a-z0-9][a-z0-9_-]{0,63}$ */
static const char *const GS_PROFILE_LABEL_RE = "^[a-z0-9][a-z0-9_-]{0,63}$";

/* gs_canonical_home is defined below but used by gs_lock_owner_label above. */
char *gs_canonical_home(const char *path, const char *resolved);

/* ===========================================================================
 *  _scope_hash  (gateway/status.py:_scope_hash)
 *
 *      return hashlib.sha256(identity.encode("utf-8")).hexdigest()[:16]
 *
 *  The lock filename is `<scope>-<hash>.lock`; the hash is what keeps one
 *  profile's identity from colliding with another's. Caller frees.
 * =========================================================================== */

/* PoP: _scope_hash @ gateway/status.py:_scope_hash */
char *gs_scope_hash(const char *identity)
{
    if (!identity) identity = "";
    char *hex = hash_sha256_hex((const unsigned char *)identity, strlen(identity));
    if (!hex) return NULL;
    /* [:16] */
    if (strlen(hex) > 16) hex[16] = '\0';
    return hex;
}

/* ===========================================================================
 *  _start_times_agree  (gateway/status.py:_start_times_agree)
 *
 *      cur = float(current)
 *      return cur > 0 and all(r > 0 and abs(r - cur) <= 0.001
 *                             for r in map(float, recorded))
 *
 *  Same process object: every fingerprint must be POSITIVE (a zero or negative
 *  reading is unreadable, not a match) and within 1 ms of the current one.
 *  `recorded` is a NULL-terminated array of numeric strings. Returns 1/0.
 * =========================================================================== */

/* PoP: _start_times_agree @ gateway/status.py:_start_times_agree */
int gs_start_times_agree(const char *current, const char *const *recorded)
{
    char *end = NULL;
    double cur = strtod(current ? current : "", &end);
    if (end == current || !(cur > 0)) return 0;

    for (int i = 0; recorded && recorded[i]; i++) {
        char *e2 = NULL;
        double r = strtod(recorded[i], &e2);
        if (e2 == recorded[i]) return 0;      /* junk: float() would raise */
        if (!(r > 0)) return 0;
        double d = r - cur;
        if (d < 0) d = -d;
        if (d > GS_START_TIME_WINDOW) return 0;
    }
    return 1;
}

/* ===========================================================================
 *  start_time_fingerprints_match  (gateway/status.py:start_time_fingerprints_match)
 *
 *      return abs(int(current) - int(recorded)) <= tolerance
 *
 *  The integer-fingerprint form of the same test. `tolerance` defaults to 200
 *  (2 s on either fingerprint scale). Returns 1/0.
 * =========================================================================== */

/* PoP: start_time_fingerprints_match @ gateway/status.py:start_time_fingerprints_match */
int gs_fingerprints_match(const char *recorded, const char *current, int tolerance)
{
    /* `long` is 32-bit on Windows: a 12-digit /proc-tick fingerprint saturates
     * strtol and both sides read LONG_MAX, so the difference is 0 and every
     * comparison "matches". Use a 64-bit parse. */
    char *e1 = NULL, *e2 = NULL;
    long long rec = strtoll(recorded ? recorded : "", &e1, 10);
    long long cur = strtoll(current ? current : "", &e2, 10);
    if (e1 == (recorded ? recorded : "") || e2 == (current ? current : "")) return 0;
    long long d = cur - rec;
    if (d < 0) d = -d;
    return d <= tolerance;
}

/* ===========================================================================
 *  scoped_lock_owner_label  (gateway/status.py:scoped_lock_owner_label)
 *
 *      if not isinstance(record, dict): return None
 *      profile = record.get("profile")
 *      if isinstance(profile, str) and _PROFILE_LABEL_RE.match(profile.strip()):
 *          return profile.strip()
 *      home = record.get("hermes_home")
 *      return _profile_label_for_home(home) if home else None
 *
 *  `record` is a flat [key, value, ...] array. Returns a malloc'd label, or
 *  NULL for PID-only wording. Caller frees.
 * =========================================================================== */

/* PoP: scoped_lock_owner_label @ gateway/status.py:scoped_lock_owner_label */
char *gs_lock_owner_label(const char *const *record, const char *default_root)
{
    const char *profile = NULL, *home = NULL;
    for (int i = 0; record && record[i]; i += 2) {
        if (strcmp(record[i], "profile") == 0) profile = record[i + 1];
        else if (strcmp(record[i], "hermes_home") == 0) home = record[i + 1];
    }

    if (profile && profile[0]) {
        /* profile.strip() then the anchored label regex */
        const char *p = profile;
        while (*p && isspace((unsigned char)*p)) p++;
        size_t n = strlen(p);
        while (n > 0 && isspace((unsigned char)p[n - 1])) n--;

        hregex_t *re = regex_compile(GS_PROFILE_LABEL_RE, 0);
        if (re) {
            /* match against the STRIPPED string, so copy it */
            char *trimmed = (char *)malloc(n + 1);
            if (trimmed) {
                memcpy(trimmed, p, n);
                trimmed[n] = '\0';
                regex_match_t *m = regex_match(re, trimmed);
                int hit = (m && m->matched);
                if (m) regex_match_free(m);
                regex_free(re);
                if (hit) return trimmed;   /* the stripped label */
                free(trimmed);
            } else {
                regex_free(re);
            }
        }
    }

    if (!home || !home[0]) return NULL;
    /* _profile_label_for_home: <root>/profiles/<name> -> name */
    char *canon = gs_canonical_home(home, NULL);
    if (!canon) return NULL;

    /* The root home reads as "default" - compare the FULL canonical home,
     * before the basename is cut off for the profiles/ check below. */
    if (default_root && default_root[0] &&
        gs_same_hermes_home(canon, NULL, default_root, NULL)) {
        free(canon);
        return strdup("default");
    }

    char *slash = strrchr(canon, '/');
    if (!slash || slash == canon) { free(canon); return NULL; }
    *slash = '\0';
    char *parent = strrchr(canon, '/');
    const char *name = slash + 1;
    int ok = 0;
    if (parent && strcmp(parent + 1, "profiles") == 0 && name[0]) {
        hregex_t *re = regex_compile(GS_PROFILE_LABEL_RE, 0);
        if (re) {
            regex_match_t *m = regex_match(re, name);
            ok = (m && m->matched);
            if (m) regex_match_free(m);
            regex_free(re);
        }
    }
    char *out = ok ? strdup(name) : NULL;
    free(canon);
    return out;
}

/* ===========================================================================
 *  _canonical_hermes_home  (gateway/status.py:_canonical_hermes_home)
 *
 *      return Path(path).expanduser().resolve(strict=False)
 *
 *  `resolved` is the resolve() answer, or NULL to use `path` verbatim. Caller
 *  frees. (The real resolve() touches the filesystem; the port models it as a
 *  parameter, like the other path helpers.)
 * =========================================================================== */

/* PoP: _canonical_hermes_home @ gateway/status.py:_canonical_hermes_home */
char *gs_canonical_home(const char *path, const char *resolved)
{
    const char *raw = (resolved && resolved[0]) ? resolved : (path ? path : "");
    if (raw[0] == '~') {
        /* expanduser(): ~ and ~/... */
        const char *home = getenv("HERMES_HOME");
        if (!home) home = getenv("HOME");
        if (home) {
            size_t hl = strlen(home), tl = strlen(raw);
            char *out = (char *)malloc(hl + tl + 2);
            if (!out) return NULL;
            snprintf(out, hl + tl + 2, "%s%s", home, raw + 1);
            return out;
        }
    }
    return strdup(raw);
}

/* ===========================================================================
 *  _same_hermes_home  (gateway/status.py:_same_hermes_home)
 *
 *      left_c = os.path.normcase(str(_canonical_hermes_home(left)))
 *      return left_c == os.path.normcase(str(_canonical_hermes_home(right)))
 *
 *  normcase on Windows lower-cases and converts / to \; on POSIX it is a no-op.
 *  `left_resolved` / `right_resolved` stand in for the two resolve() answers.
 * =========================================================================== */

/* PoP: _same_hermes_home @ gateway/status.py:_same_hermes_home */
int gs_same_hermes_home(const char *left, const char *left_resolved,
                        const char *right, const char *right_resolved)
{
    char *lc = gs_canonical_home(left, left_resolved);
    char *rc = gs_canonical_home(right, right_resolved);
    if (!lc || !rc) { free(lc); free(rc); return 0; }

    /* normcase: lower-case and / -> \ */
    for (size_t i = 0; lc[i]; i++) {
        lc[i] = (char)tolower((unsigned char)lc[i]);
        if (lc[i] == '/') lc[i] = '\\';
    }
    for (size_t i = 0; rc[i]; i++) {
        rc[i] = (char)tolower((unsigned char)rc[i]);
        if (rc[i] == '/') rc[i] = '\\';
    }
    int same = (strcmp(lc, rc) == 0);
    free(lc);
    free(rc);
    return same;
}

/* ===========================================================================
 *  retained_gateway_state  (gateway/status.py:retained_gateway_state)
 *
 *      rt = runtime if isinstance(runtime, dict) else {}
 *      if rt.get("desired_state") != "stopped":
 *          if rt.get("gateway_state") == "startup_failed": return "startup_failed"
 *          if rt.get("gateway_state") == "degraded" and rt.get("exit_reason") in WATCHDOG_EXIT_REASONS:
 *              return "degraded"
 *      return "stopped"
 *
 *  `runtime` is a flat [key, value, ...] array; NULL reads as {} (a dead process
 *  is just "stopped"). Shared by /api/status and /api/messaging/platforms so the
 *  sidebar strip and the Channels page cannot disagree. Caller frees.
 * =========================================================================== */

/* PoP: retained_gateway_state @ gateway/status.py:retained_gateway_state */
char *gs_retained_state(const char *const *runtime)
{
    const char *desired = NULL, *state = NULL, *reason = NULL;
    for (int i = 0; runtime && runtime[i]; i += 2) {
        if (strcmp(runtime[i], "desired_state") == 0) desired = runtime[i + 1];
        else if (strcmp(runtime[i], "gateway_state") == 0) state = runtime[i + 1];
        else if (strcmp(runtime[i], "exit_reason") == 0) reason = runtime[i + 1];
    }

    if (!(desired && strcmp(desired, "stopped") == 0)) {
        if (state && strcmp(state, "startup_failed") == 0) return strdup("startup_failed");
        if (state && strcmp(state, "degraded") == 0 && reason) {
            for (size_t i = 0; i < GS_WATCHDOG_N; i++)
                if (strcmp(reason, GS_WATCHDOG_REASONS[i]) == 0)
                    return strdup("degraded");
        }
    }
    return strdup("stopped");
}
