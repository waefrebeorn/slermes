/*
 * auxiliary_rate_limit_backoff.c — C home for upstream Hermes agent/auxiliary_rate_limit_backoff.py.
 * C11 behavior port of the same-credential rate-limit ladder's wait schedule
 * (gate w25_auxiliaryr_1beb43).
 *
 * PoP: w25_auxiliaryr_1beb43_retry_after @ agent/auxiliary_rate_limit_backoff.py:rate_limit_retry_after
 * PoP: w25_auxiliaryr_1beb43_wait @ agent/auxiliary_rate_limit_backoff.py:rate_limit_wait
 * PoP: w25_auxiliaryr_1beb43_constants @ agent/auxiliary_rate_limit_backoff.py:<module constants>
 *
 * The ladder driver (yield/generator rungs), the client re-send, the sleep and
 * the logger are seams.  What is kept here is the part that is a pure function
 * of values: which provider-declared cooldown an exception carries, whether it
 * is short enough to sit through, and the jittered exponential wait otherwise.
 * The one non-deterministic input (random.Random seeded from time + counter in
 * retry_utils.jittered_backoff) is a parameter.
 */
#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w25_auxiliaryr_1beb43.h"

/* Module-level literal constants mirrored from the Python source. */
static long long agent_auxiliary_rate_limit_backoff_CONST_RATE_LIMIT_RETRIES = 1;

/* RATE_LIMIT_BACKOFF_BASE / _CAP / RATE_LIMIT_RETRY_AFTER_CAP */
#define AUX_BACKOFF_BASE 1.0
#define AUX_BACKOFF_CAP 4.0
#define AUX_RETRY_AFTER_CAP 5.0

static int aux_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

/* retry_utils.parse_retry_after_seconds for the shapes an exception carries:
 * a number, a numeric string, or a headers mapping (both casings tried).
 * Returns 1 with *out clamped at 0.0 when a value was parseable, 0 when absent.
 * HTTP-date form is not ported: it needs "now" (a clock seam), so an
 * unparseable string is treated as absent. */
static int aux_parse_after_depth(const json_t *raw, double *out, int depth) {
    if (!raw) return 0;
    switch (raw->type) {
    case JSON_BOOL:
        return 0; /* isinstance(raw, bool) -> None */
    case JSON_NUMBER:
        *out = raw->num_val > 0.0 ? raw->num_val : 0.0;
        return 1;
    case JSON_STRING: {
        const char *s = raw->str_val ? raw->str_val : "";
        const char *endp = NULL;
        char *parse_end = NULL;
        double v;
        while (*s && aux_ws(*s)) s++;
        endp = s + strlen(s);
        while (endp > s && aux_ws(endp[-1])) endp--;
        if (endp == s) return 0;
        {
            size_t len = (size_t)(endp - s);
            char *buf = (char *)malloc(len + 1);
            if (!buf) return 0;
            memcpy(buf, s, len);
            buf[len] = '\0';
            v = strtod(buf, &parse_end);
            if (parse_end == buf || *parse_end != '\0') {
                free(buf);
                return 0;
            }
            free(buf);
        }
        *out = v > 0.0 ? v : 0.0;
        return 1;
    }
    case JSON_OBJECT: {
        const json_t *h;
        if (depth > 0) return 0; /* str(dict) is not a number nor an HTTP-date */
        h = json_obj_get(raw, "Retry-After");
        if (!h) h = json_obj_get(raw, "retry-after");
        if (!h) return 0;
        return aux_parse_after_depth(h, out, depth + 1);
    }
    default:
        return 0; /* null, array: no .get, no number */
    }
}

static int aux_parse_after(const json_t *raw, double *out) {
    return aux_parse_after_depth(raw, out, 0);
}

/* retry_utils.provider_retry_after_seconds: the response header first, else a
 * ``retry_after`` body field (top level or nested under ``error``).  A parsed
 * zero is *present* and therefore stops the body fallback, exactly as the
 * ``if value is None`` guard upstream. */
static int aux_provider_after(const json_t *exc, double *out) {
    const json_t *body, *nested, *src;
    if (!exc || exc->type != JSON_OBJECT) return 0;
    if (aux_parse_after(json_obj_get(exc, "headers"), out)) return *out > 0.0;
    body = json_obj_get(exc, "body");
    if (body && body->type == JSON_OBJECT) {
        nested = json_obj_get(body, "error");
        src = (nested && nested->type == JSON_OBJECT) ? nested : body;
        if (aux_parse_after(json_obj_get(src, "retry_after"), out)) return *out > 0.0;
    }
    return 0;
}

/* rate_limit_retry_after: provider value, else the botocore response dict's
 * ResponseMetadata.HTTPHeaders; > 0 only.  The two are exclusive: upstream
 * only reaches the botocore branch when ``isinstance(exc.response, dict)``,
 * i.e. when the response was not the SDK object carrying ``headers``. */
static int aux_declared_wait(const json_t *exc, double *out) {
    const json_t *meta;
    double v;
    if (!exc || exc->type != JSON_OBJECT) return 0;
    if (aux_provider_after(exc, out)) return 1;
    if (json_obj_get(exc, "headers")) return 0; /* response is not the botocore dict */
    meta = json_obj_get(exc, "metadata");
    if (!meta) return 0;
    if (!aux_parse_after(meta, &v)) return 0;
    if (v <= 0.0) return 0;
    *out = v;
    return 1;
}

/* retry_utils.jittered_backoff: min(base * 2^(attempt-1), cap) + one uniform
 * draw in [0, jitter_ratio * delay], jitter_ratio 0.5. */
static double aux_backoff(double attempt, double jitter) {
    double exponent = attempt - 1.0;
    double delay;
    if (!(exponent >= 0.0)) exponent = 0.0; /* max(0, attempt - 1) */
    if (exponent >= 63.0 || AUX_BACKOFF_BASE <= 0.0) {
        delay = AUX_BACKOFF_CAP;
    } else {
        double grown = AUX_BACKOFF_BASE * ldexp(1.0, (int)exponent);
        delay = grown < AUX_BACKOFF_CAP ? grown : AUX_BACKOFF_CAP;
    }
    return delay + jitter * (0.5 * delay);
}

json_t *w25_auxiliaryr_1beb43_retry_after(const json_t *exc) {
    double declared;
    if (!aux_declared_wait(exc, &declared)) return json_null();
    return json_number(declared);
}

json_t *w25_auxiliaryr_1beb43_wait(const json_t *exc, double attempt, double jitter) {
    double declared;
    if (aux_declared_wait(exc, &declared)) {
        if (declared <= AUX_RETRY_AFTER_CAP) return json_number(declared);
        return json_null(); /* a cooldown this long is not worth sitting through */
    }
    return json_number(aux_backoff(attempt, jitter));
}

json_t *w25_auxiliaryr_1beb43_constants(void) {
    json_t *o = json_object();
    json_set(o, "retries",
             json_number((double)agent_auxiliary_rate_limit_backoff_CONST_RATE_LIMIT_RETRIES));
    json_set(o, "backoff_base", json_number(AUX_BACKOFF_BASE));
    json_set(o, "backoff_cap", json_number(AUX_BACKOFF_CAP));
    json_set(o, "retry_after_cap", json_number(AUX_RETRY_AFTER_CAP));
    return o;
}
