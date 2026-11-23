/*
 * provider_media.c — C port of upstream Hermes agent/provider_media.py.
 *
 * PoP: w7_providermed_9f3e4f_generated_subdir @ agent/provider_media.py:GENERATED_SUBDIR
 * PoP: w7_providermed_9f3e4f_media_cache_max_age_hours @ agent/provider_media.py:MEDIA_CACHE_MAX_AGE_HOURS
 * PoP: w7_providermed_9f3e4f_max_save_url_redirects @ agent/provider_media.py:_MAX_SAVE_URL_REDIRECTS
 * PoP: w7_providermed_9f3e4f_is_redirect_status @ agent/provider_media.py:_REDIRECT_STATUS_CODES
 * PoP: w7_providermed_9f3e4f_cache_path_ex @ agent/provider_media.py:cache_path
 *
 * cache_path's clock/uuid/home inputs are injected explicitly (the oracle
 * freezes them on the live module); the path spelling and bounded-name layout
 * are the real behaviour. Network download (save_url) is intentionally not
 * projected: it needs sockets, SSRF policy and a response stream.
 */
#include "hermes_json.h"
#include "noc_w7_providermed_9f3e4f.h"
#include <stdlib.h>
#include <string.h>

#define GENERATED_SUBDIR "generated"
#define MEDIA_CACHE_MAX_AGE_HOURS 24
#define MAX_SAVE_URL_REDIRECTS 5

struct json_t *w7_providermed_9f3e4f_generated_subdir(void) {
    return json_string(GENERATED_SUBDIR);
}
struct json_t *w7_providermed_9f3e4f_media_cache_max_age_hours(void) {
    return json_number(MEDIA_CACHE_MAX_AGE_HOURS);
}
struct json_t *w7_providermed_9f3e4f_max_save_url_redirects(void) {
    return json_number(MAX_SAVE_URL_REDIRECTS);
}
struct json_t *w7_providermed_9f3e4f_is_redirect_status(int code) {
    return json_bool(code==301||code==302||code==303||code==307||code==308 ? true : false);
}

/* <home>/cache/<kind>/<prefix>_<ts>_<short>.<ext>, Windows spelling. */
struct json_t *w7_providermed_9f3e4f_cache_path_ex(const char *home, const char *kind, const char *prefix,
                                                   const char *ts, const char *short_id, const char *extension) {
    const char *h = home ? home : "";
    const char *k = kind ? kind : "";
    const char *p = prefix ? prefix : "";
    const char *t = ts ? ts : "";
    const char *s = short_id ? short_id : "";
    const char *e = extension ? extension : "";
    size_t cap = strlen(h)+strlen(k)+strlen(p)+strlen(t)+strlen(s)+strlen(e)+64;
    char *out = (char *)malloc(cap), *q = out;
    #define PUT(c) (*q++ = (c))
    #define PUTS(str) do { const char *_x=(str); while (*_x) PUT(*_x++); } while (0)
    for (const char *c = h; *c; c++) PUT(*c=='/' ? '\\' : *c);
    while (q > out && q[-1]=='\\') q--;
    PUTS("\\cache\\");
    for (const char *c = k; *c; c++) PUT(*c=='/' ? '\\' : *c);
    PUT('\\'); PUTS(p); PUT('_'); PUTS(t); PUT('_'); PUTS(s); PUT('.'); PUTS(e);
    PUT('\0');
    #undef PUTS
    #undef PUT
    json_t *r = json_string(out); free(out); return r;
}
