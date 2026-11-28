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
