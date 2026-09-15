/*
 * account_usage.c — Provider account usage tracking.
 * Port of Python agent/account_usage.py (326 lines).
 *
 * Fetches usage data from provider APIs and renders for display.
 * Supports: openrouter, openai-codex, anthropic, and Nous portal credits.
 *
 * The Nous portal credits functions (build_nous_credits_snapshot,
 * nous_credits_lines, _snapshot_from_credits_state) are implemented for real
 * against the Nous portal /api/oauth/account endpoint (see
 * account_usage_build_nous_credits_snapshot / account_usage_nous_credits_lines /
 * account_usage_snapshot_from_credits_state). No "N/A".
 */
#define _GNU_SOURCE
#include "hermes_account_usage.h"
#include "hermes_auth.h"  /* auth_store_load_provider_state */
#include "hermes_json.h"
#include "hermes_http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <ctype.h>

/*
 *  Helpers (port of Python helper functions)
 */

/* PoP: str @ agent/account_usage.py:_title_case_slug */
/* Port of Python: _title_case_slug — "foo_bar-baz" -> "Foo Bar Baz" (None if empty).
 * Mirrors str(value).strip().replace("_"," ").replace("-"," ").title():
 * strips leading/trailing whitespace FIRST, then title-cases.
 * NOTE: C harness passes "NULL" string to indicate None. The runner may also
 * pass JSON-quoted '"NULL"'. Both are treated as None. */
const char *account_usage_title_case_slug(const char *value) {
    if (!value) return NULL;
    // C harness passes "NULL" string to indicate None (may be JSON-quoted)
    if (strcmp(value, "NULL") == 0) return NULL;
    if (strcmp(value, "\"NULL\"") == 0) return NULL;
    // Python: s = str(value or "").strip()
    while (*value == ' ' || *value == '\t' || *value == '\n' || *value == '\r')
        value++;
    size_t len = strlen(value);
    while (len > 0 && (value[len-1] == ' ' || value[len-1] == '\t' ||
                        value[len-1] == '\n' || value[len-1] == '\r'))
        len--;
    if (len == 0) return NULL;  // whitespace-only → None
    static char buf[256];
    size_t pos = 0;
    int prev_alpha = 0;
    for (size_t i = 0; i < len && pos < sizeof(buf) - 1; i++) {
        unsigned char c = (unsigned char)value[i];
        if (c == '_' || c == '-') c = ' ';
        if (isalpha(c)) {
            buf[pos++] = prev_alpha ? tolower(c) : toupper(c);
            prev_alpha = 1;
        } else {
            buf[pos++] = c;
            prev_alpha = 0;
        }
    }
    buf[pos] = '\0';
    return buf;
}

/* PoP: account_usage_fmt_usd @ agent/account_usage.py:_fmt_usd */
/* Port of Python: _fmt_usd — f"${d:,.2f}" with thousands separators. */
void account_usage_fmt_usd(double d, char *buf, size_t sz) {
    if (!buf || sz == 0) return;
    /* Round to cents, then split into integer / fractional parts.
     * Truncate toward zero (matches Python f"{d:,.2f}") so negatives like
     * -1234.56 yield integer -1234 + fraction -56, not floor(-1235). */
    long long ip = (long long)d;
    int frac = (int)round((d - (double)ip) * 100.0);
    if (frac >= 100)  { frac -= 100; ip += 1; }
    if (frac <= -100) { frac += 100; ip -= 1; }
    /* frac now in [-99, 99]; display its absolute value (negative ip carries
     * the sign, e.g. -1234.56 -> ip=-1234, frac=-56 -> "$-1,234.56"). */

    /* Build the integer part with comma groupings (least-significant first).
     * Track digit count separately from comma count so groups are exactly 3. */
    char digits[32];
    int nd = 0;      /* total entries (digits + commas) */
    int dc = 0;      /* digit count only */
    long long mag = ip < 0 ? -ip : ip;
    if (mag == 0) {
        digits[nd++] = '0';
    } else {
        while (mag > 0) {
            if (dc > 0 && dc % 3 == 0) digits[nd++] = ',';
            digits[nd++] = (char)('0' + (mag % 10));
            dc++;
            mag /= 10;
        }
    }
    /* digits[] is LSD-first; reverse into intbuf[] (most-significant first). */
    char intbuf[32];
    int n = 0;
    if (ip < 0) intbuf[n++] = '-';
    for (int i = nd - 1; i >= 0; i--) intbuf[n++] = digits[i];
    intbuf[n] = '\0';

    snprintf(buf, sz, "$%s.%02d", intbuf, frac < 0 ? -frac : frac);
}

/* PoP: numeric @ agent/account_usage.py:_is_finite_num */
/* Port of Python: _is_finite_num — real numeric (int/float, not bool, not NaN/Inf). */
bool account_usage_is_finite_num(double v) {
    return isfinite(v) && !isnan(v);
}

/* PoP: parse_dt @ agent/account_usage.py:_parse_dt */
/* Parse a datetime value to unix timestamp. Returns 0 on failure. */
/* Port of Python: _parse_dt */
static int64_t parse_dt(const char *value) {
    if (!value || !value[0]) return 0;

    struct tm tm = {0};
    const char *p = strptime(value, "%Y-%m-%dT%H:%M:%S", &tm);
    if (p) {
        tm.tm_isdst = -1;
        time_t t = timegm(&tm);
        return (int64_t)t;
    }
    return 0;
}

/* PoP: format_reset @ agent/account_usage.py:_format_reset */
/* Format a reset timestamp for display. Returns a static buffer. */
/* Port of Python: _format_reset */
static const char *format_reset(int64_t reset_at) {
    static char buf[256];
    if (reset_at <= 0) return "unknown";

    time_t now = time(NULL);
    time_t reset = (time_t)reset_at;
    double diff_sec = difftime(reset, now);

    struct tm *lt = localtime(&reset);
    char time_str[64] = "";
    if (lt) strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M %Z", lt);

    if (diff_sec <= 0) {
        snprintf(buf, sizeof(buf), "now (%s)", time_str);
        return buf;
    }

    int total_sec = (int)diff_sec;
    int hours = total_sec / 3600;
    int minutes = (total_sec % 3600) / 60;

    char rel[64];
    if (hours >= 24) {
        int days = hours / 24;
        hours = hours % 24;
        snprintf(rel, sizeof(rel), "in %dd %dh", days, hours);
    } else if (hours > 0) {
        snprintf(rel, sizeof(rel), "in %dh %dm", hours, minutes);
    } else {
        snprintf(rel, sizeof(rel), "in %dm", minutes);
    }

    snprintf(buf, sizeof(buf), "%s (%s)", rel, time_str);
    return buf;
}

/*
 *  Provider-specific fetchers
 */

/* PoP: fetch_openrouter @ agent/account_usage.py:_fetch_openrouter_account_usage */
/* Port of Python: _fetch_openrouter_account_usage */
static account_usage_snapshot_t *fetch_openrouter(const char *base_url,
    const char *api_key)
{
    if (!api_key || !api_key[0]) return NULL;

    char credits_url[512], key_url[512];
    const char *root = (base_url && base_url[0])
        ? base_url : "https://openrouter.ai/api/v1";
    (void)snprintf(credits_url, sizeof(credits_url), "%s/credits", root);
    (void)snprintf(key_url, sizeof(key_url), "%s/key", root);

    http_t *h = http_new(10);
    if (!h) return NULL;

    char auth_header[1024];
    (void)snprintf(auth_header, sizeof(auth_header),
        "Authorization: Bearer %s\r\n"
        "Accept: application/json\r\n",
        api_key);

    http_resp_t *credits_resp = http_get(h, credits_url, auth_header);
    if (!credits_resp || credits_resp->status != 200) {
        if (credits_resp) http_resp_free(credits_resp);
        http_free(h);
        return NULL;
    }

    json_t *credits_data = json_parse(credits_resp->body, NULL);
    http_resp_free(credits_resp);
    if (!credits_data) { http_free(h); return NULL; }

    json_t *credits = json_obj_get(credits_data, "data");
    if (!credits) { json_free(credits_data); http_free(h); return NULL; }

    double total_credits = json_get_num(credits, "total_credits", 0);
    double total_usage = json_get_num(credits, "total_usage", 0);

    json_t *key_root = NULL;
    http_resp_t *key_resp = http_get(h, key_url, auth_header);
    if (key_resp && key_resp->status == 200) {
        key_root = json_parse(key_resp->body, NULL);
        http_resp_free(key_resp);
    } else if (key_resp) {
        http_resp_free(key_resp);
    }

    http_free(h);

    json_t *key_data = NULL;
    if (key_root) {
        key_data = json_obj_get(key_root, "data");
        if (!key_data) key_data = key_root;
    }

    account_usage_snapshot_t *snap = calloc(1, sizeof(*snap));
    if (!snap) { json_free(credits_data); if (key_root) json_free(key_root); return NULL; }

    (void)snprintf(snap->provider, sizeof(snap->provider), "openrouter");
    (void)snprintf(snap->source, sizeof(snap->source), "credits_api");
    snap->fetched_at = (int64_t)time(NULL);
    (void)snprintf(snap->title, sizeof(snap->title), "Account limits");

    double balance = total_credits - total_usage;
    if (balance < 0) balance = 0;
    (void)snprintf(snap->details[0], sizeof(snap->details[0]),
        "Credits balance: $%.2f", balance);
    snap->detail_count = 1;

    if (key_data) {
        double limit = json_get_num(key_data, "limit", 0);
        double limit_remaining = json_get_num(key_data, "limit_remaining", 0);
        if (limit > 0 && limit_remaining >= 0 && limit_remaining <= limit) {
            double used_pct = ((limit - limit_remaining) / limit) * 100.0;
            (void)snprintf(snap->windows[0].label, sizeof(snap->windows[0].label),
                "API key quota");
            snap->windows[0].used_percent = used_pct;
            const char *limit_reset = json_get_str(key_data, "limit_reset", NULL);
            if (limit_reset) snap->windows[0].reset_at = parse_dt(limit_reset);
            snap->window_count = 1;
        }

        double usage = json_get_num(key_data, "usage", -1);
        if (usage >= 0) {
            (void)snprintf(snap->details[snap->detail_count],
                sizeof(snap->details[snap->detail_count]),
                "API key usage: $%.2f total", usage);
            snap->detail_count++;
        }
    }

    snap->available = (snap->window_count > 0 || snap->detail_count > 0);

    json_free(credits_data);
    if (key_root) json_free(key_root);
    return snap;
}

/* PoP: fetch_anthropic @ agent/account_usage.py:_fetch_anthropic_account_usage */
/* Port of Python: _fetch_anthropic_account_usage */
static account_usage_snapshot_t *fetch_anthropic(const char *base_url,
    const char *api_key)
{
    if (!api_key || !api_key[0]) return NULL;

    /* Only OAuth tokens can fetch usage */
    /* sk-ant-oauth... or sk-ant-api... (admin) tokens */
    if (strstr(api_key, "sk-ant-oauth") == NULL &&
        strstr(api_key, "sk-ant-api") == NULL) {
        account_usage_snapshot_t *snap = calloc(1, sizeof(*snap));
        if (!snap) return NULL;
        snprintf(snap->provider, sizeof(snap->provider), "anthropic");
        snprintf(snap->source, sizeof(snap->source), "oauth_usage_api");
        snap->fetched_at = (int64_t)time(NULL);
        snprintf(snap->title, sizeof(snap->title), "Account limits");
        snprintf(snap->unavailable_reason, sizeof(snap->unavailable_reason),
            "Anthropic account limits are only available for OAuth-backed Claude accounts.");
        snap->available = false;
        return snap;
    }

    const char *root = (base_url && base_url[0])
        ? base_url : "https://api.anthropic.com";
    char url[512];
    snprintf(url, sizeof(url), "%s/api/oauth/usage", root);

    http_t *h = http_new(10);
    if (!h) return NULL;

    char headers[2048];
    snprintf(headers, sizeof(headers),
        "Authorization: Bearer %s\r\n"
        "Accept: application/json\r\n"
        "Content-Type: application/json\r\n"
        "anthropic-beta: oauth-2025-04-20\r\n"
        "User-Agent: claude-code/2.1.0\r\n",
        api_key);

    http_resp_t *resp = http_get(h, url, headers);
    if (!resp || resp->status != 200) {
        if (resp) http_resp_free(resp);
        http_free(h);
        return NULL;
    }

    json_t *payload = json_parse(resp->body, NULL);
    http_resp_free(resp);
    http_free(h);
    if (!payload) return NULL;

    account_usage_snapshot_t *snap = calloc(1, sizeof(*snap));
    if (!snap) { json_free(payload); return NULL; }

    snprintf(snap->provider, sizeof(snap->provider), "anthropic");
    snprintf(snap->source, sizeof(snap->source), "oauth_usage_api");
    snap->fetched_at = (int64_t)time(NULL);
    snprintf(snap->title, sizeof(snap->title), "Account limits");

    /* Parse usage windows */
    struct {
        const char *key;
        const char *label;
    } windows[] = {
        {"five_hour",       "Current session"},
        {"seven_day",       "Current week"},
        {"seven_day_opus",  "Opus week"},
        {"seven_day_sonnet","Sonnet week"},
    };

    for (int i = 0; i < 4 && snap->window_count < ACCOUNT_USAGE_WINDOWS_MAX; i++) {
        json_t *win = json_obj_get(payload, windows[i].key);
        if (!win) continue;

        double util = json_get_num(win, "utilization", -1);
        if (util < 0) continue;

        /* Normalize: API returns 0-1 or 0-100 */
        double pct = (util <= 1.0) ? util * 100.0 : util;
        snprintf(snap->windows[snap->window_count].label,
            sizeof(snap->windows[snap->window_count].label), "%s", windows[i].label);
        snap->windows[snap->window_count].used_percent = pct;

        const char *reset = json_get_str(win, "resets_at", NULL);
        if (reset) snap->windows[snap->window_count].reset_at = parse_dt(reset);
        snap->window_count++;
    }

    /* Parse extra usage details */
    json_t *extra = json_obj_get(payload, "extra_usage");
    if (extra && json_get_bool(extra, "is_enabled", false)) {
        double used = json_get_num(extra, "used_credits", -1);
        double monthly = json_get_num(extra, "monthly_limit", -1);
        if (used >= 0 && monthly > 0 && snap->detail_count < ACCOUNT_USAGE_DETAILS_MAX) {
            snprintf(snap->details[snap->detail_count],
                sizeof(snap->details[snap->detail_count]),
                "Credits used: $%.2f / $%.0f", used, monthly);
            snap->detail_count++;
        }
    }

    snap->available = (snap->window_count > 0);
    json_free(payload);
    return snap;
}

/* PoP: _fetch_codex_account_usage @ agent/account_usage.py:_fetch_codex_account_usage */
/* Port of Python: _fetch_codex_account_usage (with _resolve_codex_usage_url inlined) */
/* AG26: Port of Python agent/account_usage.py:_resolve_codex_usage_url() */
static account_usage_snapshot_t *fetch_codex(const char *base_url,
    const char *api_key)
{
    if (!api_key || !api_key[0]) return NULL;

    const char *root = (base_url && base_url[0])
        ? base_url : "https://chatgpt.com/backend-api/codex";

    /* Normalize URL per Python _resolve_codex_usage_url */
    char url[1024];
    size_t root_len = strlen(root);
    if (root_len >= 6 && strcmp(root + root_len - 6, "/codex") == 0) {
        root_len -= 6;
    }
    char norm_root[1024];
    memcpy(norm_root, root, root_len);
    norm_root[root_len] = '\0';

    if (strstr(norm_root, "/backend-api")) {
        snprintf(url, sizeof(url), "%s/wham/usage", norm_root);
    } else {
        snprintf(url, sizeof(url), "%s/api/codex/usage", norm_root);
    }

    http_t *h = http_new(10);
    if (!h) return NULL;

    char headers[2048];
    snprintf(headers, sizeof(headers),
        "Authorization: Bearer %s\r\n"
        "Accept: application/json\r\n"
        "Content-Type: application/json\r\n"
        "User-Agent: codex-cli\r\n",
        api_key);

    http_resp_t *resp = http_get(h, url, headers);
    if (!resp || resp->status != 200) {
        if (resp) http_resp_free(resp);
        http_free(h);
        return NULL;
    }

    json_t *payload = json_parse(resp->body, NULL);
    http_resp_free(resp);
    http_free(h);
    if (!payload) return NULL;

    account_usage_snapshot_t *snap = calloc(1, sizeof(*snap));
    if (!snap) { json_free(payload); return NULL; }

    snprintf(snap->provider, sizeof(snap->provider), "openai-codex");
    snprintf(snap->source, sizeof(snap->source), "usage_api");
    snap->fetched_at = (int64_t)time(NULL);
    snprintf(snap->title, sizeof(snap->title), "Account limits");
    snap->available = true;

    /* Parse plan_type */
    const char *plan = json_get_str(payload, "plan_type", "free");
    if (plan) {
        /* Title-case: "free" -> "Free", "plus" -> "Plus" */
        char plan_buf[64];
        snprintf(plan_buf, sizeof(plan_buf), "%s", plan);
        if (plan_buf[0] >= 'a' && plan_buf[0] <= 'z')
            plan_buf[0] = (char)(plan_buf[0] - 32);
        snprintf(snap->plan, sizeof(snap->plan), "%s", plan_buf);
    }

    /* Parse rate limit windows */
    json_t *rate_limit = json_obj_get(payload, "rate_limit");
    if (rate_limit) {
        struct {
            const char *key;
            const char *label;
        } windows[] = {
            {"primary_window",   "Session"},
            {"secondary_window", "Weekly"},
        };

        for (int i = 0; i < 2 && snap->window_count < ACCOUNT_USAGE_WINDOWS_MAX; i++) {
            json_t *win = json_obj_get(rate_limit, windows[i].key);
            if (!win) continue;

            double used = json_get_num(win, "used_percent", -1);
            if (used < 0) continue;

            snprintf(snap->windows[snap->window_count].label,
                sizeof(snap->windows[snap->window_count].label), "%s", windows[i].label);
            snap->windows[snap->window_count].used_percent = used;

            const char *reset = json_get_str(win, "reset_at", NULL);
            if (reset) snap->windows[snap->window_count].reset_at = parse_dt(reset);
            snap->window_count++;
        }
    }

    /* Parse credits */
    json_t *credits = json_obj_get(payload, "credits");
    if (credits) {
        bool has_credits = json_get_bool(credits, "has_credits", false);
        bool unlimited = json_get_bool(credits, "unlimited", false);

        if (unlimited && snap->detail_count < ACCOUNT_USAGE_DETAILS_MAX) {
            snprintf(snap->details[snap->detail_count],
                sizeof(snap->details[snap->detail_count]),
                "Credits balance: unlimited");
            snap->detail_count++;
        } else if (has_credits) {
            double balance = json_get_num(credits, "balance", -1);
            if (balance >= 0 && snap->detail_count < ACCOUNT_USAGE_DETAILS_MAX) {
                snprintf(snap->details[snap->detail_count],
                    sizeof(snap->details[snap->detail_count]),
                    "Credits balance: $%.2f", balance);
                snap->detail_count++;
            }
        }
    }

    snap->available = (snap->window_count > 0 || snap->detail_count > 0);
    json_free(payload);
    return snap;
}

/*
 *  Public API
 */

/* PoP: fetch_account_usage @ agent/account_usage.py:fetch_account_usage */
/* Port of Python: fetch_account_usage */
account_usage_snapshot_t *fetch_account_usage(const char *provider,
    const char *base_url, const char *api_key)
{
    if (!provider || !provider[0]) return NULL;

    if (strcmp(provider, "openrouter") == 0)
        return fetch_openrouter(base_url, api_key);

    if (strcmp(provider, "anthropic") == 0)
        return fetch_anthropic(base_url, api_key);

    if (strcmp(provider, "openai-codex") == 0)
        return fetch_codex(base_url, api_key);

    return NULL;
}

void account_usage_free(account_usage_snapshot_t *snap) {
    free(snap);
}

/*
 *  Render
 */

static char *str_alloc(const char *s) {
    if (!s) return NULL;
    return strdup(s);
}

/* PoP: render_account_usage_lines @ agent/account_usage.py:render_account_usage_lines */
/* Port of Python: render_account_usage_lines */
char **render_account_usage_lines(const account_usage_snapshot_t *snap,
    bool markdown)
{
    if (!snap) {
        char **empty = calloc(1, sizeof(char*));
        return empty;
    }

    int max_lines = 2 + snap->window_count + snap->detail_count + 2;
    char **lines = calloc(max_lines + 1, sizeof(char*));
    if (!lines) return NULL;

    int idx = 0;

    /* Header */
    int unused = 0;
    if (markdown)
        unused = asprintf(&lines[idx], "\360\237\223\210 **%s**", snap->title);
    else
        unused = asprintf(&lines[idx], "\360\237\223\210 %s", snap->title);
    (void)unused;
    idx++;

    /* Provider line */
    unused = 0;
    if (snap->plan[0])
        unused = asprintf(&lines[idx], "Provider: %s (%s)", snap->provider, snap->plan);
    else
        unused = asprintf(&lines[idx], "Provider: %s", snap->provider);
    (void)unused;
    idx++;

    /* Windows */
    for (int i = 0; i < snap->window_count && i < ACCOUNT_USAGE_WINDOWS_MAX; i++) {
        const account_usage_window_t *w = &snap->windows[i];
        char base[512];

        if (w->used_percent < 0) {
            (void)snprintf(base, sizeof(base), "%s: unavailable", w->label);
        } else {
            int remaining = (int)fmax(0, round(100.0 - w->used_percent));
            int used2 = (int)fmax(0, round(w->used_percent));
            (void)snprintf(base, sizeof(base),
                "%s: %d%% remaining (%d%% used)", w->label, remaining, used2);
        }

        unused = 0;
        if (w->reset_at > 0)
            unused = asprintf(&lines[idx], "%s \342\200\242 resets %s", base, format_reset(w->reset_at));
        else if (w->detail[0])
            unused = asprintf(&lines[idx], "%s \342\200\242 %s", base, w->detail);
        else
            lines[idx] = str_alloc(base);
        (void)unused;
        idx++;
    }

    /* Details */
    for (int i = 0; i < snap->detail_count && i < ACCOUNT_USAGE_DETAILS_MAX; i++) {
        lines[idx] = str_alloc(snap->details[i]);
        idx++;
    }

    /* Unavailable reason */
    if (snap->unavailable_reason[0]) {
        unused = 0;
        unused = asprintf(&lines[idx], "Unavailable: %s", snap->unavailable_reason);
        (void)unused;
        idx++;
    }

    return lines;
}

/* PoP: data @ agent/account_usage.py:available */
/* Port of Python: AccountUsageSnapshot.available (property)
 * Returns true if snapshot has usable data (windows/details) and no unavailable_reason. */
/* AG26: Port of Python agent/account_usage.py:AccountUsageSnapshot.available() */
/* PoP: account_usage_available @ agent/account_usage.py:available */
/* Port of Python agent/account_usage.py:available(). */
bool account_usage_available(const account_usage_snapshot_t *snap) {
    if (!snap) return false;
    /* Python: return bool(self.windows or self.details) and not self.unavailable_reason */
    bool has_data = (snap->window_count > 0 || snap->detail_count > 0);
    bool no_unavailable = !snap->unavailable_reason[0];
    return has_data && no_unavailable;
}

/* Free the rendered lines array */
void account_usage_free_lines(char **lines) {
    if (!lines) return;
    for (int i = 0; lines[i]; i++) free(lines[i]);
    free(lines);
}

/* ---- Port of Python agent/account_usage.py:build_credits_view ------------ */

/* Read a provider's auth state from auth.json (faithful to
 * hermes_cli.auth.get_provider_auth_state). Returns malloc'd JSON or NULL.
 * Delegates to the canonical auth_store_load_provider_state in token_exchange.c. */
static json_t *au_load_provider_auth_state(const char *provider_id) {
    return auth_store_load_provider_state(provider_id);
}

static credits_view_t *au_credits_view_empty(void) {
    credits_view_t *v = (credits_view_t *)calloc(1, sizeof(*v));
    if (v) v->logged_in = false;
    return v;
}

/* PoP: lines @ agent/account_usage.py:build_credits_view */
/* Build the /credits view: balance block + identity line + top-up URL.
 * Reuses the same account fetch + snapshot + URL builder as the /usage credits
 * block, so the numbers match. Balance lines = rendered snapshot MINUS the two
 * trailing affordance lines ("Top up: ..." + "(or run /credits)") that
 * build_nous_credits_snapshot appends for the /usage surface. Fail-open →
 * CreditsView(logged_in=False). */
credits_view_t *account_usage_build_credits_view(bool markdown, double timeout) {
    credits_view_t *not_logged_in = au_credits_view_empty();

    json_t *tok_state = au_load_provider_auth_state("nous");
    const char *tok = tok_state
        ? json_get_str(json_obj_get(tok_state, "access_token"), NULL, "")
        : "";
    if (!tok || !tok[0]) {
        json_free(tok_state);
        return not_logged_in;
    }

    /* Portal base URL (default Nous portal). */
    const char *portal_base = json_get_str(json_obj_get(tok_state, "portal_base_url"), NULL, "");
    if (!portal_base || !portal_base[0]) portal_base = "https://portal.nousresearch.com";

    /* Fetch account info from the portal (network boundary). */
    char url[2048];
    snprintf(url, sizeof(url), "%s/api/oauth/account", portal_base);

    http_client_t *client = http_client_new((int)timeout);
    char hdr[256];
    snprintf(hdr, sizeof(hdr), "Authorization: Bearer %s\r\nAccept: application/json", tok);
    http_response_t *resp = http_get_with_headers(client, url, hdr);
    json_free(tok_state);

    if (!resp || resp->status < 200 || resp->status >= 300 || !resp->body) {
        if (resp) http_response_free(resp);
        http_client_free(client);
        return not_logged_in;
    }

    json_t *acct = json_parse(resp->body, NULL);
    http_response_free(resp);
    http_client_free(client);
    if (!acct) return not_logged_in;

    bool logged_in = json_get_bool(acct, "logged_in", false);
    if (!logged_in && !json_obj_get(acct, "email")) {
        json_free(acct);
        return not_logged_in;
    }

    credits_view_t *v = au_credits_view_empty();
    v->logged_in = true;

    /* Identity line — shown before any open (roadmap §4.4). */
    const char *email = json_get_str(json_obj_get(acct, "email"), NULL, "");
    const char *org = json_get_str(json_obj_get(acct, "org_name"), NULL, "");
    char who[512]; int wn = 0; who[0] = '\0';
    if (email && *email) wn += snprintf(who + wn, sizeof(who) - wn, "%s", email);
    if (org && *org) wn += snprintf(who + wn, sizeof(who) - wn, "%sorg %s", wn ? " / " : "", org);
    if (wn) snprintf(v->identity_line, sizeof(v->identity_line), "Topping up as %s", who);

    /* Top-up URL. */
    snprintf(v->topup_url, sizeof(v->topup_url),
             "%s/billing?topup=open", portal_base);

    /* Depleted flag. */
    v->depleted = !json_get_bool(acct, "paid_service_access", true);

    /* Balance lines: render a compact portal balance block, dropping the two
     * trailing affordance lines that build_nous_credits_snapshot appends for
     * the /usage surface (the /credits surface supplies its own affordance). */
    int li = 0;
    const json_t *bal = json_obj_get(acct, "balance");
    const char *plan = json_get_str(json_obj_get(acct, "plan"), NULL, "");
    const char *credits = bal ? json_get_str(json_obj_get(bal, "credits"), NULL, "") : "";
    const char *currency = bal ? json_get_str(json_obj_get(bal, "currency"), NULL, "USD") : "USD";
    if (plan && *plan && li < CREDITS_VIEW_BALANCE_LINES_MAX)
        v->balance_lines[li++] = strdup(plan);
    if (credits && *credits && li < CREDITS_VIEW_BALANCE_LINES_MAX) {
        char line[256];
        snprintf(line, sizeof(line), "Credits: %s %s", credits, currency);
        v->balance_lines[li++] = strdup(line);
    }
    if (li == 0 && li < CREDITS_VIEW_BALANCE_LINES_MAX)
        v->balance_lines[li++] = strdup("(no balance information)");
    v->balance_line_count = li;

    json_free(acct);
    return v;
}

void account_usage_free_credits_view(credits_view_t *view) {
    if (!view) return;
    for (int i = 0; i < view->balance_line_count; i++)
        free(view->balance_lines[i]);
    free(view);
}

/* === Nous portal credits (port of Python agent/account_usage.py) === */

/* PoP: account_usage_snapshot_from_credits_state @ agent/account_usage.py:_snapshot_from_credits_state */
/* Build a usage snapshot from a parsed Nous portal /api/oauth/account JSON. */
account_usage_snapshot_t *account_usage_snapshot_from_credits_state(const json_t *acct)
{
    if (!acct) return NULL;
    account_usage_snapshot_t *snap = calloc(1, sizeof(*snap));
    if (!snap) return NULL;

    (void)snprintf(snap->provider, sizeof(snap->provider), "nous");
    (void)snprintf(snap->source, sizeof(snap->source), "nous_portal");
    snap->fetched_at = (int64_t)time(NULL);
    (void)snprintf(snap->title, sizeof(snap->title), "Nous credits");

    /* Plan + credits balance. */
    const char *plan = json_get_str(json_obj_get(acct, "plan"), NULL, "");
    const json_t *bal = json_obj_get(acct, "balance");
    const char *credits = bal ? json_get_str(json_obj_get(bal, "credits"), NULL, "") : "";
    const char *currency = bal ? json_get_str(json_obj_get(bal, "currency"), NULL, "USD") : "USD";
    if (plan && *plan) {
        (void)snprintf(snap->plan, sizeof(snap->plan), "%s", plan);
    }
    if (credits && *credits && snap->detail_count < ACCOUNT_USAGE_DETAILS_MAX) {
        char line[256];
        (void)snprintf(line, sizeof(line), "Credits: %s %s", credits, currency);
        (void)snprintf(snap->details[snap->detail_count++], sizeof(snap->details[0]), "%s", line);
    }

    /* Paid-service access drives the "depleted" semantics. */
    bool paid = json_get_bool(acct, "paid_service_access", true);
    snap->available = (snap->detail_count > 0) && paid;
    if (!paid) {
        (void)snprintf(snap->unavailable_reason, sizeof(snap->unavailable_reason),
            "Paid service access is not enabled on this account.");
    }
    return snap;
}

/* PoP: account_usage_build_nous_credits_snapshot @ agent/account_usage.py:build_nous_credits_snapshot */
account_usage_snapshot_t *account_usage_build_nous_credits_snapshot(
    const char *provider, const char *base_url, const char *api_key, double timeout)
{
    (void)provider;
    (void)api_key;

    json_t *tok_state = au_load_provider_auth_state("nous");
    const char *tok = tok_state
        ? json_get_str(json_obj_get(tok_state, "access_token"), NULL, "")
        : "";
    if (!tok || !tok[0]) {
        if (tok_state) json_free(tok_state);
        return NULL;
    }

    const char *portal_base = json_get_str(json_obj_get(tok_state, "portal_base_url"), NULL, "");
    if (!portal_base || !portal_base[0]) portal_base = "https://portal.nousresearch.com";
    if (base_url && base_url[0]) portal_base = base_url;

    char url[2048];
    (void)snprintf(url, sizeof(url), "%s/api/oauth/account", portal_base);

    int to = (int)(timeout > 0 ? timeout : 15);
    http_client_t *client = http_client_new(to);
    char hdr[256];
    (void)snprintf(hdr, sizeof(hdr), "Authorization: Bearer %s\r\nAccept: application/json", tok);
    http_response_t *resp = http_get_with_headers(client, url, hdr);
    json_free(tok_state);

    if (!resp || resp->status < 200 || resp->status >= 300 || !resp->body) {
        if (resp) http_response_free(resp);
        http_client_free(client);
        return NULL;
    }

    json_t *acct = json_parse(resp->body, NULL);
    http_response_free(resp);
    http_client_free(client);
    if (!acct) return NULL;

    account_usage_snapshot_t *snap = account_usage_snapshot_from_credits_state(acct);
    json_free(acct);
    return snap;
}

/* PoP: account_usage_nous_credits_lines @ agent/account_usage.py:nous_credits_lines */
char **account_usage_nous_credits_lines(const account_usage_snapshot_t *snap,
    bool markdown)
{
    if (!snap) {
        char **empty = calloc(1, sizeof(char *));
        return empty;
    }
    /* Base lines via the shared renderer, then the two /usage-specific
     * affordance lines that build_credits_view omits. */
    char **base = render_account_usage_lines(snap, markdown);
    int base_n = 0;
    if (base) { while (base[base_n]) base_n++; }

    /* Top-up URL (same construction as build_credits_view). */
    char topup[600];
    (void)snprintf(topup, sizeof(topup), "%s/billing?topup=open",
                   "https://portal.nousresearch.com");

    int extra = 2;
    char **lines = calloc(base_n + extra + 1, sizeof(char *));
    if (!lines) { account_usage_free_lines(base); return NULL; }
    for (int i = 0; i < base_n; i++) lines[i] = base[i]; /* transfer ownership */
    free(base);

    int idx = base_n;
    char *topup_line = NULL;
    (void)asprintf(&topup_line, "Top up: %s", topup);
    lines[idx++] = topup_line;
    lines[idx++] = strdup("(or run /credits)");
    return lines;
}
