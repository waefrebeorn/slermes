#ifndef NOC_W14_WEBAPPLAUN_8E9A00_H
#define NOC_W14_WEBAPPLAUN_8E9A00_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w14_webapplaun_8e9a00_older_gateway @ hermes_cli/webapp_launcher.py:older_gateway */
struct json_t *w14_webapplaun_8e9a00_older_gateway(const struct json_t *in);

/* PoP: w14_webapplaun_8e9a00_refusal @ hermes_cli/webapp_launcher.py:_refusal */
struct json_t *w14_webapplaun_8e9a00_refusal(const struct json_t *in);

/* PoP: w14_webapplaun_8e9a00_plain_http_note @ hermes_cli/webapp_launcher.py:_plain_http_note */
struct json_t *w14_webapplaun_8e9a00_plain_http_note(const struct json_t *in);

/* PoP: w14_webapplaun_8e9a00_signed_out_summary @ hermes_cli/webapp_launcher.py:_signed_out_summary */
struct json_t *w14_webapplaun_8e9a00_signed_out_summary(const struct json_t *in);

/* PoP: w14_webapplaun_8e9a00_restart_applies @ hermes_cli/webapp_launcher.py:_restart_applies */
struct json_t *w14_webapplaun_8e9a00_restart_applies(const struct json_t *in);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W14_WEBAPPLAUN_8E9A00_H */
