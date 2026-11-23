#ifndef NOC_W8_HOSTTOPOLOG_1E949C_H
#define NOC_W8_HOSTTOPOLOG_1E949C_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w8_hosttopolog_1e949c_normalized @ gateway/host_topology.py:_normalized
 * Canonical profile name (strip + lower, case-insensitive "default"); "" for null/empty.
 * Returns a JSON string. Caller frees. */
struct json_t *w8_hosttopolog_1e949c_normalized(const struct json_t *name);

/* PoP: w8_hosttopolog_1e949c_serves @ gateway/host_topology.py:HostGatewayTopology.serves
 * Args: profiles (array of strings), wanted (string|null). Returns JSON bool. Caller frees. */
struct json_t *w8_hosttopolog_1e949c_serves(const struct json_t *profiles,
                                           const struct json_t *wanted);

/* PoP: w8_hosttopolog_1e949c_describe @ gateway/host_topology.py:HostGatewayTopology.describe
 * Args: pid (decimal string), profiles (array of strings). Returns JSON string. Caller frees. */
struct json_t *w8_hosttopolog_1e949c_describe(const char *pid,
                                              const struct json_t *profiles);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W8_HOSTTOPOLOG_1E949C_H */
