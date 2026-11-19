/*
 * port_model_metadata.c -- see port_model_metadata.h.
 */

#include "port_model_metadata.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define MM_GROK_EFFORT_PREFIXES                                                      \
    "grok-3-mini", "grok-4.20-multi-agent", "grok-4.3", "grok-4.5", "grok-4.6"

static const char *const MM_FAST_MODE[] = {
    "claude-opus-4-8", "claude-opus-5", "claude-opus-5-5", NULL
};

static const char *const MM_LOCAL_HOSTS[] = {
    "localhost", "127.0.0.1", "::1", "0.0.0.0", NULL
};
static const char *const MM_CONTAINER_SUFFIXES[] = {
    ".docker.internal", ".containers.internal", ".lima.internal", NULL
};

static int startswith_ci(const char *s, const char *p)
{
    while (*p) {
        if (!*s || tolower((unsigned char)*s) != tolower((unsigned char)*p))
            return 0;
        s++;
        p++;
    }
    return 1;
}

static int endswith(const char *s, const char *suffix)
{
    size_t ls = strlen(s), lf = strlen(suffix);
    return ls >= lf && strcmp(s + ls - lf, suffix) == 0;
}

/* last path segment after the final '/' */
static const char *last_segment(const char *s)
{
    const char *slash = strrchr(s, '/');
    return slash ? slash + 1 : s;
}

/* ===========================================================================
 *  _normalize_base_url  (model_metadata.py:426)
 * =========================================================================== */

/* PoP: mm_normalize_base_url @ agent/model_metadata.py:_normalize_base_url */
char *mm_normalize_base_url(const char *base_url)
{
    if (!base_url)
        return strdup("");
    const char *s = base_url;
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r')
        s++;
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\n'
                     || s[n - 1] == '\r'))
        n--;
    while (n > 0 && s[n - 1] == '/')
        n--;
    char *out = malloc(n + 1);
    if (!out)
        return NULL;
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

/* ===========================================================================
 *  grok_supports_reasoning_effort  (model_metadata.py:374)
 *      name = (model or "").strip().lower().rsplit("/", 1)[-1]
 *      return bool(name) and any(name.startswith(prefix) for prefix in ...)
 * =========================================================================== */

/* PoP: mm_grok_supports_reasoning_effort @ agent/model_metadata.py:grok_supports_reasoning_effort */
int mm_grok_supports_reasoning_effort(const char *model)
{
    if (!model)
        return 0;
    const char *s = model;
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r')
        s++;
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\n'
                     || s[n - 1] == '\r'))
        n--;
    const char *name = last_segment(s);
    size_t nl = strlen(name);
    /* recompute the segment length within the trimmed buffer */
    (void)nl;
    char *low = malloc(n + 1);
    if (!low)
        return 0;
    for (size_t i = 0; i < n; i++)
        low[i] = (char)tolower((unsigned char)s[i]);
    low[n] = '\0';
    const char *seg = last_segment(low);
    int hit = (*seg != '\0');
    static const char *const prefixes[] = { MM_GROK_EFFORT_PREFIXES, NULL };
    if (hit) {
        hit = 0;
        for (int i = 0; prefixes[i] && !hit; i++)
            hit = startswith_ci(seg, prefixes[i]);
    }
    free(low);
    return hit;
}

/* ===========================================================================
 *  openai_model_rejects_reasoning  (model_metadata.py:388)
 *      _OPENAI_NON_REASONING_RE = r"^(?:ft:)?(?:gpt-3\.5|gpt-4(?![0-9])|chatgpt-)"
 * =========================================================================== */

/* PoP: mm_openai_model_rejects_reasoning @ agent/model_metadata.py:openai_model_rejects_reasoning */
int mm_openai_model_rejects_reasoning(const char *model)
{
    if (!model)
        return 0;
    const char *s = model;
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r')
        s++;
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\n'
                     || s[n - 1] == '\r'))
        n--;
    char *low = malloc(n + 1);
    if (!low)
        return 0;
    for (size_t i = 0; i < n; i++)
        low[i] = (char)tolower((unsigned char)s[i]);
    low[n] = '\0';
    const char *name = last_segment(low);

    int hit = 0;
    const char *p = name;
    if (strncmp(p, "ft:", 3) == 0)
        p += 3;
    if (strncmp(p, "gpt-3.5", 7) == 0)
        hit = 1;
    else if (strncmp(p, "gpt-4", 5) == 0 && !(p[5] >= '0' && p[5] <= '9'))
        hit = 1;                                    /* gpt-4 but not gpt-40 */
    else if (strncmp(p, "chatgpt-", 8) == 0)
        hit = 1;
    free(low);
    return hit;
}

/* ===========================================================================
 *  is_grok_46_family  (model_metadata.py:395)
 *      name = (model or "").strip().lower().replace("_", "-").rsplit("/", 1)[-1]
 *      return name == "grok-4.6" or name.startswith("grok-4.6-")
 * =========================================================================== */

/* PoP: mm_is_grok_46_family @ agent/model_metadata.py:is_grok_46_family */
int mm_is_grok_46_family(const char *model)
{
    if (!model)
        return 0;
    const char *s = model;
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r')
        s++;
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\n'
                     || s[n - 1] == '\r'))
        n--;
    char *low = malloc(n + 1);
    if (!low)
        return 0;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        low[i] = (char)(c == '_' ? '-' : tolower(c));
    }
    low[n] = '\0';
    const char *name = last_segment(low);
    int hit = (strcmp(name, "grok-4.6") == 0 || startswith_ci(name, "grok-4.6-"));
    free(low);
    return hit;
}

/* ===========================================================================
 *  is_anthropic_fast_mode_model  (model_metadata.py:407)
 *      name = str(model or "").strip().lower().split(":", 1)[0].rsplit("/", 1)[-1]
 *      name = re.sub(r"(\d)\.(\d)", r"\1-\2", name)
 *      return re.sub(r"-\d{8}$", "", name) in _ANTHROPIC_FAST_MODE_MODELS
 * =========================================================================== */

static int str_in(const char *const *set, const char *v)
{
    for (int i = 0; set[i]; i++)
        if (strcmp(set[i], v) == 0)
            return 1;
    return 0;
}

/* PoP: mm_is_anthropic_fast_mode_model @ agent/model_metadata.py:is_anthropic_fast_mode_model */
int mm_is_anthropic_fast_mode_model(const char *model)
{
    if (!model)
        return 0;
    const char *s = model;
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r')
        s++;
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\n'
                     || s[n - 1] == '\r'))
        n--;
    /* split(":", 1)[0] -- the part before the first colon */
    const char *colon = memchr(s, ':', n);
    if (colon)
        n = (size_t)(colon - s);
    char *low = malloc(n + 1);
    if (!low)
        return 0;
    for (size_t i = 0; i < n; i++)
        low[i] = (char)tolower((unsigned char)s[i]);
    low[n] = '\0';

    const char *name = last_segment(low);
    char work[256];
    snprintf(work, sizeof(work), "%s", name);

    /* (\d)\.(\d) -> \1-\2 */
    {
        char tmp[256];
        size_t k = 0;
        for (size_t i = 0; work[i] && k < sizeof(tmp) - 1; i++) {
            if (work[i] == '.' && i > 0 && (work[i - 1] >= '0' && work[i - 1] <= '9')
                && work[i + 1] >= '0' && work[i + 1] <= '9') {
                /* work[i-1] was already emitted on the previous iteration */
                tmp[k++] = '-';
                i++;                            /* skip the dot */
                tmp[k++] = work[i];
            } else {
                tmp[k++] = work[i];
            }
        }
        tmp[k] = '\0';
        snprintf(work, sizeof(work), "%s", tmp);
    }

    /* -\d{8}$ -> "" */
    size_t wl = strlen(work);
    if (wl >= 9 && work[wl - 9] == '-') {
        int digits = 1;
        for (size_t i = wl - 8; i < wl; i++)
            if (!(work[i] >= '0' && work[i] <= '9'))
                digits = 0;
        if (digits)
            work[wl - 9] = '\0';
    }

    int hit = str_in(MM_FAST_MODE, work);
    free(low);
    return hit;
}

/* ===========================================================================
 *  _catalog_key_matches  (model_metadata.py:548)
 *
 *      if key.startswith(_BOUNDARY_MATCHED_KEY_PREFIXES):        # "solar-"
 *          model_lower = _HYPHENATED_GENERATION_RE.sub(r"\1\2", model_lower)
 *          if key.endswith("-"):
 *              return re.match(re.escape(key) + "[a-z]", last_segment) is not None
 *          return re.search(re.escape(key) + r"(?:[-:.@]|$)", model_lower) is not None
 *      return key in model_lower or _normalize_model_version(key) in \
 *             _normalize_model_version(model_lower)
 * =========================================================================== */

static int contains(const char *hay, const char *needle)
{
    return needle[0] && strstr(hay, needle) != NULL;
}

/* PoP: mm_catalog_key_matches @ agent/model_metadata.py:_catalog_key_matches */
int mm_catalog_key_matches(const char *key, const char *model_lower)
{
    if (!key || !model_lower)
        return 0;
    if (startswith_ci(key, "solar-")) {
        /* ((solar-)[a-z]+)-(\d{1,2})(?=[-:.@]|$) -> \1\2 */
        char folded[512];
        size_t k = 0;
        size_t n = strlen(model_lower);
        for (size_t i = 0; i < n && k < sizeof(folded) - 1; i++) {
            folded[k++] = model_lower[i];
            if (model_lower[i] == '-'
                && i >= 7 && strncmp(model_lower + i - 6, "solar-", 6) == 0) {
                /* count trailing [a-z] before this dash: the regex needs one */
                size_t j = i - 1;
                size_t letters = 0;
                while (j > 0 && islower((unsigned char)model_lower[j])) {
                    letters++;
                    j--;
                }
                if (letters >= 1) {
                    size_t d = 0;
                    size_t q = i + 1;
                    while (q < n && model_lower[q] >= '0' && model_lower[q] <= '9' && d < 2) {
                        q++;
                        d++;
                    }
                    if (d >= 1 && (q >= n || strchr("-:.@", model_lower[q]))) {
                        /* drop the dash */
                        k--;
                    }
                }
            }
        }
        folded[k] = '\0';

        size_t kl = strlen(key);
        if (kl > 0 && key[kl - 1] == '-') {
            /* re.match(key + "[a-z]", last segment) */
            const char *seg = last_segment(folded);
            if (strncmp(seg, key, kl) == 0 && islower((unsigned char)seg[kl]))
                return 1;
            return 0;
        }
        /* re.search(key + "(?:[-:.@]|$)", folded) */
        const char *p = folded;
        while ((p = strstr(p, key)) != NULL) {
            char nx = p[kl];
            if (nx == '\0' || strchr("-:.@", nx))
                return 1;
            p += kl;
        }
        return 0;
    }

    if (contains(model_lower, key))
        return 1;
    /* _normalize_model_version: dots -> dashes on both sides */
    char nk[256], nm[512];
    size_t a = 0;
    for (const char *p = key; *p && a < sizeof(nk) - 1; p++)
        nk[a++] = (*p == '.') ? '-' : *p;
    nk[a] = '\0';
    size_t b = 0;
    for (const char *p = model_lower; *p && b < sizeof(nm) - 1; p++)
        nm[b++] = (*p == '.') ? '-' : *p;
    nm[b] = '\0';
    return contains(nm, nk);
}

/* ===========================================================================
 *  _longest_key_match  (model_metadata.py:561)
 *      for key, value in sorted(table.items(), key=lambda x: len(x[0]), reverse=True)
 * =========================================================================== */

/* PoP: mm_longest_key_match @ agent/model_metadata.py:_longest_key_match */
json_t *mm_longest_key_match(const json_t *table, const char *model_lower,
                             char *key_out, size_t key_cap)
{
    if (!table || table->type != JSON_OBJECT || table->c.count == 0)
        return NULL;
    if (key_out && key_cap)
        key_out[0] = '\0';

    /* Selection sort by descending key length; a stable sort keeps table order
     * on ties, which is what Python's sorted() does. */
    size_t n = table->c.count;
    size_t *order = malloc(n * sizeof(size_t));
    if (!order)
        return NULL;
    for (size_t i = 0; i < n; i++)
        order[i] = i;
    for (size_t i = 0; i < n; i++) {
        size_t best = i;
        size_t best_len = table->c.keys[order[i]] ? strlen(table->c.keys[order[i]]) : 0;
        for (size_t j = i + 1; j < n; j++) {
            size_t lj = table->c.keys[order[j]] ? strlen(table->c.keys[order[j]]) : 0;
            if (lj > best_len) {
                best = j;
                best_len = lj;
            }
        }
        size_t tmp = order[i];
        order[i] = order[best];
        order[best] = tmp;
    }

    json_t *hit = NULL;
    for (size_t i = 0; i < n && !hit; i++) {
        const char *k = table->c.keys[order[i]];
        if (k && mm_catalog_key_matches(k, model_lower)) {
            hit = table->c.items[order[i]];
            if (key_out && key_cap) {
                snprintf(key_out, key_cap, "%s", k);
            }
        }
    }
    free(order);
    return hit;                              /* borrowed */
}

/* ===========================================================================
 *  _localhost_to_ipv4  (model_metadata.py:711)
 *      re.sub(r"^(https?://)localhost(?=[:/]|$)", r"\g<1>127.0.0.1", url, count=1)
 * =========================================================================== */

/* PoP: mm_localhost_to_ipv4 @ agent/model_metadata.py:_localhost_to_ipv4 */
char *mm_localhost_to_ipv4(const char *url)
{
    if (!url)
        return strdup("");
    const char *scheme = NULL;
    /* Python's pattern has no IGNORECASE, so the scheme match is case-sensitive. */
    if (strncmp(url, "http://", 7) == 0)
        scheme = "http://";
    else if (strncmp(url, "https://", 8) == 0)
        scheme = "https://";
    if (!scheme)
        return strdup(url);
    size_t sl = strlen(scheme);
    if (strcmp(url + sl, "localhost") != 0
        && strncmp(url + sl, "localhost:", 10) != 0
        && strncmp(url + sl, "localhost/", 10) != 0)
        return strdup(url);
    size_t n = strlen(url);
    char *out = malloc(n + 16);
    if (!out)
        return NULL;
    memcpy(out, url, sl);
    memcpy(out + sl, "127.0.0.1", 9);
    strcpy(out + sl + 9, url + sl + 9);
    return out;
}

/* ===========================================================================
 *  _server_root / _lmstudio_server_root  (model_metadata.py:535 / 526)
 * =========================================================================== */

/* PoP: mm_server_root @ agent/model_metadata.py:_server_root */
char *mm_server_root(const char *base_url)
{
    char *norm = mm_normalize_base_url(base_url);
    if (!norm)
        return NULL;
    char *v4 = mm_localhost_to_ipv4(norm);
    free(norm);
    if (!v4)
        return NULL;
    size_t n = strlen(v4);
    if (n >= 3 && strcmp(v4 + n - 3, "/v1") == 0)
        v4[n - 3] = '\0';
    return v4;
}

/* PoP: mm_lmstudio_server_root @ agent/model_metadata.py:_lmstudio_server_root */
char *mm_lmstudio_server_root(const char *base_url)
{
    char *root = mm_normalize_base_url(base_url);
    if (!root)
        return NULL;
    static const char *const suffixes[] = { "/api/v1", "/api", "/v1", NULL };
    for (int i = 0; suffixes[i]; i++) {
        size_t sl = strlen(suffixes[i]);
        if (endswith(root, suffixes[i])) {
            root[strlen(root) - sl] = '\0';
            size_t n = strlen(root);
            while (n > 0 && root[n - 1] == '/')
                root[--n] = '\0';
            return root;
        }
    }
    return root;
}

/* ===========================================================================
 *  is_local_endpoint  (model_metadata.py:677)
 * =========================================================================== */

/* Parse a dotted-quad; returns 0 when it is not one. */
static int parse_ipv4(const char *h, unsigned *a, unsigned *b, unsigned *c, unsigned *d)
{
    char extra;
    if (sscanf(h, "%u.%u.%u.%u%c", a, b, c, d, &extra) != 4)
        return 0;
    return *a < 256 && *b < 256 && *c < 256 && *d < 256;
}

static int ipv6_class(const char *h)
{
    if (strcmp(h, "::1") == 0 || strcmp(h, "::") == 0)
        return 1;                            /* loopback / unspecified */
    if (startswith_ci(h, "fe80"))
        return 2;                            /* link-local */
    if (h[0] == 'f' && (h[1] == 'c' || h[1] == 'd'))
        return 3;                            /* unique local */
    if (strcmp(h, "fd00::1") == 0)
        return 3;
    return 0;
}

/* PoP: mm_is_local_endpoint @ agent/model_metadata.py:is_local_endpoint */
int mm_is_local_endpoint(const char *base_url)
{
    char *norm = mm_normalize_base_url(base_url);
    if (!norm)
        return 0;
    /* hostname = urlparse(...).hostname; None when unparseable */
    const char *host = NULL;
    char hostbuf[256];
    const char *p = strstr(norm, "://");
    p = p ? p + 3 : norm;
    const char *end = p;
    while (*end && *end != '/' && *end != '?' && *end != '#')
        end++;
    {
        const char *at = NULL;
        for (const char *q = p; q < end; q++)
            if (*q == '@') at = q;
        if (at)
            p = at + 1;
    }
    if (*p == '[') {
        const char *close = strchr(p, ']');
        if (!close) {
            free(norm);
            return 0;
        }
        size_t n = (size_t)(close - p - 1);
        if (n >= sizeof(hostbuf))
            n = sizeof(hostbuf) - 1;
        memcpy(hostbuf, p + 1, n);
        hostbuf[n] = '\0';
        host = hostbuf;
    } else {
        const char *colon = p;
        while (colon < end && *colon != ':')
            colon++;
        size_t n = (size_t)(colon - p);
        if (n >= sizeof(hostbuf))
            n = sizeof(hostbuf) - 1;
        memcpy(hostbuf, p, n);
        hostbuf[n] = '\0';
        host = hostbuf;
    }
    free(norm);
    if (!host || !*host)
        return 0;

    for (int i = 0; MM_LOCAL_HOSTS[i]; i++)
        if (strcmp(host, MM_LOCAL_HOSTS[i]) == 0)
            return 1;
    for (int i = 0; MM_CONTAINER_SUFFIXES[i]; i++)
        if (endswith(host, MM_CONTAINER_SUFFIXES[i]))
            return 1;
    if (endswith(host, ".local"))
        return 1;
    if (!strchr(host, '.') && !strchr(host, ':'))
        return 1;

    {
        unsigned a, b, c, d;
        if (parse_ipv4(host, &a, &b, &c, &d)) {
            (void)c;
            (void)d;
            if (a == 10 || (a == 172 && b >= 16 && b <= 31)
                || (a == 192 && b == 168) || (a == 100 && b >= 64 && b <= 127)
                || a == 127 || a == 0)
                return 1;
            return 0;
        }
    }
    {
        int cls = ipv6_class(host);
        if (cls)
            return 1;
    }
    return 0;
}

/* ===========================================================================
 *  Batch 2: pure gap ports (string / number / JSON logic only).
 * =========================================================================== */

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>

/* --- small shared helpers ------------------------------------------------ */

/* Python str.strip() over ASCII whitespace; caller frees. */
static char *mm_strip_dup(const char *s)
{
    if (!s)
        return strdup("");
    while (*s && isspace((unsigned char)*s))
        s++;
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1]))
        n--;
    char *out = malloc(n + 1);
    if (!out)
        return NULL;
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

static char *mm_lower_dup(const char *s)
{
    if (!s)
        return strdup("");
    size_t n = strlen(s);
    char *out = malloc(n + 1);
    if (!out)
        return NULL;
    for (size_t i = 0; i < n; i++)
        out[i] = (char)tolower((unsigned char)s[i]);
    out[n] = '\0';
    return out;
}

static char *mm_strip_lower_dup(const char *s)
{
    char *t = mm_strip_dup(s);
    if (!t)
        return NULL;
    for (char *p = t; *p; p++)
        *p = (char)tolower((unsigned char)*p);
    return t;
}

static char *dup_n(const char *s, size_t n)
{
    char *out = malloc(n + 1);
    if (!out)
        return NULL;
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

static void copy_trunc(char *dst, size_t cap, const char *s, const char *e)
{
    size_t n = (size_t)(e - s);
    if (n >= cap)
        n = cap - 1;
    memcpy(dst, s, n);
    dst[n] = '\0';
}

static int str_eq_ci(const char *a, const char *b)
{
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
            return 0;
        a++;
        b++;
    }
    return *a == *b;
}

/* --- Python float formatting / parsing ----------------------------------- */

/* Python str(float): shortest round-trip digits; fixed notation for
 * -4 <= exp < 16, scientific otherwise; integral fixed values keep ".0". */
static void py_float_str(double v, char *out, size_t cap)
{
    if (isnan(v)) {
        snprintf(out, cap, "nan");
        return;
    }
    if (isinf(v)) {
        snprintf(out, cap, v < 0 ? "-inf" : "inf");
        return;
    }
    int neg = signbit(v) != 0;
    if (v == 0.0) {
        snprintf(out, cap, "%s0.0", neg ? "-" : "");
        return;
    }
    double a = neg ? -v : v;
    char buf[64];
    for (int prec = 1; prec <= 17; prec++) {
        snprintf(buf, sizeof buf, "%.*g", prec, a);
        if (strtod(buf, NULL) == a)
            break;
    }
    /* Collect the mantissa digits and the decimal exponent E (d.ddd x 10^E). */
    char digits[40];
    size_t nd = 0, before_dot = 0;
    int exp10 = 0, seen_dot = 0;
    const char *e = strchr(buf, 'e');
    const char *mend = e ? e : buf + strlen(buf);
    if (e)
        exp10 = atoi(e + 1);
    for (const char *p = buf; p < mend; p++) {
        if (*p == '.') {
            seen_dot = 1;
            continue;
        }
        if (nd < sizeof digits - 1)
            digits[nd++] = *p;
        if (!seen_dot)
            before_dot++;
    }
    digits[nd] = '\0';
    size_t dot_pos = seen_dot ? before_dot : nd;
    size_t f = 0;
    while (f < nd && digits[f] == '0')
        f++;
    size_t last = nd;
    while (last > f && digits[last - 1] == '0')
        last--;
    int E = (int)dot_pos - 1 - (int)f + exp10;
    char sig[40];
    size_t ns = last - f;
    memcpy(sig, digits + f, ns);
    sig[ns] = '\0';
    char body[96];
    if (E < -4 || E >= 16) {
        char m[48];
        if (ns > 1)
            snprintf(m, sizeof m, "%c.%s", sig[0], sig + 1);
        else
            snprintf(m, sizeof m, "%c", sig[0]);
        snprintf(body, sizeof body, "%se%c%02d", m, E < 0 ? '-' : '+',
                 E < 0 ? -E : E);
    } else if (E >= 0) {
        size_t ip = (size_t)E + 1;
        if (ns > ip) {
            memcpy(body, sig, ip);
            body[ip] = '.';
            memcpy(body + ip + 1, sig + ip, ns - ip);
            body[ip + 1 + (ns - ip)] = '\0';
        } else {
            memcpy(body, sig, ns);
            memset(body + ns, '0', ip - ns);
            strcpy(body + ip, ".0");
        }
    } else {
        size_t zeros = (size_t)(-E - 1);
        body[0] = '0';
        body[1] = '.';
        memset(body + 2, '0', zeros);
        memcpy(body + 2 + zeros, sig, ns);
        body[2 + zeros + ns] = '\0';
    }
    snprintf(out, cap, "%s%s", neg ? "-" : "", body);
}

/* float() grammar for the decimal body (underscores only between digits). */
static int valid_float_body(const char *p)
{
    int ndig = 0;
    if (isdigit((unsigned char)*p)) {
        ndig++;
        p++;
        while (*p) {
            if (isdigit((unsigned char)*p)) {
                ndig++;
                p++;
            } else if (*p == '_' && isdigit((unsigned char)p[1])) {
                p++;
            } else {
                break;
            }
        }
    }
    if (*p == '.') {
        p++;
        if (isdigit((unsigned char)*p)) {
            ndig++;
            p++;
            while (*p) {
                if (isdigit((unsigned char)*p)) {
                    ndig++;
                    p++;
                } else if (*p == '_' && isdigit((unsigned char)p[1])) {
                    p++;
                } else {
                    break;
                }
            }
        }
    }
    if (!ndig)
        return 0;
    if (*p == 'e' || *p == 'E') {
        p++;
        if (*p == '+' || *p == '-')
            p++;
        if (!isdigit((unsigned char)*p))
            return 0;
        while (*p) {
            if (isdigit((unsigned char)*p))
                p++;
            else if (*p == '_' && isdigit((unsigned char)p[1]))
                p++;
            else
                break;
        }
    }
    return *p == '\0';
}

/* Python float(value) for JSON scalars: numbers and bools convert, strings
 * parse (whitespace, underscores, inf/infinity/nan); anything else raises
 * TypeError in Python and is reported as failure here. */
static int py_float_parse(const json_t *v, double *out)
{
    if (!v)
        return 0;
    if (v->type == JSON_NUMBER) {
        *out = v->num_val;
        return 1;
    }
    if (v->type == JSON_BOOL) {
        *out = v->bool_val ? 1.0 : 0.0;
        return 1;
    }
    if (v->type != JSON_STRING)
        return 0;
    char *t = mm_strip_dup(v->str_val);
    if (!t)
        return 0;
    char *p = t;
    int neg = 0;
    if (*p == '+' || *p == '-') {
        neg = *p == '-';
        p++;
    }
    if (str_eq_ci(p, "inf") || str_eq_ci(p, "infinity")) {
        *out = neg ? -INFINITY : INFINITY;
        free(t);
        return 1;
    }
    if (str_eq_ci(p, "nan")) {
        *out = NAN;
        free(t);
        return 1;
    }
    int ok = valid_float_body(p);
    double d = 0.0;
    if (ok) {
        char clean[512];
        size_t k = 0;
        for (const char *q = p; *q && k < sizeof clean - 1; q++)
            if (*q != '_')
                clean[k++] = *q;
        clean[k] = '\0';
        char *end = NULL;
        d = strtod(clean, &end);
        ok = end && *end == '\0';
        if (neg)
            d = -d;
    }
    free(t);
    if (!ok)
        return 0;
    *out = d;
    return 1;
}

/* Python int(str) grammar: optional sign, digits with underscores between
 * digits only. Caller pre-strips whitespace / commas. */
static int py_int_str_parse(const char *s, long long *out)
{
    const char *p = s;
    int neg = 0;
    if (*p == '+' || *p == '-') {
        neg = *p == '-';
        p++;
    }
    if (!isdigit((unsigned char)*p))
        return 0;
    char clean[32];
    size_t k = 0;
    while (*p) {
        if (isdigit((unsigned char)*p)) {
            if (k >= sizeof clean - 1)
                return 0;
            clean[k++] = *p++;
        } else if (*p == '_' && isdigit((unsigned char)p[1])) {
            p++;
        } else {
            return 0;
        }
    }
    clean[k] = '\0';
    errno = 0;
    long long v = strtoll(clean, NULL, 10);
    if (errno == ERANGE)
        return 0;
    *out = neg ? -v : v;
    return 1;
}

/* _coerce_reasonable_int (model_metadata.py:797): bools rejected, strings
 * stripped and comma-free, ints/floats truncated; 1024..10_000_000 window. */
static int coerce_reasonable_int(const json_t *v, long long *out)
{
    if (!v || v->type == JSON_BOOL || v->type == JSON_NULL)
        return 0;
    long long r = 0;
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (!isfinite(d) || d >= 9223372036854775808.0
            || d <= -9223372036854775808.0)
            return 0;
        r = (long long)d;
    } else if (v->type == JSON_STRING) {
        char *trimmed = mm_strip_dup(v->str_val);
        if (!trimmed)
            return 0;
        char clean[512];
        size_t k = 0;
        for (const char *p = trimmed; *p && k < sizeof clean - 1; p++)
            if (*p != ',')
                clean[k++] = *p;
        clean[k] = '\0';
        free(trimmed);
        if (!py_int_str_parse(clean, &r))
            return 0;
    } else {
        return 0;
    }
    if (r < 1024 || r > 10000000)
        return 0;
    *out = r;
    return 1;
}

/* --- URL decomposition (urllib.parse.urlsplit semantics) ------------------ */

typedef struct {
    char scheme[32];
    char netloc[512];
    char path[1024];
    char query[1024];
    char frag[512];
} url_parts;

static void url_split(const char *url, url_parts *u)
{
    memset(u, 0, sizeof *u);
    const char *p = url;
    const char *colon = strchr(url, ':');
    if (colon && colon > url && isalpha((unsigned char)url[0])) {
        int ok = 1;
        for (const char *q = url; q < colon; q++) {
            if (!(isalnum((unsigned char)*q) || *q == '+' || *q == '-'
                  || *q == '.')) {
                ok = 0;
                break;
            }
        }
        if (ok) {
            size_t n = (size_t)(colon - url);
            if (n >= sizeof u->scheme)
                n = sizeof u->scheme - 1;
            for (size_t i = 0; i < n; i++)
                u->scheme[i] = (char)tolower((unsigned char)url[i]);
            u->scheme[n] = '\0';
            p = colon + 1;
        }
    }
    if (p[0] == '/' && p[1] == '/') {
        p += 2;
        const char *e = p;
        while (*e && *e != '/' && *e != '?' && *e != '#')
            e++;
        copy_trunc(u->netloc, sizeof u->netloc, p, e);
        p = e;
    }
    const char *e = p;
    while (*e && *e != '?' && *e != '#')
        e++;
    copy_trunc(u->path, sizeof u->path, p, e);
    p = e;
    if (*p == '?') {
        p++;
        e = p;
        while (*e && *e != '#')
            e++;
        copy_trunc(u->query, sizeof u->query, p, e);
        p = e;
    }
    if (*p == '#')
        copy_trunc(u->frag, sizeof u->frag, p + 1, p + 1 + strlen(p + 1));
}

/* urllib.parse unquote: %XX bytes decoded, everything else literal. */
static void pct_decode(char *dst, size_t cap, const char *s, const char *e)
{
    size_t k = 0;
    for (const char *p = s; p < e && k + 1 < cap; p++) {
        if (*p == '%' && p + 2 < e && isxdigit((unsigned char)p[1])
            && isxdigit((unsigned char)p[2])) {
            char hx[3] = { p[1], p[2], '\0' };
            dst[k++] = (char)strtol(hx, NULL, 16);
            p += 2;
        } else {
            dst[k++] = *p;
        }
    }
    dst[k] = '\0';
}

typedef struct {
    char host[256];      /* lowercased, brackets removed; "" when absent */
    char raw_user[256];  /* userinfo before the last '@', as written */
    int has_user;
    char username[256];
    char password[256];
    int username_none;
    int password_none;
    long port;           /* -1 = None */
    int port_invalid;    /* .port would raise ValueError */
} hostinfo;

static void netloc_hostinfo(const char *netloc, hostinfo *h)
{
    memset(h, 0, sizeof *h);
    h->port = -1;
    h->username_none = 1;
    h->password_none = 1;
    const char *at = strrchr(netloc, '@');
    const char *hi = netloc;
    if (at) {
        h->has_user = 1;
        copy_trunc(h->raw_user, sizeof h->raw_user, netloc, at);
        const char *c = memchr(netloc, ':', (size_t)(at - netloc));
        pct_decode(h->username, sizeof h->username, netloc, c ? c : at);
        h->username_none = 0;
        if (c) {
            pct_decode(h->password, sizeof h->password, c + 1, at);
            h->password_none = 0;
        }
        hi = at + 1;
    }
    const char *portstr = NULL;
    if (hi[0] == '[') {
        const char *close = strchr(hi, ']');
        if (!close) {
            h->port_invalid = 1;
            return;
        }
        copy_trunc(h->host, sizeof h->host, hi + 1, close);
        if (close[1] == ':')
            portstr = close + 2;
        else if (close[1] != '\0')
            h->port_invalid = 1;
    } else {
        const char *c = strchr(hi, ':');
        if (c) {
            copy_trunc(h->host, sizeof h->host, hi, c);
            portstr = c + 1;
        } else {
            copy_trunc(h->host, sizeof h->host, hi, hi + strlen(hi));
        }
    }
    for (char *q = h->host; *q; q++)
        *q = (char)tolower((unsigned char)*q);
    if (portstr && *portstr) {
        const char *q = portstr;
        if (*q == '+')
            q++;
        if (!*q) {
            h->port_invalid = 1;
            return;
        }
        long v = 0;
        for (; *q; q++) {
            if (!isdigit((unsigned char)*q) || v > 65535) {
                h->port_invalid = 1;
                return;
            }
            v = v * 10 + (*q - '0');
        }
        if (v > 65535) {
            h->port_invalid = 1;
            return;
        }
        h->port = v;
    }
}

/* utils/urls.py base_url_hostname: strip, tolerate a bare host[:port][/path]
 * (no scheme), lowercase, strip trailing dots. */
static void utils_hostname(const char *base_url, char *out, size_t cap)
{
    out[0] = '\0';
    if (!base_url)
        return;
    char *raw = mm_strip_dup(base_url);
    if (!raw)
        return;
    if (!*raw) {
        free(raw);
        return;
    }
    char *work = malloc(strlen(raw) + 4);
    if (!work) {
        free(raw);
        return;
    }
    if (strstr(raw, "://"))
        strcpy(work, raw);
    else {
        strcpy(work, "//");
        strcat(work, raw);
    }
    url_parts u;
    url_split(work, &u);
    free(work);
    free(raw);
    hostinfo h;
    netloc_hostinfo(u.netloc, &h);
    size_t n = strlen(h.host);
    while (n > 0 && h.host[n - 1] == '.')
        n--;
    if (n >= cap)
        n = cap - 1;
    memcpy(out, h.host, n);
    out[n] = '\0';
}

/* Static slice of _infer_provider_from_url: the _URL_TO_PROVIDER table plus
 * the regional Vertex rule. Provider-profile claims (plugin registry, home
 * dependent) are outside the pure slice; for every bundled profile the table
 * and the registry agree on the hosts used by the oracle cases. */
static const char *infer_provider_static(const char *base_url)
{
    static const struct { const char *domain, *provider; } table[] = {
        { "api.openai.com", "openai" }, { "chatgpt.com", "openai" },
        { "api.anthropic.com", "anthropic" },
        { "api.z.ai", "zai" }, { "open.bigmodel.cn", "zai" },
        { "api.moonshot.ai", "kimi-coding" }, { "api.moonshot.cn", "kimi-coding-cn" },
        { "api.kimi.com", "kimi-coding" },
        { "api.stepfun.ai", "stepfun" }, { "api.stepfun.com", "stepfun" },
        { "api.arcee.ai", "arcee" },
        { "api.minimax.io", "minimax" }, { "api.minimaxi.com", "minimax" },
        { "api.minimax.chat", "minimax" },
        { "dashscope.aliyuncs.com", "alibaba" },
        { "dashscope-intl.aliyuncs.com", "alibaba" },
        { "portal.qwen.ai", "qwen-oauth" },
        { "openrouter.ai", "openrouter" },
        { "generativelanguage.googleapis.com", "gemini" },
        { "inference-api.nousresearch.com", "nous" },
        { "api.deepseek.com", "deepseek" },
        { "api.fireworks.ai", "fireworks" },
        { "opencode.ai", "opencode-go" }, { "api.x.ai", "xai" },
        { "integrate.api.nvidia.com", "nvidia" },
        { "api.xiaomimimo.com", "xiaomi" }, { "xiaomimimo.com", "xiaomi" },
        { "api.gmi-serving.com", "gmi" }, { "api.novita.ai", "novita" },
        { "tokenhub.tencentmaas.com", "tencent-tokenhub" },
        { "api.lkeap.cloud.tencent.com", "tencent-tokenplan" },
        { "ollama.com", "ollama-cloud" },
        { NULL, NULL }
    };
    char host[256];
    utils_hostname(base_url, host, sizeof host);
    if (!*host)
        return NULL;
    for (int i = 0; table[i].domain; i++) {
        const char *d = table[i].domain;
        if (strcmp(host, d) == 0)
            return table[i].provider;
        size_t dl = strlen(d);
        if (strlen(host) > dl && host[strlen(host) - dl - 1] == '.'
            && strcmp(host + strlen(host) - dl, d) == 0)
            return table[i].provider;
    }
    if (endswith(host, "-aiplatform.googleapis.com"))
        return "vertex";
    return NULL;
}

/* hermes_constants.openrouter_variant_base: last-colon partition, suffix in
 * {nitro, floor, exacto, online} (case-insensitive), non-empty base. */
static char *openrouter_variant_base_dup(const char *model)
{
    const char *s = model ? model : "";
    const char *colon = strrchr(s, ':');
    if (!colon || colon == s)
        return NULL;
    char *low = mm_lower_dup(colon + 1);
    if (!low)
        return NULL;
    int hit = strcmp(low, "nitro") == 0 || strcmp(low, "floor") == 0
              || strcmp(low, "exacto") == 0 || strcmp(low, "online") == 0;
    free(low);
    if (!hit)
        return NULL;
    return dup_n(s, (size_t)(colon - s));
}

/* ===========================================================================
 *  _strip_openrouter_routing_variant  (model_metadata.py:490)
 * =========================================================================== */

/* PoP: mm_strip_openrouter_routing_variant @ agent/model_metadata.py:_strip_openrouter_routing_variant */
char *mm_strip_openrouter_routing_variant(const char *model, const char *base_url,
                                          const char *provider)
{
    if (!model)
        return NULL;
    if (!*model)
        return strdup("");
    char *pl = mm_strip_lower_dup(provider ? provider : "");
    int is_or = pl && strcmp(pl, "openrouter") == 0;
    free(pl);
    if (!is_or && base_url && *base_url) {
        const char *inf = infer_provider_static(base_url);
        is_or = inf && strcmp(inf, "openrouter") == 0;
    }
    if (!is_or)
        return strdup(model);
    char *base = openrouter_variant_base_dup(model);
    if (!base)
        return strdup(model);
    return base;
}

/* --- Codex route identity (hermes_cli.route_identity slice) --------------- */

/* normalize_route_base_url: canonicalize proven-equivalent URL components;
 * any control/space character, missing scheme/host or bad port returns raw. */
static char *normalize_route_url(const char *base_url)
{
    const char *raw = base_url ? base_url : "";
    if (!*raw)
        return strdup("");
    for (const char *p = raw; *p; p++)
        if ((unsigned char)*p <= 0x20)
            return strdup(raw);
    int had_q = 0;
    for (const char *p = raw; *p && *p != '#'; p++)
        if (*p == '?') {
            had_q = 1;
            break;
        }
    url_parts u;
    url_split(raw, &u);
    hostinfo h;
    netloc_hostinfo(u.netloc, &h);
    if (!u.scheme[0] || !h.host[0] || h.port_invalid)
        return strdup(raw);
    const char *route_host = strrchr(u.netloc, '@');
    route_host = route_host ? route_host + 1 : u.netloc;
    char host[600];
    int bracket = route_host[0] == '[' || strchr(h.host, ':') != NULL;
    snprintf(host, sizeof host, "%s%s%s", bracket ? "[" : "", h.host,
             bracket ? "]" : "");
    if (h.port >= 0
        && !((strcmp(u.scheme, "http") == 0 && h.port == 80)
             || (strcmp(u.scheme, "https") == 0 && h.port == 443))) {
        char tmp[640];
        snprintf(tmp, sizeof tmp, "%s:%ld", host, h.port);
        snprintf(host, sizeof host, "%s", tmp);
    }
    if (h.has_user) {
        char tmp[900];
        snprintf(tmp, sizeof tmp, "%s@%s", h.raw_user, host);
        snprintf(host, sizeof host, "%s", tmp);
    }
    size_t plen = strlen(u.path);
    if (plen > 0 && u.path[plen - 1] == '/' && !had_q)
        u.path[plen - 1] = '\0';
    char out[2048];
    snprintf(out, sizeof out, "%s://%s%s%s%s", u.scheme, host, u.path,
             u.query[0] ? "?" : "", u.query);
    if (had_q && !u.query[0]) {
        size_t n = strlen(out);
        if (n + 1 < sizeof out) {
            out[n] = '?';
            out[n + 1] = '\0';
        }
    }
    return strdup(out);
}

/* hermes_cli.config_providers._canonical_api_mode: alias spellings map to
 * canonical transports; unknown values pass through stripped, case kept. */
static void canonical_api_mode(const char *value, char *out, size_t cap)
{
    static const struct { const char *alias, *canon; } aliases[] = {
        { "openai", "chat_completions" },
        { "openai_chat", "chat_completions" },
        { "openai-chat", "chat_completions" },
        { "chat-completions", "chat_completions" },
        { "chatcompletions", "chat_completions" },
        { "responses", "codex_responses" },
        { "openai_responses", "codex_responses" },
        { "openai-responses", "codex_responses" },
        { "anthropic", "anthropic_messages" },
        { "anthropic-messages", "anthropic_messages" },
        { "messages", "anthropic_messages" },
        { "bedrock", "bedrock_converse" },
        { "bedrock-converse", "bedrock_converse" },
        { NULL, NULL }
    };
    char *cleaned = mm_strip_dup(value);
    char *low = mm_lower_dup(cleaned ? cleaned : "");
    const char *canon = NULL;
    if (low) {
        for (int i = 0; aliases[i].alias; i++)
            if (strcmp(low, aliases[i].alias) == 0) {
                canon = aliases[i].canon;
                break;
            }
    }
    snprintf(out, cap, "%s", canon ? canon : (cleaned ? cleaned : ""));
    free(cleaned);
    free(low);
}

/* ===========================================================================
 *  _is_codex_route  (model_metadata.py:629)
 *      The custom-provider list is passed explicitly (the pure slice); the
 *      live function loads it from config when None, which is I/O and out of
 *      scope here. NULL is treated as an empty list.
 * =========================================================================== */

/* PoP: mm_is_codex_route @ agent/model_metadata.py:_is_codex_route */
int mm_is_codex_route(const char *provider, const char *base_url,
                      const json_t *custom_providers)
{
    char *pl = mm_strip_lower_dup(provider ? provider : "");
    int native = pl && strcmp(pl, "openai-codex") == 0;
    free(pl);
    if (native)
        return 1;
    if (!base_url || !*base_url)
        return 0;
    if (!custom_providers || custom_providers->type != JSON_ARRAY)
        return 0;
    char *target = normalize_route_url(base_url);
    if (!target)
        return 0;
    int done = 0, verdict = 0;
    if (*target) {
        for (size_t i = 0; i < custom_providers->c.count && !done; i++) {
            const json_t *entry = custom_providers->c.items[i];
            if (!entry || entry->type != JSON_OBJECT)
                continue;
            const json_t *bu = json_obj_get(entry, "base_url");
            char nbuf[64];
            const char *eurl = NULL;
            if (bu && bu->type == JSON_STRING)
                eurl = bu->str_val;
            else if (bu && bu->type == JSON_NUMBER) {
                double d = bu->num_val;
                if (fabs(d) < 9.2e18 && d == (double)(long long)d)
                    snprintf(nbuf, sizeof nbuf, "%lld", (long long)d);
                else
                    py_float_str(d, nbuf, sizeof nbuf);
                eurl = nbuf;
            } else if (bu && bu->type == JSON_BOOL) {
                eurl = bu->bool_val ? "True" : "False";
            }
            char *norm = normalize_route_url(eurl);
            int match = norm && *norm && strcmp(norm, target) == 0;
            free(norm);
            if (!match)
                continue;
            static const char *const fields[] = { "api_mode", "transport" };
            for (int f = 0; f < 2 && !done; f++) {
                const json_t *v = json_obj_get(entry, fields[f]);
                if (!v || v->type != JSON_STRING)
                    continue;
                char *trimmed = mm_strip_dup(v->str_val);
                int nonempty = trimmed && *trimmed;
                free(trimmed);
                if (nonempty) {
                    char mode[128];
                    canonical_api_mode(v->str_val, mode, sizeof mode);
                    verdict = strcmp(mode, "codex_responses") == 0;
                    done = 1;
                }
            }
        }
    }
    free(target);
    return verdict;
}

/* ===========================================================================
 *  _extract_flat_context_length  (model_metadata.py:818)
 *  _context_length_from_model_payload  (model_metadata.py:824)
 * =========================================================================== */

/* PoP: mm_extract_flat_context_length @ agent/model_metadata.py:_extract_flat_context_length */
int mm_extract_flat_context_length(const json_t *payload, long long *out)
{
    static const char *const keys[] = {
        "context_length", "context_window", "context_size", "max_context_length",
        "max_position_embeddings", "max_model_len", "max_input_tokens",
        "max_sequence_length", "max_seq_len", "n_ctx_train", "n_ctx", "ctx_size",
        NULL
    };
    if (!payload || payload->type != JSON_OBJECT)
        return 0;
    for (int i = 0; keys[i]; i++) {
        const json_t *v = json_obj_get(payload, keys[i]);
        if (coerce_reasonable_int(v, out))
            return 1;
    }
    return 0;
}

/* PoP: mm_context_length_from_model_payload @ agent/model_metadata.py:_context_length_from_model_payload */
int mm_context_length_from_model_payload(const json_t *payload, long long *out)
{
    if (!payload || payload->type != JSON_OBJECT)
        return 0;
    if (mm_extract_flat_context_length(payload, out))
        return 1;
    const json_t *raw = json_obj_get(payload, "max_tokens");
    if (!raw)
        return 0;
    /* isinstance(raw, (int, float)) includes bool in Python. */
    if (raw->type == JSON_BOOL) {
        if (!raw->bool_val)
            return 0;
        *out = 1;
        return 1;
    }
    if (raw->type != JSON_NUMBER)
        return 0;
    double d = raw->num_val;
    if (!isfinite(d) || d >= 9223372036854775808.0
        || d <= -9223372036854775808.0)
        return 0;
    long long r = (long long)d;
    if (r <= 0)
        return 0;
    *out = r;
    return 1;
}

/* ===========================================================================
 *  _normalize_token_rates  (model_metadata.py:846)
 * =========================================================================== */

/* PoP: mm_normalize_token_rates @ agent/model_metadata.py:_normalize_token_rates */
json_t *mm_normalize_token_rates(json_t *pricing, const json_t *unit)
{
    static const char *const fields[] = {
        "prompt", "completion", "cache_read", "cache_write", "cache_write_1h",
        NULL
    };
    if (!pricing || pricing->type != JSON_OBJECT)
        return pricing;
    double rates[5];
    int have[5] = { 0, 0, 0, 0, 0 };
    int any_big = 0;
    for (int i = 0; fields[i]; i++) {
        const json_t *v = json_obj_get(pricing, fields[i]);
        if (v && py_float_parse(v, &rates[i])) {
            have[i] = 1;
            if (rates[i] >= 0.001)
                any_big = 1;
        }
    }
    /* str(unit or "").strip().lower(): falsy units (None, "", 0, False) -> "". */
    char ubuf[64];
    const char *us = "";
    if (unit && unit->type == JSON_STRING) {
        us = unit->str_val;
    } else if (unit && unit->type == JSON_BOOL && unit->bool_val) {
        us = "True";
    } else if (unit && unit->type == JSON_NUMBER && unit->num_val != 0.0) {
        double d = unit->num_val;
        if (fabs(d) < 9.2e18 && d == (double)(long long)d)
            snprintf(ubuf, sizeof ubuf, "%lld", (long long)d);
        else
            py_float_str(d, ubuf, sizeof ubuf);
        us = ubuf;
    }
    char *ustrip = mm_strip_dup(us);
    char *ulow = mm_lower_dup(ustrip ? ustrip : "");
    free(ustrip);
    long divisor = 0;
    if (ulow) {
        if (strcmp(ulow, "per_token") == 0)
            divisor = 1;
        else if (strcmp(ulow, "per_1k_tokens") == 0
                 || strcmp(ulow, "per_thousand_tokens") == 0)
            divisor = 1000;
        else if (strcmp(ulow, "per_1m_tokens") == 0
                 || strcmp(ulow, "per_million_tokens") == 0)
            divisor = 1000000;
    }
    free(ulow);
    if (divisor == 0)
        divisor = any_big ? 1000000 : 1;
    if (divisor != 1) {
        for (int i = 0; fields[i]; i++) {
            if (!have[i])
                continue;
            char s[64];
            py_float_str(rates[i] / (double)divisor, s, sizeof s);
            json_set(pricing, fields[i], json_string(s));
        }
    }
    return pricing;
}

/* --- provider-error regex scanners (hand-rolled; no regex lib linked) ----- */

/* Greedy digit run -> long long; NULL when absent or beyond long long. */
static const char *scan_digits(const char *d, long long *out)
{
    const char *s = d;
    unsigned long long v = 0;
    int overflow = 0;
    while (isdigit((unsigned char)*d)) {
        unsigned digit = (unsigned)(*d - '0');
        if (v > (ULLONG_MAX - digit) / 10)
            overflow = 1;
        if (!overflow)
            v = v * 10 + digit;
        d++;
    }
    if (d == s || overflow || v > LLONG_MAX)
        return NULL;
    *out = (long long)v;
    return d;
}

/* re.search(r'maximum context length is (\d+)', s) */
static int re_max_ctx_is(const char *s, long long *out)
{
    static const char lit[] = "maximum context length is ";
    const char *p = s;
    while ((p = strstr(p, lit)) != NULL) {
        long long v;
        if (scan_digits(p + sizeof lit - 1, &v)) {
            *out = v;
            return 1;
        }
        p += sizeof lit - 1;
    }
    return 0;
}

/* _COMPLETION_SPLIT_RE.search:
 * \( (\d+) \s+ (?:tokens\s+)? in (?:the messages|your prompt|the prompt)
 *   \s* [;,] \s* (\d+) \s+ (?:tokens\s+)? (?:in|for) the completion\) */
static int re_completion_split(const char *s, long long *prompt, long long *req)
{
    for (const char *p = s; (p = strchr(p, '(')) != NULL; p++) {
        const char *q = p + 1;
        long long d1, d2;
        const char *e = scan_digits(q, &d1);
        if (!e)
            continue;
        q = e;
        const char *ws = q;
        while (isspace((unsigned char)*q))
            q++;
        if (q == ws)
            continue;
        if (strncmp(q, "tokens", 6) == 0) {
            const char *r = q + 6;
            const char *w2 = r;
            while (isspace((unsigned char)*r))
                r++;
            if (r > w2)
                q = r;
        }
        if (strncmp(q, "in ", 3) != 0)
            continue;
        q += 3;
        if (strncmp(q, "the messages", 12) == 0)
            q += 12;
        else if (strncmp(q, "your prompt", 11) == 0)
            q += 11;
        else if (strncmp(q, "the prompt", 10) == 0)
            q += 10;
        else
            continue;
        while (isspace((unsigned char)*q))
            q++;
        if (*q != ';' && *q != ',')
            continue;
        q++;
        while (isspace((unsigned char)*q))
            q++;
        e = scan_digits(q, &d2);
        if (!e)
            continue;
        q = e;
        ws = q;
        while (isspace((unsigned char)*q))
            q++;
        if (q == ws)
            continue;
        if (strncmp(q, "tokens", 6) == 0) {
            const char *r = q + 6;
            const char *w2 = r;
            while (isspace((unsigned char)*r))
                r++;
            if (r > w2)
                q = r;
        }
        if (strncmp(q, "in ", 3) == 0)
            q += 3;
        else if (strncmp(q, "for ", 4) == 0)
            q += 4;
        else
            continue;
        if (strncmp(q, "the completion)", 15) != 0)
            continue;
        *prompt = d1;
        *req = d2;
        return 1;
    }
    return 0;
}

/* re.search(r'maximum context length of (\d+)\s*token', s) */
static int re_sglang_window(const char *s, long long *out)
{
    static const char lit[] = "maximum context length of ";
    const char *p = s;
    while ((p = strstr(p, lit)) != NULL) {
        long long v;
        const char *e = scan_digits(p + sizeof lit - 1, &v);
        if (e) {
            while (isspace((unsigned char)*e))
                e++;
            if (strncmp(e, "token", 5) == 0) {
                *out = v;
                return 1;
            }
        }
        p += sizeof lit - 1;
    }
    return 0;
}

/* re.search(r'(\d+)\s*tokens from the input messages', s) */
static int re_sglang_input(const char *s, long long *out)
{
    static const char lit[] = "tokens from the input messages";
    for (const char *p = s; *p; p++) {
        if (!isdigit((unsigned char)*p))
            continue;
        if (p > s && isdigit((unsigned char)p[-1]))
            continue;
        long long v;
        const char *e = scan_digits(p, &v);
        if (!e) {
            while (isdigit((unsigned char)*p))
                p++;
            continue;
        }
        const char *q = e;
        while (isspace((unsigned char)*q))
            q++;
        if (strncmp(q, lit, sizeof lit - 1) == 0) {
            *out = v;
            return 1;
        }
        p = e - 1;
    }
    return 0;
}

/* ===========================================================================
 *  _completion_split_budget  (model_metadata.py:1260)
 * =========================================================================== */

/* PoP: mm_completion_split_budget @ agent/model_metadata.py:_completion_split_budget */
int mm_completion_split_budget(const char *error_lower, long long *out)
{
    if (!error_lower)
        return 0;
    long long prompt, req, ctx;
    if (!re_completion_split(error_lower, &prompt, &req))
        return 0;
    if (!re_max_ctx_is(error_lower, &ctx))
        return 0;
    long long available = ctx - prompt;
    if (available < 1)
        return 0;
    *out = available;
    return 1;
}

/* ===========================================================================
 *  _sglang_window_and_input  (model_metadata.py:1371)
 * =========================================================================== */

/* PoP: mm_sglang_window_and_input @ agent/model_metadata.py:_sglang_window_and_input */
int mm_sglang_window_and_input(const char *error_lower, long long *window,
                               long long *input)
{
    if (!error_lower)
        return 0;
    long long w, in;
    if (!re_sglang_window(error_lower, &w))
        return 0;
    if (!re_sglang_input(error_lower, &in))
        return 0;
    *window = w;
    *input = in;
    return 1;
}

/* ===========================================================================
 *  _any_phrase_group  (model_metadata.py:1378)
 * =========================================================================== */

/* PoP: mm_any_phrase_group @ agent/model_metadata.py:_any_phrase_group */
int mm_any_phrase_group(const char *text, const json_t *groups)
{
    if (!text || !groups || groups->type != JSON_ARRAY)
        return 0;
    for (size_t i = 0; i < groups->c.count; i++) {
        const json_t *g = groups->c.items[i];
        if (!g || g->type != JSON_ARRAY)
            continue;
        int all = 1;
        for (size_t j = 0; j < g->c.count && all; j++) {
            const json_t *ph = g->c.items[j];
            if (!ph || ph->type != JSON_STRING || !strstr(text, ph->str_val))
                all = 0;
        }
        if (all)
            return 1;
    }
    return 0;
}

/* ===========================================================================
 *  _model_name_suggests_minimax  (model_metadata.py:1533)
 *  _model_name_suggests_stale_32k_underreport  (model_metadata.py:1539)
 *      kimi slice: lower.startswith("kimi") or "moonshot" in lower
 * =========================================================================== */

/* PoP: mm_model_name_suggests_minimax @ agent/model_metadata.py:_model_name_suggests_minimax */
int mm_model_name_suggests_minimax(const char *model)
{
    if (!model)
        return 0;
    char *low = mm_lower_dup(model);
    if (!low)
        return 0;
    int hit = strncmp(low, "minimax", 7) == 0 || contains(low, "minimaxai/");
    free(low);
    return hit;
}

/* PoP: mm_model_name_suggests_stale_32k_underreport @ agent/model_metadata.py:_model_name_suggests_stale_32k_underreport */
int mm_model_name_suggests_stale_32k_underreport(const char *model)
{
    if (!model)
        return 0;
    char *low = mm_lower_dup(model);
    if (!low)
        return 0;
    int hit = strncmp(low, "kimi", 4) == 0 || contains(low, "moonshot")
              || strncmp(low, "minimax", 7) == 0 || contains(low, "minimaxai/");
    free(low);
    return hit;
}

/* ===========================================================================
 *  _positive_int  (model_metadata.py:1549)
 *      int(value) if isinstance(value, (int, float)) and value else None
 *      (bool is an int subclass in Python: True -> 1, False -> None;
 *       floats truncate toward zero, so 0.5 -> 0)
 * =========================================================================== */

/* PoP: mm_positive_int @ agent/model_metadata.py:_positive_int */
int mm_positive_int(const json_t *value, long long *out)
{
    if (!value)
        return 0;
    if (value->type == JSON_BOOL) {
        if (!value->bool_val)
            return 0;
        *out = 1;
        return 1;
    }
    if (value->type != JSON_NUMBER)
        return 0;
    double d = value->num_val;
    if (d == 0.0 || !isfinite(d))
        return 0;
    if (d >= 9223372036854775808.0 || d <= -9223372036854775808.0)
        return 0;
    *out = (long long)d;
    return 1;
}

/* --- Codex 900k variant slugs --------------------------------------------- */

#define MM_CODEX_VARIANT_SUFFIX "-900k"

static int codex_900k_base_eligible(const char *slug)
{
    static const char *const eligible[] = {
        "gpt-5.6-sol", "gpt-5.6-terra", "gpt-5.6-luna", "gpt-6-sol", "gpt-6-luna",
        "gpt-5.4", "gpt-daybreak-blue-latest", "gpt-6-astra", "gpt-6.1-sol", NULL
    };
    static const char *const snapshot_bases[] = {
        "gpt-5.6-sol", "gpt-5.6-terra", "gpt-5.6-luna", "gpt-6-sol", "gpt-6-luna",
        NULL
    };
    if (!slug || !*slug || endswith(slug, MM_CODEX_VARIANT_SUFFIX))
        return 0;
    for (int i = 0; eligible[i]; i++)
        if (strcmp(slug, eligible[i]) == 0)
            return 1;
    for (int i = 0; snapshot_bases[i]; i++) {
        size_t bl = strlen(snapshot_bases[i]);
        if (strncmp(slug, snapshot_bases[i], bl) == 0 && slug[bl] == '-') {
            const char *r = slug + bl + 1;
            if (strlen(r) == 10
                && isdigit((unsigned char)r[0]) && isdigit((unsigned char)r[1])
                && isdigit((unsigned char)r[2]) && isdigit((unsigned char)r[3])
                && r[4] == '-'
                && isdigit((unsigned char)r[5]) && isdigit((unsigned char)r[6])
                && r[7] == '-'
                && isdigit((unsigned char)r[8]) && isdigit((unsigned char)r[9]))
                return 1;
        }
    }
    return 0;
}

/* ===========================================================================
 *  _bare_codex_slug  (model_metadata.py:1691)
 * =========================================================================== */

/* PoP: mm_bare_codex_slug @ agent/model_metadata.py:_bare_codex_slug */
char *mm_bare_codex_slug(const char *model)
{
    char *sl = mm_strip_lower_dup(model ? model : "");
    if (!sl)
        return NULL;
    char *out = strdup(last_segment(sl));
    free(sl);
    return out;
}

/* ===========================================================================
 *  _codex_variant_base  (model_metadata.py:1711)
 * =========================================================================== */

/* PoP: mm_codex_variant_base @ agent/model_metadata.py:_codex_variant_base */
char *mm_codex_variant_base(const char *model)
{
    char *slug = mm_bare_codex_slug(model);
    if (!slug)
        return NULL;
    char *out = NULL;
    if (endswith(slug, MM_CODEX_VARIANT_SUFFIX)) {
        slug[strlen(slug) - (sizeof MM_CODEX_VARIANT_SUFFIX - 1)] = '\0';
        if (codex_900k_base_eligible(slug))
            out = strdup(slug);
    }
    free(slug);
    return out;
}

/* ===========================================================================
 *  _is_bedrock_context  (model_metadata.py:2005)
 * =========================================================================== */

/* PoP: mm_is_bedrock_context @ agent/model_metadata.py:_is_bedrock_context */
int mm_is_bedrock_context(const char *base_url, const char *provider)
{
    if (provider && strcmp(provider, "bedrock") == 0)
        return 1;
    if (!base_url || !*base_url)
        return 0;
    char host[256];
    utils_hostname(base_url, host, sizeof host);
    if (strncmp(host, "bedrock-runtime.", 16) != 0)
        return 0;
    return strcmp(host, "amazonaws.com") == 0 || endswith(host, ".amazonaws.com");
}

/* ===========================================================================
 *  _parse_base_url  (model_metadata.py:99)
 *      urlparse of the normalized URL (scheme prepended when absent);
 *      returns 0 when Python returns None (empty after normalization).
 * =========================================================================== */

/* PoP: mm_parse_base_url @ agent/model_metadata.py:_parse_base_url */
int mm_parse_base_url(const char *base_url, const char *scheme, mm_parsed_url *out)
{
    if (!out)
        return 0;
    memset(out, 0, sizeof *out);
    out->port = -1;
    out->username_none = 1;
    out->password_none = 1;
    char *norm = mm_normalize_base_url(base_url);
    if (!norm)
        return 0;
    if (!*norm) {
        free(norm);
        return 0;
    }
    const char *sch = (scheme && *scheme) ? scheme : "http";
    char *url;
    if (strstr(norm, "://")) {
        url = norm;
    } else {
        url = malloc(strlen(norm) + strlen(sch) + 4);
        if (!url) {
            free(norm);
            return 0;
        }
        sprintf(url, "%s://%s", sch, norm);
        free(norm);
    }
    url_parts u;
    url_split(url, &u);
    free(url);
    snprintf(out->scheme, sizeof out->scheme, "%s", u.scheme);
    snprintf(out->netloc, sizeof out->netloc, "%s", u.netloc);
    snprintf(out->query, sizeof out->query, "%s", u.query);
    snprintf(out->fragment, sizeof out->fragment, "%s", u.frag);
    /* urlparse _splitparams: ';' at or after the last '/' splits params. */
    const char *slash = strrchr(u.path, '/');
    const char *semi = slash ? strchr(slash, ';') : strchr(u.path, ';');
    if (semi) {
        copy_trunc(out->path, sizeof out->path, u.path, semi);
        snprintf(out->params, sizeof out->params, "%s", semi + 1);
    } else {
        snprintf(out->path, sizeof out->path, "%s", u.path);
    }
    hostinfo h;
    netloc_hostinfo(u.netloc, &h);
    snprintf(out->hostname, sizeof out->hostname, "%s", h.host);
    out->port = h.port;
    out->port_invalid = h.port_invalid;
    out->username_none = h.username_none;
    out->password_none = h.password_none;
    snprintf(out->username, sizeof out->username, "%s", h.username);
    snprintf(out->password, sizeof out->password, "%s", h.password);
    return 1;
}

/* ===========================================================================
 *  _codex_catalog_key  (model_metadata.py:1883)
 *      Exact key first, then case-insensitive; keys may be an object (its
 *      keys) or an array of strings. Caller frees the result.
 * =========================================================================== */

/* PoP: mm_codex_catalog_key @ agent/model_metadata.py:_codex_catalog_key */
char *mm_codex_catalog_key(const json_t *keys, const char *slug)
{
    if (!keys || !slug)
        return NULL;
    if (keys->type == JSON_OBJECT) {
        for (size_t i = 0; i < keys->c.count; i++)
            if (keys->c.keys[i] && strcmp(keys->c.keys[i], slug) == 0)
                return strdup(slug);
        for (size_t i = 0; i < keys->c.count; i++) {
            const char *k = keys->c.keys[i];
            if (k && str_eq_ci(k, slug))
                return strdup(k);
        }
        return NULL;
    }
    if (keys->type == JSON_ARRAY) {
        for (size_t i = 0; i < keys->c.count; i++) {
            const json_t *e = keys->c.items[i];
            if (e && e->type == JSON_STRING && strcmp(e->str_val, slug) == 0)
                return strdup(slug);
        }
        for (size_t i = 0; i < keys->c.count; i++) {
            const json_t *e = keys->c.items[i];
            if (e && e->type == JSON_STRING && str_eq_ci(e->str_val, slug))
                return strdup(e->str_val);
        }
    }
    return NULL;
}