#ifndef NOC_W18_GATEWAYHOS_F57858_H
#define NOC_W18_GATEWAYHOS_F57858_H

#include "hermes_json.h"

/* Faithful C11 port of hermes_cli/gateway_host_record.py
 * (gate w18_gatewayhos_f57858). The late-imported gateway helpers are injected
 * through the `sim` seam so the C code mirrors the live try/except boundary:
 *   "ok_true"    helper returns True / invalidate succeeds
 *   "ok_false"   helper returns False / invalidate succeeds
 *   "import_error" late import fails
 *   "call_error"   helper call raises
 */

json_t *w18_gatewayhos_f57858_discard_dead_host_record(const char *sim);
json_t *w18_gatewayhos_f57858_forget_host_gateway_probe(const char *sim);

#endif /* NOC_W18_GATEWAYHOS_F57858_H */
