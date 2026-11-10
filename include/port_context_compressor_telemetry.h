/*
 * port_context_compressor_telemetry.h — pure core of the per-attempt compression
 * telemetry seed (agent/context_compressor_telemetry.py), plus its _safe_int
 * dependency. The self-facts the live method reads (provider, model,
 * context_length, threshold_tokens) and the seed dict are passed as
 * parameters; the returned JSON is the same per-attempt dict, serialized.
 */
#ifndef PORT_CONTEXT_COMPRESSOR_TELEMETRY_H
#define PORT_CONTEXT_COMPRESSOR_TELEMETRY_H

/* Python int(value) best-effort coercion: a JSON number/string/bool/null node
 * (value_json is a serialized single value) -> long long, or 0 + *is_none=1
 * when Python would return None (TypeError/ValueError). Bool maps to 1/0;
 * a float truncates toward zero. */
long long cct_safe_int(const char *value_json, int *is_none);

/* _begin_compression_telemetry: build the per-attempt telemetry dict from the
 * given facts. current_tokens / context_length / threshold_tokens are
 * serialized single JSON values (string or number). provider/model are plain
 * strings. seed_json is the (optional, may be NULL/"null") compression seed
 * dict. attempt_id is the deterministic caller-supplied id (the live method
 * falls back to uuid only when it is absent). Returns malloc'd compact JSON. */
char *cct_begin_compression_telemetry(
    const char *current_tokens,
    const char *attempt_id,
    const char *session_id,
    const char *trigger_source,
    const char *provider,
    const char *model,
    const char *context_length,
    const char *threshold_tokens,
    const char *seed_json);

#endif /* PORT_CONTEXT_COMPRESSOR_TELEMETRY_H */
