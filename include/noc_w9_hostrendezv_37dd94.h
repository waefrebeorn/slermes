#ifndef NOC_W9_HOSTRENDEZV_37DD94_H
#define NOC_W9_HOSTRENDEZV_37DD94_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w9_hostrendezv_37dd94_describe @ gateway/host_rendezvous.py:HostRecord.from_json+describe */
struct json_t *w9_hostrendezv_37dd94_describe(const struct json_t *in);

/* PoP: w9_hostrendezv_37dd94_dial @ gateway/host_rendezvous.py:HostRecord.from_json+dial_host */
struct json_t *w9_hostrendezv_37dd94_dial(const struct json_t *in);

/* PoP: w9_hostrendezv_37dd94_validated_role @ gateway/host_rendezvous.py:_validated_role
 * Returns the role string, or JSON null when ValueError would be raised. */
struct json_t *w9_hostrendezv_37dd94_validated_role(const struct json_t *in);

/* PoP: w9_hostrendezv_37dd94_from_json_ok @ gateway/host_rendezvous.py:HostRecord.from_json */
struct json_t *w9_hostrendezv_37dd94_from_json_ok(const struct json_t *in);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W9_HOSTRENDEZV_37DD94_H */
