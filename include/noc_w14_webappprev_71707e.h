#ifndef NOC_W14_WEBAPPPREV_71707E_H
#define NOC_W14_WEBAPPPREV_71707E_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w14_webappprev_71707e_preview_policy @ hermes_cli/webapp_previews.py:PREVIEW_POLICY */
struct json_t *w14_webappprev_71707e_preview_policy(void);
/* PoP: w14_webappprev_71707e_guest_policy @ hermes_cli/webapp_previews.py:GUEST_POLICY */
struct json_t *w14_webappprev_71707e_guest_policy(void);
/* PoP: w14_webappprev_71707e_preview_html @ hermes_cli/webapp_previews.py:PREVIEW_HTML */
struct json_t *w14_webappprev_71707e_preview_html(void);
/* PoP: w14_webappprev_71707e_preview_loader @ hermes_cli/webapp_previews.py:PREVIEW_LOADER */
struct json_t *w14_webappprev_71707e_preview_loader(void);
/* PoP: w14_webappprev_71707e_route_manifest @ hermes_cli/webapp_previews.py:mount_preview_assets */
struct json_t *w14_webappprev_71707e_route_manifest(void);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W14_WEBAPPPREV_71707E_H */
