#ifndef NOC_W12_WEBAPPSIGN_185F5A_H
#define NOC_W12_WEBAPPSIGN_185F5A_H

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w12_webappsign_185f5a_binding @ hermes_cli/webapp_sign_in.py:binding
 * hex(HMAC-SHA256(key=utf8(token), msg=b"webapp-sign-in")); caller frees. */
char *w12_webappsign_185f5a_binding(const char *token);

/* PoP: w12_webappsign_185f5a_digest @ hermes_cli/webapp_sign_in.py:_digest
 * hex(SHA256(utf8(token))); caller frees. */
char *w12_webappsign_185f5a_digest(const char *token);

/* PoP: w12_webappsign_185f5a_applies @ hermes_cli/webapp_sign_in.py:applies
 * ui_surface == "webapp" and bool(auth_required). */
int w12_webappsign_185f5a_applies(const char *ui_surface, int auth_required);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W12_WEBAPPSIGN_185F5A_H */
