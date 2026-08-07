/**
 * @file hermes_credits_tracker.h
 * @brief Credits tracking for Nous inference API responses.
 *
 * Port of Python agent/credits_tracker.py (724 lines).
 * Parses x-nous-credits-* headers into credits_state_t, evaluates notices.
 *
 * @{
 */
#ifndef HERMES_CREDITS_TRACKER_H
#define HERMES_CREDITS_TRACKER_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Credits state ─────────────────────────────────────────────────────── */

/** Full credits state parsed from x-nous-credits-* response headers. */
typedef struct credits_state {
    int32_t     version;                    /* contract/schema version */
    int64_t     remaining_micros;           /* total remaining balance (micros) */
    char        remaining_usd[32];          /* same, formatted USD string */
    int64_t     subscription_micros;        /* SIGNED — may be negative (debt) */
    char        subscription_usd[32];
    int64_t     subscription_limit_micros;  /* -1 = None (absent) */
    char        subscription_limit_usd[32]; /* empty = None */
    int64_t     rollover_micros;
    int64_t     purchased_micros;
    char        purchased_usd[32];
    int64_t     tool_pool_micros;
    bool        tool_pool_gated_off;
    char        denominator_kind[32];       /* "subscription_cap" | "none" */
    bool        paid_access;                /* depletion keys off THIS == false */
    char        disabled_reason[256];       /* empty when None (header omitted) */
    int64_t     as_of_ms;                   /* server-side timestamp (ms epoch) */
    double      captured_at;                /* time() when this was captured */
    bool        from_header;                /* true only when populated by parse */
} credits_state_t;

/** Sentinel value for subscription_limit_micros meaning "not present". */
#define CREDITS_LIMIT_NONE  (-1)

/* ── Agent notice ──────────────────────────────────────────────────────── */

/** Out-of-band notice payload (driver-agnostic). */
typedef struct agent_notice {
    char    text[512];
    char    level[16];        /* info | warn | error | success */
    char    kind[16];         /* sticky | ttl */
    int     ttl_ms;           /* honored only when kind == "ttl" */
    char    key[64];          /* dedupe / fired-once-latch / clear key */
    char    id[64];
} agent_notice_t;

/* ── Credits latch (reconciliation state) ─────────────────────────────── */

/** Per-agent latch for evaluate_credits_notices idempotency. */
typedef struct credits_latch {
    bool active[16];              /* which notice keys are active */
    char active_keys[16][64];     /* notice key strings */
    int  active_count;            /* number of active keys */
    bool seen_below_50;           /* gate: observed uf < lowest band */
    int  usage_band;              /* currently displayed usage band pct, or -1 */
} credits_latch_t;

/** Maximum number of notices that can be returned in one evaluation. */
#define CREDITS_MAX_NOTICES 8

/* ── Credits policy constants ─────────────────────────────────────────── */

#define CREDITS_NOTICE_KIND      "sticky"
#define CREDITS_RESTORED_TTL_MS  8000

/* Usage-gauge bands: threshold_fraction, level, label_pct */
#define CREDITS_BAND_COUNT 3
extern const double   CREDITS_BAND_THRESHOLDS[CREDITS_BAND_COUNT];
extern const char    *CREDITS_BAND_LEVELS[CREDITS_BAND_COUNT];
extern const int      CREDITS_BAND_PCTS[CREDITS_BAND_COUNT];

#define CREDITS_USAGE_KEY  "credits.usage"
#define CREDITS_GRANT_KEY  "credits.grant_spent"
#define CREDITS_DEPLETED   "credits.depleted"
#define CREDITS_RESTORED   "credits.restored"

/* ── CreditsState computed properties ─────────────────────────────────── */

/** Returns true if the state has been populated (captured_at > 0). */
static inline bool credits_state_has_data(const credits_state_t *s) {
    return s->captured_at > 0.0;
}

/** Returns age in seconds since capture, or INFINITY if no data. */
static inline double credits_state_age_seconds(const credits_state_t *s) {
    if (!credits_state_has_data(s)) return 1.0 / 0.0; /* INFINITY */
    return ((double)time(NULL)) - s->captured_at;
}

/**
 * Returns true when the account has lost paid access.
 * AG26: Port of Python agent/credits_tracker.py:depleted().
 */
static inline bool credits_state_depleted(const credits_state_t *s) {
    return !s->paid_access;
}

/**
 * Fraction of the subscription cap consumed, in [0.0, 1.0].
 * Returns -1.0 when there is no computable denominator.
 * AG26: Port of Python agent/credits_tracker.py:used_fraction().
 */
double credits_state_used_fraction(const credits_state_t *s);

/* ── CreditsState constructor ─────────────────────────────────────────── */

/** Initialize a credits_state_t to default/zero values. */
void credits_state_init(credits_state_t *s);

/* ── Parsing ──────────────────────────────────────────────────────────── */

/**
 * Parse a header value to an exact int64_t (money-safe).
 * Returns true on success, false if value is not a valid integer string.
 * Never uses float conversion — avoids precision loss above 2^53.
 */
bool safe_int(const char *value, int64_t *out);

/**
 * Validate a USD string: matches ^-?\d+\.\d{2}$.
 * Returns true if value is non-NULL and matches the pattern.
 */
bool validate_usd(const char *value);

/**
 * Parse x-nous-credits-* headers into a credits_state_t.
 *
 * @param state     Output: populated credits_state (valid only on true return)
 * @param headers   Array of "Name: Value" header strings, NULL-terminated
 * @param provider  Provider name string (unused in parsing, for logging)
 * @return          true if parsing succeeded, false on any validation failure
 */
bool credits_tracker_parse_headers(credits_state_t *state,
                                   const char *const *headers,
                                   const char *provider);

/* ── Notice evaluation ────────────────────────────────────────────────── */

/**
 * Reconcile credits notices against the latch. Mutates latch IN PLACE.
 *
 * @param state     Current credits state
 * @param latch     Latch/state for idempotency (mutated in place)
 * @param to_show   Output array of notices to emit (caller must clear first)
 * @param n_to_show Output: number of notices in to_show
 * @param to_clear  Output array of notice key strings to clear
 * @param n_to_clear Output: number of keys in to_clear
 */
void credits_tracker_evaluate_notices(const credits_state_t *state,
                                       credits_latch_t *latch,
                                       agent_notice_t to_show[CREDITS_MAX_NOTICES],
                                       size_t *n_to_show,
                                       char to_clear[CREDITS_MAX_NOTICES][64],
                                       size_t *n_to_clear);

/* ── Latch initialization ─────────────────────────────────────────────── */

/* PoP: new_credits_latch @ agent/credits_tracker.py:new_credits_latch */
void credits_latch_init(credits_latch_t *latch);

/* ── Dev fixture ──────────────────────────────────────────────────────── */

/**
 * Return a fixture credits_state for HERMES_DEV_CREDITS_FIXTURE.
 * Returns true and populates state when a valid fixture is active.
 * Returns false when no fixture (normal behaviour).
 *
 * Hard guard: HERMES_DEV_CREDITS must also be set.
 */
bool credits_tracker_dev_fixture(credits_state_t *state);

#ifdef __cplusplus
}
#endif

/** @} */
#endif /* HERMES_CREDITS_TRACKER_H */
