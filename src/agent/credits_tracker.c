/*
 * credits_tracker.c — Credits tracking for Nous inference API.
 *
 * Port of Python agent/credits_tracker.py (724 lines).
 * 8 module-level functions + 2 classes (CreditsState, AgentNotice).
 * Pure logic: regex parsing of x-nous-credits-* headers, integer math,
 * USD validation, time-based expiry. Fully portable.
 *
 * Port of Python: _safe_int
 * Port of Python: _validate_usd
 * Port of Python: evaluate_credits_notices
 * Port of Python: parse_credits_headers
 * Port of Python: dev_fixture_credits_state
 * Port of Python: _credits_state_from_account (agent-dependent — INLINE in C agent loop)
 * Port of Python: _hydrate_seed_state (agent-dependent — INLINE in C agent loop)
 * Port of Python: seed_credits_at_session_start (agent-dependent — INLINE in C agent loop)
 * N/A: CreditsState — Python dataclass (C has credits_state_t struct)
 * N/A: AgentNotice — Python dataclass (C has agent_notice_t struct)
 */
#define _GNU_SOURCE
#include "hermes_credits_tracker.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <time.h>

/* ── Band constants ───────────────────────────────────────────────────── */

const double   CREDITS_BAND_THRESHOLDS[CREDITS_BAND_COUNT] = {0.50, 0.75, 0.90};
const char    *CREDITS_BAND_LEVELS[CREDITS_BAND_COUNT]     = {"info", "warn", "warn"};
const int      CREDITS_BAND_PCTS[CREDITS_BAND_COUNT]        = {50, 75, 90};

/* ── Dev fixtures ─────────────────────────────────────────────────────── */

static const struct {
    const char *name;
    int64_t remaining_micros;
    const char *remaining_usd;
    int64_t subscription_micros;
    const char *subscription_usd;
    int64_t subscription_limit_micros;
    const char *subscription_limit_usd;
    int64_t purchased_micros;
    const char *purchased_usd;
    const char *denominator_kind;
    bool paid_access;
    const char *disabled_reason;
    int64_t rollover_micros;
} CREDITS_DEV_FIXTURES[] = {
    {"healthy",
        30340000, "30.34",
        18000000, "18.00",
        20000000, "20.00",
        12340000, "12.34",
        "subscription_cap", true, "",
        0},
    {"sub_50pct",
        10000000, "10.00",
        10000000, "10.00",
        20000000, "20.00",
        0, "0.00",
        "subscription_cap", true, "",
        0},
    {"sub_75pct",
        5000000, "5.00",
        5000000, "5.00",
        20000000, "20.00",
        0, "0.00",
        "subscription_cap", true, "",
        0},
    {"sub_90pct",
        2000000, "2.00",
        2000000, "2.00",
        20000000, "20.00",
        0, "0.00",
        "subscription_cap", true, "",
        0},
    {"grant_exhausted",
        12340000, "12.34",
        0, "0.00",
        20000000, "20.00",
        12340000, "12.34",
        "subscription_cap", true, "",
        0},
    {"depleted",
        0, "0.00",
        0, "0.00",
        CREDITS_LIMIT_NONE, "",
        0, "0.00",
        "none", false, "out_of_credits",
        0},
    {"debt",
        0, "0.00",
        -5000000, "-5.00",
        20000000, "20.00",
        0, "0.00",
        "subscription_cap", false, "out_of_credits",
        0},
};
#define CREDITS_DEV_FIXTURE_COUNT \
    (sizeof(CREDITS_DEV_FIXTURES) / sizeof(CREDITS_DEV_FIXTURES[0]))

/* ── CreditsState constructor ─────────────────────────────────────────── */

void credits_state_init(credits_state_t *s) {
    if (!s) return;
    memset(s, 0, sizeof(*s));
    s->subscription_limit_micros = CREDITS_LIMIT_NONE;
    s->tool_pool_micros = 0;
    s->tool_pool_gated_off = false;
    s->paid_access = true;
    s->denominator_kind[0] = '\0';
    s->disabled_reason[0] = '\0';
    s->from_header = false;
}

/* ── CreditsState computed properties ───────────────────────────────────── */

/* AG26: Port of Python agent/credits_tracker.py:used_fraction(). */
double credits_state_used_fraction(const credits_state_t *s) {
    if (!s) return -1.0;
    if (s->subscription_limit_micros == CREDITS_LIMIT_NONE)
        return -1.0;
    if (s->subscription_limit_micros <= 0)
        return -1.0;
    double used = (double)(s->subscription_limit_micros - s->subscription_micros);
    double frac = used / (double)s->subscription_limit_micros;
    if (frac < 0.0) frac = 0.0;
    if (frac > 1.0) frac = 1.0;
    return frac;
}

/* ── _safe_int ────────────────────────────────────────────────────────── */

/* Port of Python: _safe_int */
bool safe_int(const char *value, int64_t *out) {
    if (!value || !value[0]) return false;
    char *end = NULL;
    long long val = strtoll(value, &end, 10);
    if (end == value || *end != '\0') return false;
    /* Reject float-shaped strings like "1.5" — strtoll stops at '.' but
     * we check *end == '\0' above, so "1.5" fails correctly. */
    if (out) *out = (int64_t)val;
    return true;
}

/* ── _validate_usd ────────────────────────────────────────────────────── */

/* Port of Python: _validate_usd */
bool validate_usd(const char *value) {
    if (!value || !value[0]) return false;
    /* Pattern: ^-?\d+\.\d{2}$ */
    const char *p = value;
    if (*p == '-') p++;
    if (!*p) return false;
    /* Must have at least one digit before dot */
    if (!isdigit((unsigned char)*p)) return false;
    while (isdigit((unsigned char)*p)) p++;
    if (*p != '.') return false;
    p++;
    /* Exactly 2 digits after dot */
    if (!isdigit((unsigned char)*p)) return false;
    p++;
    if (!isdigit((unsigned char)*p)) return false;
    p++;
    return *p == '\0';
}

/* ── parse_credits_headers ────────────────────────────────────────────── */

/* Port of Python: parse_credits_headers */
bool credits_tracker_parse_headers(credits_state_t *state,
                                    const char *const *headers,
                                    const char *provider) {
    (void)provider; /* unused in parse, preserved for future logging */

    if (!state || !headers) return false;

    /* Step 1: Check for the version header (cheap probe) */
    bool has_version = false;
    for (const char *const *h = headers; *h; h++) {
        const char *val = *h;
        /* Find colon delimiter */
        const char *colon = strchr(val, ':');
        if (!colon) continue;

        /* Compare header name case-insensitively */
        size_t name_len = (size_t)(colon - val);
        if (name_len == strlen("x-nous-credits-version") &&
            strncasecmp(val, "x-nous-credits-version", name_len) == 0) {
            has_version = true;
            break;
        }
    }
    if (!has_version) return false;

    /* Step 2: Build lowercase header lookup from the array */
    /* We store up to 64 headers for the lookup */
#define CREDITS_MAX_HEADERS 64
    struct { char name[128]; char value[1024]; } lookup[CREDITS_MAX_HEADERS];
    int n_lookup = 0;

    for (const char *const *h = headers; *h && n_lookup < CREDITS_MAX_HEADERS; h++) {
        const char *colon = strchr(*h, ':');
        if (!colon) continue;

        /* Lowercase the header name */
        size_t name_len = (size_t)(colon - *h);
        if (name_len >= sizeof(lookup[0].name)) name_len = sizeof(lookup[0].name) - 1;
        for (size_t i = 0; i < name_len; i++)
            lookup[n_lookup].name[i] = tolower((unsigned char)(*h)[i]);
        lookup[n_lookup].name[name_len] = '\0';

        /* Skip colon and whitespace for the value */
        const char *v = colon + 1;
        while (*v == ' ' || *v == '\t') v++;
        size_t vlen = strlen(v);
        if (vlen >= sizeof(lookup[0].value)) vlen = sizeof(lookup[0].value) - 1;
        memcpy(lookup[n_lookup].value, v, vlen);
        lookup[n_lookup].value[vlen] = '\0';

        n_lookup++;
    }

    /* Helper: get a header value by (lowercase) name */
    const char *get_hdr(const char *name) {
        for (int i = 0; i < n_lookup; i++) {
            if (strcmp(lookup[i].name, name) == 0)
                return lookup[i].value;
        }
        return NULL;
    }

    /* Step 3: Version check — must be present and exactly 1 */
    const char *version_raw = get_hdr("x-nous-credits-version");
    if (!version_raw) return false;

    int64_t version_val;
    if (!safe_int(version_raw, &version_val))
        return false;
    if (version_val != 1) {
        /* Python: version > 1 emits a one-time logger.warning */
        static bool version_warning_emitted = false;
        if (version_val > 1 && !version_warning_emitted) {
            version_warning_emitted = true;
            hermes_log(LOG_WARNING, "credits",
                       "credits header version %lld unsupported, ignoring — update Hermes",
                       (long long)version_val);
        }
        return false;
    }

    /* Step 4: Parse required non-negative int fields */
    /* Helper: get a required non-negative int, fail on sentinel */
    #define REQ_NONNEG(field, key) do { \
        const char *_raw = get_hdr(key); \
        int64_t _val; \
        if (!safe_int(_raw, &_val)) return false; \
        if (_val < 0) return false; \
        state->field = _val; \
    } while(0)

    /* Helper: get a required int that may be negative (subscription only) */
    #define REQ_INT(field, key) do { \
        const char *_raw = get_hdr(key); \
        int64_t _val; \
        if (!safe_int(_raw, &_val)) return false; \
        state->field = _val; \
    } while(0)

    REQ_NONNEG(remaining_micros, "x-nous-credits-remaining-micros");
    REQ_INT(subscription_micros,  "x-nous-credits-subscription-micros");
    REQ_NONNEG(rollover_micros,   "x-nous-credits-rollover-micros");
    REQ_NONNEG(purchased_micros,  "x-nous-credits-purchased-micros");
    REQ_NONNEG(as_of_ms,          "x-nous-credits-as-of-ms");

    /* tool_pool_micros is OPTIONAL: absent -> 0 */
    {
        const char *_tp = get_hdr("x-nous-tool-pool-micros");
        if (_tp) {
            int64_t _v;
            if (!safe_int(_tp, &_v) || _v < 0)
                return false;
            state->tool_pool_micros = _v;
        } else {
            state->tool_pool_micros = 0;
        }
    }

    #undef REQ_NONNEG
    #undef REQ_INT

    /* Step 5: Validate USD strings */
    {
        const char *usd = get_hdr("x-nous-credits-remaining-usd");
        if (!usd || !validate_usd(usd)) return false;
        strncpy(state->remaining_usd, usd, sizeof(state->remaining_usd) - 1);
        state->remaining_usd[sizeof(state->remaining_usd) - 1] = '\0';
    }
    {
        const char *usd = get_hdr("x-nous-credits-subscription-usd");
        if (!usd || !validate_usd(usd)) return false;
        strncpy(state->subscription_usd, usd, sizeof(state->subscription_usd) - 1);
        state->subscription_usd[sizeof(state->subscription_usd) - 1] = '\0';
    }
    {
        const char *usd = get_hdr("x-nous-credits-purchased-usd");
        if (!usd || !validate_usd(usd)) return false;
        strncpy(state->purchased_usd, usd, sizeof(state->purchased_usd) - 1);
        state->purchased_usd[sizeof(state->purchased_usd) - 1] = '\0';
    }

    /* Step 6: subscription_limit_* PAIRED + OPTIONAL */
    {
        const char *lm_raw = get_hdr("x-nous-credits-subscription-limit-micros");
        const char *lu_raw = get_hdr("x-nous-credits-subscription-limit-usd");
        if (lm_raw && lu_raw) {
            int64_t lm;
            if (!safe_int(lm_raw, &lm) || lm < 0)
                return false;
            if (!validate_usd(lu_raw))
                return false;
            state->subscription_limit_micros = lm;
            strncpy(state->subscription_limit_usd, lu_raw,
                    sizeof(state->subscription_limit_usd) - 1);
            state->subscription_limit_usd[sizeof(state->subscription_limit_usd) - 1] = '\0';
        }
        /* else: half-pair or both absent -> leave default (CREDITS_LIMIT_NONE + "") */
    }

    /* Step 7: denominator_kind */
    {
        const char *dk = get_hdr("x-nous-credits-denominator-kind");
        if (!dk) dk = "none";
        if (strcmp(dk, "subscription_cap") != 0 && strcmp(dk, "none") != 0)
            return false;
        strncpy(state->denominator_kind, dk, sizeof(state->denominator_kind) - 1);
        state->denominator_kind[sizeof(state->denominator_kind) - 1] = '\0';
    }

    /* Step 8: paid_access / tool_pool_gated_off */
    {
        const char *pa = get_hdr("x-nous-credits-paid-access");
        if (pa) {
            /* Strip whitespace and lowercase */
            const char *p = pa;
            while (*p == ' ' || *p == '\t') p++;
            if (strcasecmp(p, "true") == 0)
                state->paid_access = true;
            else if (strcasecmp(p, "false") == 0)
                state->paid_access = false;
            else
                return false;
        } else {
            state->paid_access = true; /* fail-open */
        }
    }
    {
        const char *tpg = get_hdr("x-nous-tool-pool-gated-off");
        if (tpg) {
            const char *p = tpg;
            while (*p == ' ' || *p == '\t') p++;
            if (strcasecmp(p, "true") == 0)
                state->tool_pool_gated_off = true;
            else if (strcasecmp(p, "false") == 0)
                state->tool_pool_gated_off = false;
            else
                return false;
        } else {
            state->tool_pool_gated_off = false;
        }
    }

    /* Step 9: disabled_reason (header omitted when null) */
    {
        const char *dr = get_hdr("x-nous-credits-disabled-reason");
        if (dr) {
            strncpy(state->disabled_reason, dr, sizeof(state->disabled_reason) - 1);
            state->disabled_reason[sizeof(state->disabled_reason) - 1] = '\0';
        } else {
            state->disabled_reason[0] = '\0';
        }
    }

    /* Step 10: Set metadata */
    state->version = (int32_t)version_val;
    state->captured_at = (double)time(NULL);
    state->from_header = true;

    return true;
}

/* ── evaluate_credits_notices ─────────────────────────────────────────── */

/* Port of Python: evaluate_credits_notices */
void credits_tracker_evaluate_notices(const credits_state_t *state,
                                       credits_latch_t *latch,
                                       agent_notice_t to_show[CREDITS_MAX_NOTICES],
                                       size_t *n_to_show,
                                       char to_clear[CREDITS_MAX_NOTICES][64],
                                       size_t *n_to_clear) {
    if (!state || !latch || !to_show || !n_to_show || !to_clear || !n_to_clear)
        return;

    *n_to_show = 0;
    *n_to_clear = 0;

    /* Helper: check if a key is active */
    #define LATCH_ACTIVE(key) ({ \
        bool _found = false; \
        for (int _i = 0; _i < latch->active_count; _i++) { \
            if (strcmp(latch->active_keys[_i], (key)) == 0) { \
                _found = true; \
                break; \
            } \
        } \
        _found; \
    })

    /* Helper: add a key to active set */
    #define LATCH_ADD(key) do { \
        if (latch->active_count < 16) { \
            strncpy(latch->active_keys[latch->active_count], (key), 63); \
            latch->active_keys[latch->active_count][63] = '\0'; \
            latch->active_count++; \
        } \
    } while(0)

    /* Helper: remove a key from active set */
    #define LATCH_REMOVE(key) do { \
        for (int _i = 0; _i < latch->active_count; _i++) { \
            if (strcmp(latch->active_keys[_i], (key)) == 0) { \
                for (int _j = _i; _j < latch->active_count - 1; _j++) \
                    memcpy(latch->active_keys[_j], latch->active_keys[_j + 1], 64); \
                latch->active_count--; \
                break; \
            } \
        } \
    } while(0)

    double uf = credits_state_used_fraction(state);

    /* Crossing latch: once observed uf below LOWEST band, open gate */
    if (uf >= 0.0 && uf < CREDITS_BAND_THRESHOLDS[0])
        latch->seen_below_50 = true;

    /* ── Conditions ──────────────────────────────────────────────────── */
    /* Current band (highest whose threshold is reached) */
    int current_band_idx = -1;
    if (uf >= 0.0) {
        for (int i = CREDITS_BAND_COUNT - 1; i >= 0; i--) {
            if (uf >= CREDITS_BAND_THRESHOLDS[i]) {
                current_band_idx = i;
                break;
            }
        }
    }

    bool grant_cond = (strcmp(state->denominator_kind, "subscription_cap") == 0
                       && uf >= 0.0 && uf >= 1.0
                       && state->purchased_micros > 0);

    bool depleted_cond = !state->paid_access;

    /* ── Usage gauge ─────────────────────────────────────────────────── */
    int target_band = -1;
    if (current_band_idx >= 0 && latch->seen_below_50)
        target_band = CREDITS_BAND_PCTS[current_band_idx];

    if (target_band != latch->usage_band) {
        /* Clear existing usage notice if active */
        if (LATCH_ACTIVE(CREDITS_USAGE_KEY)) {
            strncpy(to_clear[*n_to_clear], CREDITS_USAGE_KEY, 63);
            to_clear[*n_to_clear][63] = '\0';
            (*n_to_clear)++;
            LATCH_REMOVE(CREDITS_USAGE_KEY);
        }
        if (target_band >= 0) {
            /* Build cap_usd string */
            const char *cap_usd = state->subscription_limit_usd;
            if (!cap_usd || !cap_usd[0]) cap_usd = "?";
            const char *level = CREDITS_BAND_LEVELS[current_band_idx];
            const char *prefix = (strcmp(level, "warn") == 0) ? "⚠" : "•";
            agent_notice_t *n = &to_show[*n_to_show];
            snprintf(n->text, sizeof(n->text),
                     "%s Credits %d%% used · $%s cap",
                     prefix, target_band, cap_usd);
            strncpy(n->level, level, sizeof(n->level) - 1);
            strncpy(n->kind, CREDITS_NOTICE_KIND, sizeof(n->kind) - 1);
            strncpy(n->key, CREDITS_USAGE_KEY, sizeof(n->key) - 1);
            strncpy(n->id, CREDITS_USAGE_KEY, sizeof(n->id) - 1);
            (*n_to_show)++;
            LATCH_ADD(CREDITS_USAGE_KEY);
        }
        latch->usage_band = target_band;
    }

    /* ── Grant spent ─────────────────────────────────────────────────── */
    if (grant_cond && !LATCH_ACTIVE(CREDITS_GRANT_KEY)) {
        agent_notice_t *n = &to_show[*n_to_show];
        const char *purchased_usd = state->purchased_usd;
        if (!purchased_usd || !purchased_usd[0]) purchased_usd = "0.00";
        snprintf(n->text, sizeof(n->text),
                 "• Grant spent · $%s top-up left", purchased_usd);
        strncpy(n->level, "info", sizeof(n->level) - 1);
        strncpy(n->kind, CREDITS_NOTICE_KIND, sizeof(n->kind) - 1);
        strncpy(n->key, CREDITS_GRANT_KEY, sizeof(n->key) - 1);
        strncpy(n->id, CREDITS_GRANT_KEY, sizeof(n->id) - 1);
        (*n_to_show)++;
        LATCH_ADD(CREDITS_GRANT_KEY);
    } else if (LATCH_ACTIVE(CREDITS_GRANT_KEY) && !grant_cond) {
        strncpy(to_clear[*n_to_clear], CREDITS_GRANT_KEY, 63);
        to_clear[*n_to_clear][63] = '\0';
        (*n_to_clear)++;
        LATCH_REMOVE(CREDITS_GRANT_KEY);
    }

    /* ── Depleted ────────────────────────────────────────────────────── */
    if (depleted_cond && !LATCH_ACTIVE(CREDITS_DEPLETED)) {
        agent_notice_t *n = &to_show[*n_to_show];
        snprintf(n->text, sizeof(n->text),
                 "✕ Credit access paused · run /usage for balance");
        strncpy(n->level, "error", sizeof(n->level) - 1);
        strncpy(n->kind, CREDITS_NOTICE_KIND, sizeof(n->kind) - 1);
        strncpy(n->key, CREDITS_DEPLETED, sizeof(n->key) - 1);
        strncpy(n->id, CREDITS_DEPLETED, sizeof(n->id) - 1);
        (*n_to_show)++;
        LATCH_ADD(CREDITS_DEPLETED);
    } else if (LATCH_ACTIVE(CREDITS_DEPLETED) && !depleted_cond) {
        strncpy(to_clear[*n_to_clear], CREDITS_DEPLETED, 63);
        to_clear[*n_to_clear][63] = '\0';
        (*n_to_clear)++;
        LATCH_REMOVE(CREDITS_DEPLETED);
        /* Recovery: also emit the success notice */
        agent_notice_t *n = &to_show[*n_to_show];
        snprintf(n->text, sizeof(n->text),
                 "✓ Credit access restored");
        strncpy(n->level, "success", sizeof(n->level) - 1);
        strncpy(n->kind, "ttl", sizeof(n->kind) - 1);
        n->ttl_ms = CREDITS_RESTORED_TTL_MS;
        strncpy(n->key, CREDITS_RESTORED, sizeof(n->key) - 1);
        strncpy(n->id, CREDITS_RESTORED, sizeof(n->id) - 1);
        (*n_to_show)++;
    }

    #undef LATCH_ACTIVE
    #undef LATCH_ADD
    #undef LATCH_REMOVE
}

/* PoP: credits_state_depleted @ credits_tracker:depleted */
/* (implementation in include/hermes_credits_tracker.h as static inline) */

/* ── Latch init ───────────────────────────────────────────────────────── */

/* PoP: new_credits_latch @ agent/credits_tracker.py:new_credits_latch */
/* Initialize a credits_latch_t to the fresh notice latch shape that
 * evaluate_credits_notices expects: active=empty, seen_below_90=false,
 * usage_band=unset(-1), seen_grant_unspent=false. */
void credits_latch_init(credits_latch_t *latch) {
    if (!latch) return;
    memset(latch, 0, sizeof(*latch));
    latch->usage_band = -1;
}

/* ── dev_fixture_credits_state ────────────────────────────────────────── */

/* Port of Python: dev_fixture_credits_state */
bool credits_tracker_dev_fixture(credits_state_t *state) {
    if (!state) return false;

    /* Hard prod-leak guard: HERMES_DEV_CREDITS must also be set */
    const char *dev_flag = getenv("HERMES_DEV_CREDITS");
    if (!dev_flag || !dev_flag[0]) return false;

    /* Check for truthy value */
    const char *p = dev_flag;
    while (*p == ' ' || *p == '\t') p++;
    if (strcmp(p, "0") == 0 || strcmp(p, "false") == 0 ||
        strcmp(p, "no") == 0 || strcmp(p, "off") == 0 || strcmp(p, "") == 0)
        return false;

    const char *raw = getenv("HERMES_DEV_CREDITS_FIXTURE");
    if (!raw || !raw[0]) return false;

    /* Skip leading whitespace */
    while (*raw == ' ' || *raw == '\t') raw++;
    if (!raw[0]) return false;

    /* Check if it's a file path (contains / or \\) */
    char name_buf[256];
    const char *name = raw;
    if (strchr(raw, '/') || strchr(raw, '\\')) {
        /* Read name from file */
        FILE *fh = fopen(raw, "r");
        if (!fh) return false;
        if (!fgets(name_buf, sizeof(name_buf), fh)) {
            fclose(fh);
            return false;
        }
        fclose(fh);
        /* Strip trailing newline/whitespace */
        size_t nlen = strlen(name_buf);
        while (nlen > 0 && (name_buf[nlen - 1] == '\n' ||
               name_buf[nlen - 1] == '\r' || name_buf[nlen - 1] == ' '))
            name_buf[--nlen] = '\0';
        name = name_buf;
    }

    /* Find the fixture by name (case-insensitive) */
    credits_state_init(state);
    for (size_t i = 0; i < CREDITS_DEV_FIXTURE_COUNT; i++) {
        if (strcasecmp(name, CREDITS_DEV_FIXTURES[i].name) == 0) {
            state->version = 1;
            state->remaining_micros = CREDITS_DEV_FIXTURES[i].remaining_micros;
            strncpy(state->remaining_usd, CREDITS_DEV_FIXTURES[i].remaining_usd,
                    sizeof(state->remaining_usd) - 1);
            state->subscription_micros = CREDITS_DEV_FIXTURES[i].subscription_micros;
            strncpy(state->subscription_usd, CREDITS_DEV_FIXTURES[i].subscription_usd,
                    sizeof(state->subscription_usd) - 1);
            if (CREDITS_DEV_FIXTURES[i].subscription_limit_micros != CREDITS_LIMIT_NONE) {
                state->subscription_limit_micros = CREDITS_DEV_FIXTURES[i].subscription_limit_micros;
                strncpy(state->subscription_limit_usd,
                        CREDITS_DEV_FIXTURES[i].subscription_limit_usd,
                        sizeof(state->subscription_limit_usd) - 1);
            }
            state->purchased_micros = CREDITS_DEV_FIXTURES[i].purchased_micros;
            strncpy(state->purchased_usd, CREDITS_DEV_FIXTURES[i].purchased_usd,
                    sizeof(state->purchased_usd) - 1);
            strncpy(state->denominator_kind, CREDITS_DEV_FIXTURES[i].denominator_kind,
                    sizeof(state->denominator_kind) - 1);
            state->paid_access = CREDITS_DEV_FIXTURES[i].paid_access;
            if (CREDITS_DEV_FIXTURES[i].disabled_reason[0])
                strncpy(state->disabled_reason, CREDITS_DEV_FIXTURES[i].disabled_reason,
                        sizeof(state->disabled_reason) - 1);
            state->rollover_micros = CREDITS_DEV_FIXTURES[i].rollover_micros;
            state->captured_at = (double)time(NULL);
            state->from_header = true;
            return true;
        }
    }
    return false;
}
