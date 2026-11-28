#include "crypto.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hermes_json.h"

/* PoP: cron_incidents_u_incident_id @ cron/incidents.py:_incident_id */
/* f"{job_id[:6]}_{error_sig}" -- first 6 CODEPOINTS of job_id. */
char *cron_incidents_u_incident_id(const char *arg) {
    json_t *o = arg ? json_parse(arg, NULL) : NULL;
    if (!o || !json_is_object(o)) { if (o) json_free(o); return strdup("_"); }
    json_t *jv = json_obj_get(o, "job_id"), *ev = json_obj_get(o, "error_sig");
    const char *job = (jv && json_is_string(jv) && jv->str_val) ? jv->str_val : "";
    const char *sig = (ev && json_is_string(ev) && ev->str_val) ? ev->str_val : "";
    size_t cut = 0; int seen = 0;
    for (size_t i = 0; job[i] && seen < 6; ) {
        unsigned char b = (unsigned char)job[i]; int len = 1;
        if (b >= 0xF0) len = 4; else if (b >= 0xE0) len = 3; else if (b >= 0xC0) len = 2;
        i += (size_t)len; cut = i; seen++;
    }
    size_t need = cut + strlen(sig) + 2;
    char *r = malloc(need);
    snprintf(r, need, "%.*s_%s", (int)cut, job, sig);
    json_free(o);
    return r;
}

/* PoP: cron_incidents_u_normalize_error @ cron/incidents.py:_normalize_error */
/* re.sub(r"\s+", " ", str(error or "")).strip().lower() -- collapse every run of
 * unicode whitespace to ONE space, strip the ends, lowercase (ASCII fold). */
static int _ne_ws(unsigned c) {
    return (c>=0x9&&c<=0xD)||(c>=0x1C&&c<=0x20)||c==0x85||c==0xA0||c==0x1680||
           (c>=0x2000&&c<=0x200A)||c==0x2028||c==0x2029||c==0x202F||c==0x205F||c==0x3000;
}
static unsigned _ne_dec(const char *s, size_t n, size_t i, int *len) {
    unsigned char b = (unsigned char)s[i];
    if (b < 0x80) { *len = 1; return b; }
    if ((b&0xe0)==0xc0 && i+1<n) { *len=2; return ((b&0x1f)<<6)|((unsigned char)s[i+1]&0x3f); }
    if ((b&0xf0)==0xe0 && i+2<n) { *len=3; return ((b&0x0f)<<12)|(((unsigned char)s[i+1]&0x3f)<<6)|((unsigned char)s[i+2]&0x3f); }
    *len=1; return b;
}
char *cron_incidents_u_normalize_error(const char *arg) {
    const char *s = arg ? arg : "";
    size_t n = strlen(s), cap = n+8, w = 0;
    char *out = malloc(cap); out[0] = 0;
    size_t i = 0; int pending_space = 0;
    while (i < n) {
        int l; unsigned c = _ne_dec(s, n, i, &l);
        if (_ne_ws(c)) { if (w > 0) pending_space = 1; i += (size_t)l; continue; }
        if (pending_space) { out[w++] = ' '; pending_space = 0; }
        for (int k = 0; k < l; k++) { char ch = s[i + (size_t)k]; out[w++] = (ch>='A'&&ch<='Z') ? (char)(ch+32) : ch; }
        i += (size_t)l;
    }
    out[w] = 0;
    return out;
}

static void _hx2(const unsigned char *d, char *out, int n) {
    static const char *H = "0123456789abcdef";
    for (int i = 0; i < n; i++) { out[2*i] = H[(d[i] >> 4) & 15]; out[2*i+1] = H[d[i] & 15]; }
    out[2*n] = 0;
}

/* PoP: cron_incidents_u_error_signature @ cron/incidents.py:_error_signature */
/* normalized = _DURATION_RE.sub("#s", _normalize_error(error))[:200]
 * return sha256(job_id + normalized).hexdigest()[:12]
 *
 * _DURATION_RE = \b\d+(?:\.\d+)?\s*(?:ms|s|secs?|seconds?|m|mins?|minutes?|h|hrs?|hours?)\b
 * -- alternation order matters: for "seconds" the "s" alternative matches but fails its
 * trailing \b, so the engine backtracks to "secs?"/"seconds?" and only then accepts.
 * The unit list below is that written order, greedy-variants first. */
static const char *ES_UNITS[] = {"ms","s","secs","sec","seconds","second","m","mins","min",
    "minutes","minute","h","hrs","hr","hours","hour"};
static int _es_word(unsigned c) { return (c>='0'&&c<='9')||(c>='a'&&c<='z')||(c>='A'&&c<='Z')||c=='_'||c>=0x80; }
static int _es_digit(unsigned c) { return c>='0'&&c<='9'; }
static int _es_ws(unsigned c) {
    return (c>=0x9&&c<=0xD)||(c>=0x1C&&c<=0x20)||c==0x85||c==0xA0||c==0x1680||
           (c>=0x2000&&c<=0x200A)||c==0x2028||c==0x2029||c==0x202F||c==0x205F||c==0x3000;
}
/* decode the normalized string into codepoints */
static int _es_cps(const char *s, unsigned *out, int max) {
    int k = 0; size_t n = strlen(s);
    for (size_t i = 0; i < n && k < max; ) {
        unsigned char b = (unsigned char)s[i]; unsigned cp = b; int len = 1;
        if (b >= 0xF0) { cp = b & 0x07; len = 4; }
        else if (b >= 0xE0) { cp = b & 0x0f; len = 3; }
        else if (b >= 0xC0) { cp = b & 0x1f; len = 2; }
        for (int j = 1; j < len && i + (size_t)j < n; j++) cp = (cp << 6) | ((unsigned char)s[i+j] & 0x3f);
        out[k++] = cp; i += (size_t)len;
    }
    return k;
}
char *cron_incidents_u_error_signature(const char *arg) {
    json_t *o = arg ? json_parse(arg, NULL) : NULL;
    const char *job = "", *err = "";
    if (o && json_is_object(o)) {
        json_t *j = json_obj_get(o, "job_id"), *e = json_obj_get(o, "error");
        if (j && json_is_string(j) && j->str_val) job = j->str_val;
        if (e && json_is_string(e) && e->str_val) err = e->str_val;
    }
    char *norm = cron_incidents_u_normalize_error(err);       /* strip/collapse/lower */
    unsigned cp[8192]; int n = _es_cps(norm, cp, 8192);
    unsigned res[8192]; int m = 0, i = 0;
    while (i < n) {
        if ((i == 0 || !_es_word(cp[i-1])) && _es_digit(cp[i])) {
            int j = i; while (j < n && _es_digit(cp[j])) j++;
            if (j < n && cp[j] == '.') { int k2 = j + 1;
                if (k2 < n && _es_digit(cp[k2])) { while (k2 < n && _es_digit(cp[k2])) k2++; j = k2; } }
            int k = j; while (k < n && _es_ws(cp[k])) k++;
            int hit = 0, consumed = 0;
            for (int u = 0; u < (int)(sizeof(ES_UNITS)/sizeof(ES_UNITS[0])) && !hit; u++) {
                const char *unit = ES_UNITS[u]; int len = (int)strlen(unit);
                if (k + len > n) continue;
                int ok = 1;
                for (int t = 0; t < len; t++) if (cp[k+t] != (unsigned char)unit[t]) { ok = 0; break; }
                if (!ok) continue;
                if (k + len < n && _es_word(cp[k+len])) continue;   /* trailing \b */
                hit = 1; consumed = k + len;
            }
            if (hit) { res[m++] = '#'; res[m++] = 's'; i = consumed; continue; }
        }
        res[m++] = cp[i++];
    }
    int lim = m > 200 ? 200 : m;
    /* re-encode the first lim codepoints to UTF-8 */
    size_t cap = (size_t)lim * 4 + 8, w = 0;
    char *flat = malloc(cap);
    for (int a = 0; a < lim; a++) {
        unsigned c = res[a];
        if (c < 0x80) flat[w++] = (char)c;
        else if (c < 0x800) { flat[w++] = (char)(0xC0|(c>>6)); flat[w++] = (char)(0x80|(c&0x3F)); }
        else { flat[w++] = (char)(0xE0|(c>>12)); flat[w++] = (char)(0x80|((c>>6)&0x3F)); flat[w++] = (char)(0x80|(c&0x3F)); }
    }
    flat[w] = 0;
    size_t need = strlen(job) + w + 1;
    char *buf = malloc(need);
    snprintf(buf, need, "%s%s", job, flat);
    unsigned char d[32]; crypto_sha256((const unsigned char *)buf, strlen(buf), d);
    char hex[33]; _hx2(d, hex, 32);
    char *out = malloc(16); snprintf(out, 16, "%.12s", hex);
    free(norm); free(flat); free(buf); if (o) json_free(o);
    return out;
}
