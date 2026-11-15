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
