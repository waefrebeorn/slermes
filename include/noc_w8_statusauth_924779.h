#ifndef NOC_W8_STATUSAUTH_924779_H
#define NOC_W8_STATUSAUTH_924779_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w8_statusauth_924779_qwen_expiry @ hermes_cli/status_auth.py:_qwen_expiry
 * expires_at_ms is a decimal STRING (int64). Returns the UTC ISO-8601 instant
 * datetime.fromtimestamp(int(ms)/1000, tz=UTC).isoformat(). */
struct json_t *w8_statusauth_924779_qwen_expiry(const char *expires_at_ms);

/* PoP: w8_statusauth_924779_format_iso_timestamp @ hermes_cli/status_auth.py:_format_iso_timestamp
 * Non-str / empty-after-strip -> "(unknown)"; unparseable -> the ORIGINAL value;
 * else "%Y-%m-%d %H:%M:%S %Z" in local time (UTC when built with TZ=UTC). */
struct json_t *w8_statusauth_924779_format_iso_timestamp(const struct json_t *value);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W8_STATUSAUTH_924779_H */
