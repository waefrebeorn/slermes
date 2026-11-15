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

/* ── _model_recency_key comparison ─────────────────────────────────────────
 * The Python sorts by a tuple of (0, 0.0, text) / (1, float, "") tags, so
 * text tokens sort before numeric ones and numbers compare numerically --
 * a plain string compare of the canonical form would put "10" before "5.4".
 */
static int
ac_recency_cmp(const char *a, const char *b)
{
    const char *pa = a, *pb = b;
    for (;;) {
        if (!*pa && !*pb)
            return 0;
        if (!*pa)
            return -1;
        if (!*pb)
            return 1;

        int ta = (pa[0] == 't');
        int tb = (pb[0] == 't');
        const char *va = pa + 2, *vb = pb + 2;

        if (ta != tb)
            return ta ? -1 : 1;              /* text token before number token */

        if (ta) {
            /* compare the text up to the next '|' */
            const char *ea = strchr(va, '|');
            const char *eb = strchr(vb, '|');
            size_t la = ea ? (size_t)(ea - va) : strlen(va);
            size_t lb = eb ? (size_t)(eb - vb) : strlen(vb);
            size_t m = la < lb ? la : lb;
            int c = strncmp(va, vb, m);
            if (c)
                return c;
            if (la != lb)
                return la < lb ? -1 : 1;
        } else {
            double da = strtod(va, NULL), db = strtod(vb, NULL);
            if (da < db) return -1;
            if (da > db) return 1;
        }
        pa = strchr(pa, '|');
        pb = strchr(pb, '|');
        if (!pa && !pb) return 0;
        if (!pa) return -1;
        if (!pb) return 1;
        pa++; pb++;
    }
}

/* ── _fast_model_from_catalog: the selection core ──────────────────────────
 * Python sorts the live catalog ids by _model_recency_key (newest first) then
 * walks _FAST_MODEL_FAMILIES, returning the first id that contains the family
 * and none of the excludes. The fetch itself is network -- this is the pure
 * half, so it is testable without a catalog.
 */
void
ac_fast_model_select(const char **ids, int n, char *out, size_t cap)
{
    static const char *const families[] = {
        "gpt-mini-latest", "gpt-nano-latest", "claude-haiku-latest",
        "gemini-flash-latest", "gpt-5.4-nano", "gpt-5.4-mini", "gpt-5-mini",
        "haiku-4.5", "gemini-3.6-flash", "flash-lite",
        "-nano", "-mini", "-flash", "haiku",
    };
    static const char *const exclude[] = {
        "thinking", "reason", "-r1", "minilm", ":batch", ":free",
        "o1-", "o3-", "o4-", "codex", "audio", "-vl", "embed",
        "-tts", "-transcribe", "-realtime", "-image", "-search-preview",
    };
    if (!out || cap == 0)
        return;
    out[0] = '\0';
    if (!ids || n <= 0)
        return;

    /* index sort, newest first (stable so ties keep catalog order) */
    int *order = (int *)malloc(sizeof(int) * (size_t)n);
    char **keys = (char **)malloc(sizeof(char *) * (size_t)n);
    if (!order || !keys) { free(order); free(keys); return; }
    for (int i = 0; i < n; i++) {
        order[i] = i;
        keys[i] = NULL;
    }
    for (int i = 0; i < n; i++) {
        keys[i] = (char *)malloc(512);
        if (!keys[i]) { for (int k = 0; k < i; k++) free(keys[k]); free(order); free(keys); return; }
        ac_model_recency_key(ids[i], keys[i], 512);
    }
    /* simple insertion sort on the recency key, descending */
    for (int i = 1; i < n; i++) {
        int cur = order[i];
        char curkey[512];
        snprintf(curkey, sizeof(curkey), "%s", keys[cur]);
        int j = i - 1;
        while (j >= 0 && ac_recency_cmp(keys[order[j]], curkey) < 0) {
            order[j + 1] = order[j];
            j--;
        }
        order[j + 1] = cur;
    }

    for (size_t f = 0; f < sizeof(families) / sizeof(families[0]); f++) {
        for (int k = 0; k < n; k++) {
            const char *id = ids[order[k]];
            size_t il = strlen(id);
            char *low = (char *)malloc(il + 1);
            if (!low)
                continue;
            for (size_t x = 0; x < il; x++)
                low[x] = (char)tolower((unsigned char)id[x]);
            low[il] = '\0';
            if (strstr(low, families[f]) != NULL) {
                int bad = 0;
                for (size_t e = 0; e < sizeof(exclude) / sizeof(exclude[0]); e++)
                    if (strstr(low, exclude[e]) != NULL) { bad = 1; break; }
                if (!bad) {
                    snprintf(out, cap, "%s", id);
                    free(low);
                    goto done;
                }
            }
            free(low);
        }
    }
done:
    for (int i = 0; i < n; i++) free(keys[i]);
    free(order);
    free(keys);
}

/* ── _API_KEY_PROVIDER_AUX_MODELS_FALLBACK ───────────────────────────────── */
void
ac_aux_model_fallback(const char *provider, char *out, size_t cap)
{
    static const struct { const char *k, *v; } table[] = {
        {"gemini", "gemini-3.6-flash"},
        {"zai", "glm-4.5-flash"},
        {"kimi-coding", "kimi-k2-turbo-preview"},
        {"stepfun", "step-3.5-flash"},
        {"kimi-coding-cn", "kimi-k2-turbo-preview"},
        {"gmi", "google/gemini-3.1-flash-lite-preview"},
        {"anthropic", "claude-haiku-4-5-20251001"},
        {"ai-gateway", "google/gemini-3-flash"},
        {"opencode-zen", "gemini-3-flash"},
        {"opencode-go", "glm-5"},
        {"kilocode", "google/gemini-3.6-flash"},
        {"ollama-cloud", "nemotron-3-nano:30b"},
        {"tencent-tokenhub", "hy4-preview"},
        {"tencent-tokenplan", "hy4-preview"},
    };
    if (!out || cap == 0)
        return;
    out[0] = '\0';
    if (!provider)
        return;
    for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); i++)
        if (strcmp(provider, table[i].k) == 0) {
            snprintf(out, cap, "%s", table[i].v);
            return;
        }
}

/* ── _FAST_MODEL_TASKS / _PROVIDER_VISION_MODELS ────────────────────────── */
int
ac_task_can_prefer_fast(const char *task)
{
    return task && strcmp(task, "title_generation") == 0;
}

int
ac_provider_vision_static(const char *provider, char *out, size_t cap)
{
    static const struct { const char *k, *v; } table[] = {
        {"xiaomi", "mimo-v2.5"},
        {"zai", "glm-5.3-flash"},
    };
    if (!out || cap == 0)
        return 0;
    out[0] = '\0';
    if (!provider)
        return 0;
    for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); i++)
        if (strcmp(provider, table[i].k) == 0) {
            snprintf(out, cap, "%s", table[i].v);
            return 1;
        }
    return 0;
}

/* ── _is_free_model ─────────────────────────────────────────────────────── */
int
ac_is_free_model(const char *model)
{
    if (!model || !*model)
        return 0;
    /* str(model).strip() -- leading AND trailing whitespace is dropped first. */
    const char *s = model;
    while (isspace((unsigned char)*s))
        s++;
    size_t len = strlen(s);
    while (len && isspace((unsigned char)s[len - 1]))
        len--;
    if (len >= 5 && strncmp(s + len - 5, ":free", 5) == 0)
        return 1;
    return strncmp(s, "stealth/", 8) == 0;
}

/* ── _contains_any ──────────────────────────────────────────────────────── */
int
ac_contains_any(const char *text, const char **needles, int n)
{
    if (!text || !needles)
        return 0;
    for (int i = 0; i < n; i++)
        if (needles[i] && strstr(text, needles[i]) != NULL)
            return 1;
    return 0;
}

/* ── _normalize_chain_label ─────────────────────────────────────────────── */
void
ac_normalize_chain_label(const char *provider, char *out, size_t cap)
{
    static const struct { const char *k, *v; } aliases[] = {
        {"openrouter", "openrouter"},
        {"nous", "nous"},
        {"custom", "local/custom"},
        {"local/custom", "local/custom"},
        {"openai-codex", "openai-codex"},
        {"codex", "openai-codex"},
    };
    if (!out || cap == 0)
        return;
    out[0] = '\0';
    if (!provider || !*provider)
        return;

    char lower[256];
    size_t i = 0;
    while (provider[i] && i + 1 < sizeof(lower)) {
        lower[i] = (char)tolower((unsigned char)provider[i]);
        i++;
    }
    lower[i] = '\0';
    /* strip() before the lookup */
    char *s = lower;
    while (*s == ' ' || *s == '\t')
        s++;
    size_t l = strlen(s);
    while (l && (s[l - 1] == ' ' || s[l - 1] == '\t'))
        s[--l] = '\0';

    for (size_t k = 0; k < sizeof(aliases) / sizeof(aliases[0]); k++)
        if (strcmp(s, aliases[k].k) == 0) {
            snprintf(out, cap, "%s", aliases[k].v);
            return;
        }
    snprintf(out, cap, "%s", s);
}

/* ── _endpoint_speaks_anthropic_messages ────────────────────────────────── */
int
ac_endpoint_speaks_anthropic_messages(const char *base_url)
{
    if (!base_url)
        return 0;
    const char *s = base_url;
    while (isspace((unsigned char)*s))
        s++;
    size_t len = strlen(s);
    while (len && (s[len - 1] == '/' || isspace((unsigned char)s[len - 1])))
        len--;
    if (len == 0)
        return 0;

    char *norm = (char *)malloc(len + 1);
    if (!norm)
        return 0;
    for (size_t i = 0; i < len; i++)
        norm[i] = (char)tolower((unsigned char)s[i]);
    norm[len] = '\0';

    /* path ends with /anthropic or /anthropic/v1 */
    const char *auth = norm;
    const char *sep = strstr(norm, "://");
    if (sep)
        auth = sep + 3;
    const char *path = strchr(auth, '/');
    int hit = 0;
    if (path) {
        size_t pl = strlen(path);
        while (pl && path[pl - 1] == '/')
            pl--;
        if ((pl >= 10 && strncmp(path + pl - 10, "/anthropic", 10) == 0) ||
            (pl >= 13 && strncmp(path + pl - 13, "/anthropic/v1", 13) == 0))
            hit = 1;
    }

    char *host = ac_base_url_hostname(norm);
    if (!hit && host) {
        if (strcmp(host, "api.anthropic.com") == 0)
            hit = 1;
        else if (strcmp(host, "api.kimi.com") == 0 && strstr(norm, "/coding") != NULL)
            hit = 1;
    }
    free(host);
    free(norm);
    return hit;
}

/* ── _is_anthropic_compatible_host ──────────────────────────────────────── */
int
ac_is_anthropic_compatible_host(const char *url)
{
    if (!url || !*url)
        return 0;
    /* Raw urlparse, unlike base_url_hostname: a scheme-less string is parsed as
     * a PATH, so its .hostname is None. Only a URL with "://" has a host. */
    int hit = 0;
    if (strstr(url, "://") != NULL) {
        char *host = ac_base_url_hostname(url);
        if (host && strcmp(host, "api.anthropic.com") == 0)
            hit = 1;
        free(host);
    }

    if (!hit) {
        /* path ends with /anthropic or /anthropic/v1 */
        const char *s = url;
        while (isspace((unsigned char)*s))
            s++;
        size_t len = strlen(s);
        while (len && s[len - 1] == '/')
            len--;
        const char *auth = s;
        const char *sep = strstr(s, "://");
        if (sep)
            auth = sep + 3;
        const char *path = strchr(auth, '/');
        if (path) {
            size_t pl = strlen(path);
            while (pl && path[pl - 1] == '/')
                pl--;
            char *plower = (char *)malloc(pl + 1);
            if (plower) {
                for (size_t i = 0; i < pl; i++)
                    plower[i] = (char)tolower((unsigned char)path[i]);
                plower[pl] = '\0';
                if ((pl >= 10 && strncmp(plower + pl - 10, "/anthropic", 10) == 0) ||
                    (pl >= 13 && strncmp(plower + pl - 13, "/anthropic/v1", 13) == 0))
                    hit = 1;
                free(plower);
            }
        }
    }
    return hit;
}

/* ── _translate_anthropic_response_format ──────────────────────────────────
 * Merge an OpenAI response_format into Anthropic's output_config.
 * Mutates `anthropic_kwargs` in place, as the Python does.
 */
void
ac_translate_response_format(json_t *anthropic_kwargs, const json_t *response_format)
{
    if (!anthropic_kwargs || !response_format ||
        response_format->type != JSON_OBJECT)
        return;

    json_t *ft = json_obj_get(response_format, "type");
    const char *format_type = (ft && ft->type == JSON_STRING) ? ft->str_val : NULL;

    json_t *schema = NULL;
    if (format_type && strcmp(format_type, "json_schema") == 0) {
        json_t *js = json_obj_get(response_format, "json_schema");
        if (!js || js->type != JSON_OBJECT)
            return;
        if (!json_obj_get(js, "schema"))
            return;
        schema = json_obj_get(js, "schema");
    } else if (format_type && strcmp(format_type, "json_object") == 0) {
        /* Anthropic has no schema-less JSON mode. */
        schema = json_object();
        if (!schema)
            return;
        json_set(schema, "type", json_string("object"));
    } else {
        return;
    }

    json_t *oc = json_obj_get(anthropic_kwargs, "output_config");
    if (!oc || oc->type != JSON_OBJECT) {
        oc = json_object();
        if (!oc) {
            if (format_type && strcmp(format_type, "json_object") == 0)
                json_free(schema);
            return;
        }
        json_set(anthropic_kwargs, "output_config", oc);
    }

    json_t *fmt = json_object();
    if (!fmt) {
        if (format_type && strcmp(format_type, "json_object") == 0)
            json_free(schema);
        return;
    }
    json_set(fmt, "type", json_string("json_schema"));
    /* json_set takes ownership of the node, so hand over a copy when the
     * schema came from the caller's response_format. */
    json_set(fmt, "schema", json_copy(schema));
    json_set(oc, "format", fmt);
    if (format_type && strcmp(format_type, "json_object") == 0)
        json_free(schema);
}
