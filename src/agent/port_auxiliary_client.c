/* port_auxiliary_client.c -- pure model/route helpers from
 * agent/auxiliary_client.py.
 *
 * PoP: ac_bare_model                       @ agent/auxiliary_client.py:_bare_model
 * PoP: ac_is_kimi_model                    @ agent/auxiliary_client.py:_is_kimi_model
 * PoP: ac_is_arcee_trinity_thinking        @ agent/auxiliary_client.py:_is_arcee_trinity_thinking
 * PoP: ac_codex_route_bare_model           @ agent/auxiliary_client.py:_codex_route_bare_model
 * PoP: ac_is_codex_spark                   @ agent/auxiliary_client.py:_is_codex_spark
 * PoP: ac_is_openai_default_temperature_only @ agent/auxiliary_client.py:_is_openai_default_temperature_only
 * PoP: ac_is_codex_gpt54_or_gpt55          @ agent/auxiliary_client.py:_is_codex_gpt54_or_gpt55
 * PoP: ac_compression_threshold_for_model  @ agent/auxiliary_client.py:_compression_threshold_for_model
 * PoP: ac_model_recency_key                @ agent/auxiliary_client.py:_model_recency_key
 *
 * Deliberately not here: _normalize_aux_provider (alias table is per-home and
 * registry-generation cached), _fixed_temperature_for_model (mutates the
 * process-global rejected-route set), _provider_alias_table, _extract_url_query_params
 * (urlparse semantics). Each is a different unit.
 */
#include "port_auxiliary_client.h"
#include "port_version_info.h"
#include "libjson/json.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define CODEX_GPT54_GPT55_THRESHOLD 0.85
#define CODEX_SPARK_THRESHOLD 0.70

/* ── _bare_model ─────────────────────────────────────────────────────────── */
char *
ac_bare_model(const char *model)
{
    const char *s = (model && *model) ? model : "";
    while (isspace((unsigned char)*s))
        s++;
    size_t len = strlen(s);
    while (len && isspace((unsigned char)s[len - 1]))
        len--;

    /* rsplit("/", 1)[-1]: the last non-empty path segment wins. */
    size_t start = 0;
    for (size_t i = 0; i < len; i++)
        if (s[i] == '/')
            start = i + 1;

    size_t n = len - start;
    char *out = (char *)malloc(n + 1);
    if (!out)
        return NULL;
    for (size_t i = 0; i < n; i++)
        out[i] = (char)tolower((unsigned char)s[start + i]);
    out[n] = '\0';
    return out;
}

/* ── _is_kimi_model ──────────────────────────────────────────────────────── */
int
ac_is_kimi_model(const char *model)
{
    char *bare = ac_bare_model(model);
    if (!bare)
        return 0;
    int hit = (strncmp(bare, "kimi-", 5) == 0) || (strcmp(bare, "kimi") == 0);
    free(bare);
    return hit;
}

/* ── _is_arcee_trinity_thinking ──────────────────────────────────────────── */
int
ac_is_arcee_trinity_thinking(const char *model)
{
    char *bare = ac_bare_model(model);
    if (!bare)
        return 0;
    int hit = strcmp(bare, "trinity-large-thinking") == 0;
    free(bare);
    return hit;
}

/* ── _codex_route_bare_model ─────────────────────────────────────────────── */
char *
ac_codex_route_bare_model(const char *model, const char *provider)
{
    const char *p = (provider && *provider) ? provider : "";
    while (isspace((unsigned char)*p))
        p++;
    size_t n = strlen(p);
    while (n && isspace((unsigned char)p[n - 1]))
        n--;
    char lower[256];
    if (n >= sizeof(lower))
        return NULL;
    for (size_t i = 0; i < n; i++)
        lower[i] = (char)tolower((unsigned char)p[i]);
    lower[n] = '\0';
    if (strcmp(lower, "openai-codex") != 0)
        return NULL;
    return ac_bare_model(model);
}

/* ── _is_codex_spark ─────────────────────────────────────────────────────── */
int
ac_is_codex_spark(const char *model, const char *provider)
{
    char *bare = ac_codex_route_bare_model(model, provider);
    if (!bare)
        return 0;
    int hit = strcmp(bare, "gpt-5.3-codex-spark") == 0;
    free(bare);
    return hit;
}

/* ── _is_openai_default_temperature_only ─────────────────────────────────── */
int
ac_is_openai_default_temperature_only(const char *model)
{
    char *bare = ac_bare_model(model);
    if (!bare)
        return 0;
    int hit = (strncmp(bare, "gpt-5", 5) == 0 ||
               strncmp(bare, "o1", 2) == 0 ||
               strncmp(bare, "o3", 2) == 0 ||
               strncmp(bare, "o4", 2) == 0) &&
              strncmp(bare, "gpt-5-chat", 10) != 0;
    free(bare);
    return hit;
}

/* ── is_codex_context_variant ──────────────────────────────────────────────
 * Ported from agent/model_metadata.py, which the Python
 * _is_codex_gpt54_or_gpt55 imports. model_metadata.c is an UNLINKED twin in
 * this build, so rather than drag in its whole dependency closure the three
 * small pure helpers it needs are ported here.
 */
static char *
ac_bare_codex_slug(const char *model)
{
    const char *s = (model && *model) ? model : "";
    while (isspace((unsigned char)*s))
        s++;
    size_t len = strlen(s);
    while (len && isspace((unsigned char)s[len - 1]))
        len--;
    size_t start = 0;
    for (size_t i = 0; i < len; i++)
        if (s[i] == '/')
            start = i + 1;
    size_t n = len - start;
    char *out = (char *)malloc(n + 1);
    if (!out)
        return NULL;
    for (size_t i = 0; i < n; i++)
        out[i] = (char)tolower((unsigned char)s[start + i]);
    out[n] = '\0';
    return out;
}

#define CODEX_CONTEXT_VARIANT_SUFFIX "-900k"

static int
ac_is_codex_900k_base(const char *model)
{
    char *slug = ac_bare_codex_slug(model);
    if (!slug || !*slug)
        return 0;
    size_t sl = strlen(slug);
    size_t suf = strlen(CODEX_CONTEXT_VARIANT_SUFFIX);
    if (sl >= suf && strcmp(slug + sl - suf, CODEX_CONTEXT_VARIANT_SUFFIX) == 0) {
        free(slug);
        return 0;
    }
    static const char *const eligible[] = {
        "gpt-5.6-sol", "gpt-5.6-terra", "gpt-5.6-luna", "gpt-6-sol",
        "gpt-6-luna", "gpt-5.4", "gpt-daybreak-blue-latest", "gpt-6-astra",
        "gpt-6.1-sol",
    };
    for (size_t i = 0; i < sizeof(eligible) / sizeof(eligible[0]); i++)
        if (strcmp(slug, eligible[i]) == 0) {
            free(slug);
            return 1;
        }
    /* dated snapshots of the routable tier bases */
    static const char *const snap_bases[] = {
        "gpt-5.6-sol", "gpt-5.6-terra", "gpt-5.6-luna", "gpt-6-sol", "gpt-6-luna",
    };
    for (size_t i = 0; i < sizeof(snap_bases) / sizeof(snap_bases[0]); i++) {
        size_t bl = strlen(snap_bases[i]);
        if (strncmp(slug, snap_bases[i], bl) == 0 && slug[bl] == '-') {
            const char *date = slug + bl + 1;
            size_t dl = strlen(date);
            int ok = (dl == 10);
            for (size_t k = 0; ok && k < dl; k++) {
                if (k == 4 || k == 7)
                    ok = date[k] == '-';
                else
                    ok = isdigit((unsigned char)date[k]) != 0;
            }
            if (ok) {
                free(slug);
                return 1;
            }
        }
    }
    free(slug);
    return 0;
}

static int
ac_is_codex_context_variant(const char *model)
{
    char *slug = ac_bare_codex_slug(model);
    if (!slug)
        return 0;
    size_t sl = strlen(slug), suf = strlen(CODEX_CONTEXT_VARIANT_SUFFIX);
    int hit = 0;
    if (sl > suf && strcmp(slug + sl - suf, CODEX_CONTEXT_VARIANT_SUFFIX) == 0) {
        char *base = (char *)malloc(sl - suf + 1);
        if (base) {
            memcpy(base, slug, sl - suf);
            base[sl - suf] = '\0';
            hit = ac_is_codex_900k_base(base);
            free(base);
        }
    }
    free(slug);
    return hit;
}

/* ── _is_codex_gpt54_or_gpt55 ────────────────────────────────────────────── */
int
ac_is_codex_gpt54_or_gpt55(const char *model, const char *provider)
{
    char *bare = ac_codex_route_bare_model(model, provider);
    if (!bare)
        return 0;
    int hit;
    if (ac_is_codex_context_variant(bare)) {
        hit = 0;
    } else if (strstr(bare, "astra") != NULL) {
        hit = strstr(bare, "900k") == NULL;
    } else if (strcmp(bare, "gpt-daybreak-blue-latest") == 0) {
        hit = 1;
    } else {
        static const char *const fams[] = {
            "gpt-5.4", "gpt-5.5", "gpt-5.6", "gpt-6-sol",
            "gpt-6.1-sol", "gpt-6-luna",
        };
        hit = 0;
        for (size_t i = 0; i < sizeof(fams) / sizeof(fams[0]) && !hit; i++) {
            size_t fl = strlen(fams[i]);
            if (strcmp(bare, fams[i]) == 0 ||
                (strncmp(bare, fams[i], fl) == 0 && bare[fl] == '-') ||
                (strncmp(bare, fams[i], fl) == 0 && bare[fl] == '.'))
                hit = 1;
        }
    }
    free(bare);
    return hit;
}

/* ── _compression_threshold_for_model ───────────────────────────────────── */
double
ac_compression_threshold_for_model(const char *model, const char *provider,
                                   int allow_codex_gpt55_autoraise)
{
    if (ac_is_arcee_trinity_thinking(model))
        return 0.75;
    if (allow_codex_gpt55_autoraise && ac_is_codex_gpt54_or_gpt55(model, provider))
        return CODEX_GPT54_GPT55_THRESHOLD;
    if (ac_is_codex_spark(model, provider))
        return CODEX_SPARK_THRESHOLD;
    return -1.0;
}

/* ── _model_recency_key ────────────────────────────────────────────────────
 * Python: re.split(r"(\d+(?:\.\d+)?)", model_id.lower()) filtered non-empty,
 * with each part tagged (0, 0.0, text) for text and (1, float(part), "") for
 * numbers. Emitting "t:<text>" / "n:<number>" preserves that ordering.
 */
void
ac_model_recency_key(const char *model_id, char *out, size_t cap)
{
    if (!model_id || !out || cap == 0)
        return;
    size_t w = 0;
    const char *p = model_id;
    int first = 1;

    while (*p) {
        if (isdigit((unsigned char)*p)) {
            /* \d+(?:\.\d+)? -- at most ONE decimal point per run. Consuming
             * every dot would fold "1.2.10" into a single 1.2 and drop the 10. */
            char num[64];
            size_t n = 0;
            while (isdigit((unsigned char)*p) && n + 1 < sizeof(num))
                num[n++] = *p++;
            if (*p == '.' && isdigit((unsigned char)p[1]) && n + 1 < sizeof(num)) {
                num[n++] = *p++;
                while (isdigit((unsigned char)*p) && n + 1 < sizeof(num))
                    num[n++] = *p++;
            }
            num[n] = '\0';
            double v = strtod(num, NULL);
            int written = snprintf(out + w, cap - w, "%sn:%.6f",
                                   first ? "" : "|", v);
            if (written > 0)
                w += (size_t)written;
            first = 0;
            continue;
        }
        if (!first && w + 2 < cap) {
            out[w++] = '|';
        }
        first = 0;
        if (w + 3 >= cap)
            break;
        out[w++] = 't';
        out[w++] = ':';
        while (*p && !isdigit((unsigned char)*p)) {
            char c = (char)tolower((unsigned char)*p++);
            if (w + 1 < cap)
                out[w++] = c;
        }
    }
    out[w] = '\0';
}

/* ── base_url_hostname / base_url_host_matches ─────────────────────────────
 * Ported from utils.py. `urlparse` tolerates a bare host by prepending "//";
 * `.hostname` drops the port and any userinfo and lower-cases.
 */
char *
ac_base_url_hostname(const char *base_url)
{
    const char *raw = (base_url && *base_url) ? base_url : "";
    while (isspace((unsigned char)*raw))
        raw++;
    size_t len = strlen(raw);
    while (len && isspace((unsigned char)raw[len - 1]))
        len--;
    if (len == 0) {
        /* urlparse("") -> None, but _hostname_of(None) -> "" -- an empty
         * string, not an absent value. */
        char *empty = (char *)malloc(1);
        if (empty)
            empty[0] = '\0';
        return empty;
    }

    /* skip the scheme */
    const char *auth = raw;
    const char *sep = strstr(raw, "://");
    if (sep)
        auth = sep + 3;

    /* authority ends at '/', '?' or '#' */
    size_t alen = 0;
    while (auth[alen] && auth[alen] != '/' && auth[alen] != '?' && auth[alen] != '#')
        alen++;

    /* drop userinfo up to the last '@' */
    size_t start = 0;
    for (size_t i = 0; i < alen; i++)
        if (auth[i] == '@')
            start = i + 1;

    /* drop the port */
    size_t hlen = alen - start;
    for (size_t i = start; i < alen; i++)
        if (auth[i] == ':') {
            hlen = i - start;
            break;
        }

    char *out = (char *)malloc(hlen + 1);
    if (!out)
        return NULL;
    /* IPv6 literals arrive bracketed; urlparse's .hostname strips them. */
    if (hlen && out != NULL && auth[start] == '[') {
        size_t close = 0;
        while (start + close < alen && auth[start + close] != ']')
            close++;
        start++;
        hlen = close > 0 ? close - 1 : 0;
    }
    for (size_t i = 0; i < hlen; i++)
        out[i] = (char)tolower((unsigned char)auth[start + i]);
    while (hlen && out[hlen - 1] == '.')
        hlen--;
    out[hlen] = '\0';
    return out;
}

int
ac_base_url_host_matches(const char *base_url, const char *domain)
{
    char *host = ac_base_url_hostname(base_url);
    if (!host || !*host) {
        free(host);
        return 0;
    }
    const char *d = (domain && *domain) ? domain : "";
    while (isspace((unsigned char)*d))
        d++;
    size_t dl = strlen(d);
    while (dl && (isspace((unsigned char)d[dl - 1]) || d[dl - 1] == '.'))
        dl--;

    int hit;
    if (dl == 0) {
        hit = 0;
    } else {
        char *dlower = (char *)malloc(dl + 1);
        if (!dlower) { free(host); return 0; }
        for (size_t i = 0; i < dl; i++)
            dlower[i] = (char)tolower((unsigned char)d[i]);
        dlower[dl] = '\0';
        size_t hl = strlen(host);
        hit = (hl == dl && strncmp(host, dlower, dl) == 0) ||
              (hl > dl + 1 && host[hl - dl - 1] == '.' &&
               strncmp(host + hl - dl, dlower, dl) == 0);
        free(dlower);
    }
    free(host);
    return hit;
}

int
ac_nvidia_nim_host(const char *base_url)
{
    return ac_base_url_host_matches(base_url, "integrate.api.nvidia.com");
}

/* ── _anthropic_event_has_content ───────────────────────────────────────── */
static const char *
ac_json_str(const json_t *obj, const char *key)
{
    if (!obj || obj->type != JSON_OBJECT)
        return NULL;
    json_t *v = json_obj_get(obj, key);
    if (!v || v->type != JSON_STRING)
        return NULL;
    return v->str_val;
}

/* Python `bool(value)`: a non-empty string, a non-zero number, true, or a
 * non-empty array/object all count. Only the falsy shapes fall through. */
static int
ac_json_truthy(const json_t *v)
{
    if (!v)
        return 0;
    switch (v->type) {
    case JSON_NULL:
        return 0;
    case JSON_BOOL:
        return v->bool_val ? 1 : 0;
    case JSON_NUMBER:
        return v->num_val != 0.0;
    case JSON_STRING:
        return v->str_val && v->str_val[0] ? 1 : 0;
    case JSON_ARRAY:
    case JSON_OBJECT:
        return v->c.count > 0;
    }
    return 0;
}

int
ac_anthropic_event_has_content(const json_t *event)
{
    const char *etype = ac_json_str(event, "type");

    if (etype && strcmp(etype, "content_block_delta") == 0) {
        json_t *delta = json_obj_get(event, "delta");
        static const char *const fields[] =
            {"text", "thinking", "partial_json", "signature", "citation"};
        for (size_t i = 0; i < sizeof(fields) / sizeof(fields[0]); i++)
            if (ac_json_truthy(json_obj_get(delta, fields[i])))
                return 1;
        return 0;
    }

    if (etype && strcmp(etype, "content_block_start") == 0) {
        json_t *block = json_obj_get(event, "content_block");
        const char *bt = ac_json_str(block, "type");
        if (!bt || strcmp(bt, "tool_use") != 0)
            return 0;
        return ac_json_truthy(json_obj_get(block, "id")) ||
               ac_json_truthy(json_obj_get(block, "name"));
    }
    return 0;
}

/* ── header builders ────────────────────────────────────────────────────── */
json_t *
ac_ai_gateway_headers(const char *base_version)
{
    json_t *h = json_object();
    if (!h)
        return NULL;
    char ua[256];
    version_user_agent("HermesAgent", base_version ? base_version : "",
                      ua, sizeof(ua));
    json_set(h, "User-Agent", json_string(ua));
    json_set(h, "HTTP-Referer", json_string("https://hermes-agent.nousresearch.com"));
    json_set(h, "X-Title", json_string("Hermes Agent"));
    return h;
}

static int
ac_truthy_env(const char *s)
{
    if (!s || !*s)
        return 0;
    /* Python does .strip().lower() before the membership test. */
    char low[64];
    size_t i = 0;
    while (s[i] && i + 1 < sizeof(low)) {
        low[i] = (char)tolower((unsigned char)s[i]);
        i++;
    }
    low[i] = '\0';
    return strcmp(low, "1") == 0 || strcmp(low, "true") == 0 ||
           strcmp(low, "yes") == 0 || strcmp(low, "on") == 0;
}

json_t *
ac_build_or_headers(const char *env_cache, int cfg_cache,
                    const char *env_ttl, double cfg_ttl)
{
    json_t *h = json_object();
    if (!h)
        return NULL;
    json_set(h, "HTTP-Referer", json_string("https://hermes-agent.nousresearch.com"));
    json_set(h, "X-Title", json_string("Hermes Agent"));
    json_set(h, "X-OpenRouter-Categories",
             json_string("productivity,cli-agent"));

    int enabled = env_cache && *env_cache ? ac_truthy_env(env_cache)
                                          : (cfg_cache != 0);
    if (!enabled)
        return h;

    json_set(h, "X-OpenRouter-Cache", json_string("true"));

    if (env_ttl && *env_ttl) {
        /* isdigit() over the WHOLE string, then 1..86400 */
        int alldig = 1;
        for (const char *p = env_ttl; *p; p++)
            if (!isdigit((unsigned char)*p)) { alldig = 0; break; }
        if (alldig) {
            long v = strtol(env_ttl, NULL, 10);
            if (v >= 1 && v <= 86400) {
                char buf[32];
                snprintf(buf, sizeof(buf), "%ld", v);
                json_set(h, "X-OpenRouter-Cache-TTL", json_string(buf));
            }
        }
    } else if (cfg_ttl >= 1.0 && cfg_ttl <= 86400.0) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%d", (int)cfg_ttl);
        json_set(h, "X-OpenRouter-Cache-TTL", json_string(buf));
    }
    return h;
}
