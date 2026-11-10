/*
 * port_api_store_error.h — pure HTTP-mapping decision from
 * gateway/platforms/api_server_authority_runs.py store_error_response. The
 * aiohttp response body construction (the _openai_error JSON / str(exc)) is
 * the unported IO half.
 */
#ifndef PORT_API_STORE_ERROR_H
#define PORT_API_STORE_ERROR_H

/* store_error_response's status decision: given the excuse's reason and
 * whether the caller threw a RuntimeStoreError, return a malloc'd JSON
 * {"reason","status","queued","x_should_retry_false","retry_after_1"}.
 *   reason = exc_reason if is_runtime_store_error else "storage_unavailable"
 *   queued ("admission_queued") -> 503, no Retry-After, x-should-retry=false
 *   else status = {"invalid_params":400,"runtime_draining":503,
 *                  "storage_unavailable":503,"chain_moved":503}.get(reason,409)
 *         retry_after_1 when status==503. Caller frees. */
char *asr_error_status(const char *exc_reason, int is_runtime_store_error);

#endif /* PORT_API_STORE_ERROR_H */
