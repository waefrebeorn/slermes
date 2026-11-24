#ifndef NOC_W20_SETUPFILES_7EB3BD_H
#define NOC_W20_SETUPFILES_7EB3BD_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of plugins/platforms/google_chat/setup_files.py (gate w20_setupfiles_7eb3bd).
 *
 * `handle_setup_files_command` is ported as its interaction trace: the reply
 * bodies it sends (each carrying the catalog key and kwargs instead of the
 * rendered language string), the credential slot it leaves behind, and the
 * exception it can leak. The adapter's socket, the OAuth helpers and the
 * translator stay upstream -- the fixture states what each answers.
 *
 * Request:
 *   raw_text, sender_email?, chat_id, thread_id?, token_path,
 *   client_secret_exists, token_exists, creds_loaded,
 *   helper: {"mode":"ok"|"exit"|"error", "output":..., "error":...},
 *   reload_creds, build_api_ok
 * Answer:
 *   {"status":"ok","value":{"consumed":true,"replies":[body,...],
 *                           "creds":{"shared":b,"api":b,"by_email":[...]}}} |
 *   {"status":"raised","type":"IndexError","message":...}
 * A `body` is exactly what `_reply` hands the adapter: {"text": <marker>} plus
 * {"thread":{"name":...}} when a thread id was supplied.
 */
json_t *w20_setupfiles_7eb3bd_run(const json_t *request);

/* PoP: w20_setupfiles_7eb3bd_split_arg @ plugins/platforms/google_chat/setup_files.py:handle_setup_files_command
 * `raw_text.split(maxsplit=1)` + `parts[1].strip()`: the trailing subcommand or
 * argument, "" when the command carries none. Caller frees. */
char *w20_setupfiles_7eb3bd_split_arg(const char *raw_text);

/* PoP: w20_setupfiles_7eb3bd_sender_key @ plugins/platforms/google_chat/setup_files.py:handle_setup_files_command
 * `sender_email.strip().lower() if sender_email else None`; NULL in, NULL out. */
char *w20_setupfiles_7eb3bd_sender_key(const char *sender_email);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W20_SETUPFILES_7EB3BD_H */
