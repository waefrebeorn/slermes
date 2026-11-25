/*
 * noc_w34_runtimeboo_7f06de.h - C surface for upstream
 * gateway/runtime_bootstrap_windows.py (gate w34_runtimeboo_7f06de).
 *
 * The pure decision core of the native local-only bootstrap pipes: whether
 * this client may talk to the control pipe's server (judge_pipe_server over
 * Win32 facts probed by the caller), the refusal codes and their remedies,
 * the peer-subject fallback chain, and the overlapped-I/O budget arithmetic
 * (deadline -> milliseconds, accept/open/read policy tables, the private pipe
 * DACL and open mode).
 *
 * Every seam stays in the caller: the ctypes/Win32 calls (OpenProcess,
 * GetSecurityInfo, ImpersonateNamedPipeClient, CreateFile, ConnectNamedPipe),
 * the pipe handles and their OVERLAPPED objects, time.monotonic(), the worker
 * threads and the handler. What is ported here is only what those values
 * decide, so each function is a value -> value mapping with no state.
 */
#ifndef NOC_W34_RUNTIMEBOO_7F06DE_H
#define NOC_W34_RUNTIMEBOO_7F06DE_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Win32 flags/limits carried by the bootstrap pipes. */
#define W34_RUNTIMEBOO_7F06DE_FILE_FLAG_OVERLAPPED 0x40000000u
#define W34_RUNTIMEBOO_7F06DE_FIRST_PIPE_INSTANCE 0x00080000u
#define W34_RUNTIMEBOO_7F06DE_PIPE_ACCESS_DUPLEX 3u
#define W34_RUNTIMEBOO_7F06DE_ERROR_OPERATION_ABORTED 995
#define W34_RUNTIMEBOO_7F06DE_ERROR_PIPE_BUSY 231
#define W34_RUNTIMEBOO_7F06DE_ERROR_SEM_TIMEOUT 121
#define W34_RUNTIMEBOO_7F06DE_READ_CHUNK 4096

/* request: {own_session, owner_session, own_elevated, owner_elevated} ->
 * "gateway_elevated" / "gateway_other_session" / null.  (classify_refused_owner) */
json_t *w34_runtimeboo_7f06de_classify_refused_owner(const json_t *req);

/* request: {reason, pid, owner_session} -> the remedy sentence.
 * (refused_owner_remedy) */
json_t *w34_runtimeboo_7f06de_refused_owner_remedy(const json_t *req);

/* request: {own_sid, process_sid, pipe:{owner,dacl}, pid, own_elevated,
 *           own_session, server_session, server_elevated} ->
 * {ok, subject, error, reason, pid}.  (judge_pipe_server) */
json_t *w34_runtimeboo_7f06de_judge_pipe_server(const json_t *req);

/* request: {client_sid, client_pid, pid_sid, own_sid} ->
 * {ok, subject, error}.  (the serving half of _peer_subject) */
json_t *w34_runtimeboo_7f06de_peer_subject(const json_t *req);

/* request: {remaining} seconds -> {ok, ms} or {ok:false, error}.  (_remaining_ms) */
json_t *w34_runtimeboo_7f06de_remaining_ms(const json_t *req);

/* request: {error} (GetOverlappedResult code) ->
 * {connected, error, code}.  (the decision tail of _accept) */
json_t *w34_runtimeboo_7f06de_accept_outcome(const json_t *req);

/* request: {code, phase:"open"|"wait"} -> {action, error}.
 * (query_runtime_control's CreateFile / WaitNamedPipe error table) */
json_t *w34_runtimeboo_7f06de_pipe_open_action(const json_t *req);

/* request: {maximum, before, count} -> {chunk, error}: the read result
 * (count bytes appended to before) and, when accepted, the next ReadFile size.
 * (the body of _read_line) */
json_t *w34_runtimeboo_7f06de_read_step(const json_t *req);

/* request: {sid} -> "D:P(A;;GA;;;<sid>)".  (_create_pipe's private DACL) */
json_t *w34_runtimeboo_7f06de_pipe_dacl(const json_t *req);

/* request: {first} -> 3 | FILE_FLAG_OVERLAPPED | (FIRST_PIPE_INSTANCE if first).
 * (_create_pipe's open mode) */
json_t *w34_runtimeboo_7f06de_pipe_create_mode(const json_t *req);

/* The module's pipe constants, as an object.  (_PIPE_CLIENT_SQOS,
 * _PIPE_INSTANCES and the client open flags derived from them) */
json_t *w34_runtimeboo_7f06de_constants(void);

#ifdef __cplusplus
}
#endif
#endif
