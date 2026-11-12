#ifndef SLERMES_PORT_WEB_COMMON_H
#define SLERMES_PORT_WEB_COMMON_H

#include <hermes_json.h>

/* plugins/web/_common.py — shared plumbing for the bundled web-search
 * provider plugins: the result shapes (key order is part of the contract, it
 * reaches the model as JSON), the origin-HTTP-status classifiers, and the
 * `hermes tools` picker schemas. Every function returns a NEW json_t the
 * caller owns; string/NULL arguments are borrowed. */

json_t *webc_search_ok(json_t *web_results);
json_t *webc_search_fail(const char *error);
json_t *webc_web_hit(const char *url, const char *title, const char *description,
                     long position);
json_t *webc_title_hit(const char *title, const char *url, const char *description,
                       long position);
json_t *webc_document(const char *url, const char *title, const char *content,
                      const char *source_url);        /* NULL -> url */
json_t *webc_page_error(const char *url, const char *error);
json_t *webc_extract_fail(json_t *urls, const char *error);

int webc_is_origin_http_error(const char *error);
/* "HTTP <code> from origin[: detail]" for a >=400 target answer, else null. */
json_t *webc_origin_http_error(json_t *status, const char *reason);
/* Exa CRAWL_HTTP_<code> / CRAWL_NOT_FOUND -> code, else null. */
json_t *webc_exa_crawl_status(const char *text);
/* Keenable message that describes the target's own answer -> origin error. */
json_t *webc_keenable_origin_error(json_t *status, const char *body);

/* Mutates `entry` in place and returns it. */
json_t *webc_with_origin_status(json_t *entry, json_t *status, const char *reason);
json_t *webc_vendor_error_entry(const char *url, json_t *status,
                                const char *reason, const char *body);

/* Brave/SearXNG row normalizer: str() every field, 1-based positions. */
json_t *webc_titled_rows(json_t *raw_results, const char *description_key);
json_t *webc_extract_interrupted(json_t *urls);

json_t *webc_setup_schema(const char *name, const char *badge, const char *tag,
                          const char *key_env, const char *prompt, const char *url,
                          json_t *extra);
json_t *webc_keyless_variant_schema(const char *display, const char *key_env,
                                    const char *key_url, const char *free_tag,
                                    const char *paid_tag);

#endif /* SLERMES_PORT_WEB_COMMON_H */
