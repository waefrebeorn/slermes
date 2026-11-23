#ifndef NOC_W10_GATEWAYTOO_83997C_H
#define NOC_W10_GATEWAYTOO_83997C_H
struct json_t;
#ifdef __cplusplus
extern "C" {
#endif
/* PoP: w10_gatewaytoo_83997c_proc_from_row @ hermes_cli/gateway_tools.py:_Proc.__init__
 * Mirrors the ProcessSession view attributes main's dock reads from a process.list row. */
struct json_t *w10_gatewaytoo_83997c_proc_from_row(const struct json_t *row);
/* PoP: w10_gatewaytoo_83997c_filter_printable @ hermes_cli/gateway_tools.py:GatewayProcessView.subagent_tail
 * The transcript-tail filter: "".join(c for c in text if c.isprintable() or c in "\n\t"). */
struct json_t *w10_gatewaytoo_83997c_filter_printable(const char *text);
/* PoP: w10_gatewaytoo_83997c_interesting @ hermes_cli/gateway_tools.py:GatewayProcessView._interesting
 * Any row still running or exited within _RETAIN_SECONDS of `now`. */
int w10_gatewaytoo_83997c_interesting(const struct json_t *rows, double now);
#ifdef __cplusplus
}
#endif
#endif /* NOC_W10_GATEWAYTOO_83997C_H */
