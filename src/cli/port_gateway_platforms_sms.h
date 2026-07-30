/*
 * port_gateway_platforms_sms.h — Slermes C11 port of
 * gateway/platforms/sms.py Twilio signature validation.
 *
 * Public surface consumed by the SMS platform webhook handlers. Faithful
 * extraction from the god header so callers no longer include hermes.h
 * transitively.
 */

#ifndef PORT_GATEWAY_PLATFORMS_SMS_H
#define PORT_GATEWAY_PLATFORMS_SMS_H

#ifdef __cplusplus
extern "C" {
#endif

/* Validate X-Twilio-Signature header (HMAC-SHA1, base64). */
int cli_gateway_platforms_sms__validate_twilio_signature(
    const char *url, const char **param_keys, const char **param_values,
    int param_count, const char *signature,
    const char *auth_token);

/* Port variant: check the request signature against configured secrets. */
int cli_gateway_platforms_sms__check_signature(
    const char *url, const char **param_keys, const char **param_values,
    int param_count, const char *signature,
    const char *auth_token);

#ifdef __cplusplus
}
#endif

#endif /* PORT_GATEWAY_PLATFORMS_SMS_H */
