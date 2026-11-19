/*
 * port_run_adapters_flow.c — full-suite resolution wrapper for the
 * gateway/run_adapters.py adapter-lifecycle FLOW cores. The port lives in
 * src/gateway/run_adapters_flow.c (the oracle harness #includes it directly);
 * this wrapper lets the untouched run_all_oracles.sh default deps
 * (src/cli/port_<name>.o + lib/libjson/json.o) build the object via the generic
 * src/%.o pattern rule. Self-contained: libjson only, no I/O.
 */
#include "../gateway/run_adapters_flow.c"
