#ifndef NOC_W15_WEBSERVER_995DB6_H
#define NOC_W15_WEBSERVER_995DB6_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w15_webserver_995db6_path_text @ hermes_cli/web_server_files.py:_path_text
 * Returns {"ok":true,"value":...} or {"error":..,"status":400}. */
struct json_t *w15_webserver_995db6_path_text(const char *raw, int has_nul);

/* PoP: w15_webserver_995db6_is_container_cwd @ hermes_cli/web_server_files.py:_is_container_cwd */
int w15_webserver_995db6_is_container_cwd(const char *host);

/* PoP: w15_webserver_995db6_path_is_under @ hermes_cli/web_server_files.py:_path_is_under
 * Lexical pathlib containment: target == root or root in target.parents. */
int w15_webserver_995db6_path_is_under(const char *root, const char *target);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W15_WEBSERVER_995DB6_H */
