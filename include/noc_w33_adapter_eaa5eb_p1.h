/*
 * noc_w33_adapter_eaa5eb_p1.h - C surface for upstream
 * plugins/platforms/irc/adapter.py, part 1 (gate w33_adapter_eaa5eb_p1).
 *
 * The wire-format half of the IRC adapter: how a raw line parses into
 * {prefix, command, params}, how a nickname is lifted out of a prefix, how
 * markdown becomes IRC plain text, how one payload is chunked under the
 * 510-byte line budget (character-aware, never splitting a code point), and
 * how an outgoing line is framed.
 *
 * Every seam (the stream reader/writer, the TLS context, the clock) stays in
 * the caller; this file owns only what the values decide.
 */
#ifndef NOC_W33_ADAPTER_EAA5EB_P1_H
#define NOC_W33_ADAPTER_EAA5EB_P1_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Bytes of IRC payload a PRIVMSG line may carry: 510 minus protocol + CRLF. */
#define W33_ADAPTER_EAA5EB_P1_LINE_BUDGET 510

/* raw: str -> {"prefix", "command", "params": [...]}
 * (upstream _parse_irc_message). */
json_t *w33_adapter_eaa5eb_p1_parse_message(const json_t *req);

/* prefix: "nick!user@host" -> "nick" (upstream _extract_nick). */
json_t *w33_adapter_eaa5eb_p1_extract_nick(const json_t *req);

/* text: markdown -> IRC plain text, through the eight _MARKDOWN_RULES in
 * order (upstream IRCAdapter._strip_markdown). */
json_t *w33_adapter_eaa5eb_p1_strip_markdown(const json_t *req);

/* {paragraph, limit} -> [chunk, ...] UTF-8-safe chunks of <= limit bytes,
 * preferring space boundaries (upstream _chunk_paragraph). */
json_t *w33_adapter_eaa5eb_p1_chunk_paragraph(const json_t *req);

/* {paragraphs: [str], limit} -> flat [chunk, ...] (upstream _split_lines). */
json_t *w33_adapter_eaa5eb_p1_split_lines(const json_t *req);

/* target -> payload bytes left after "PRIVMSG <target> :" and CRLF
 * (upstream _privmsg_budget). */
json_t *w33_adapter_eaa5eb_p1_privmsg_budget(const json_t *req);

/* {content, target, max_message_length} -> the full adapter split: markdown
 * stripped, blank paragraphs dropped, limit = min(max_message_length,
 * _privmsg_budget(target)), and [""] when nothing survives
 * (upstream IRCAdapter._split_message). */
json_t *w33_adapter_eaa5eb_p1_split_message(const json_t *req);

/* line -> "line\r\n", the bytes _send_raw writes (upstream _encode_line). */
json_t *w33_adapter_eaa5eb_p1_encode_line(const json_t *req);

/* text -> CR/LF neutralised to spaces, NUL dropped
 * (upstream _strip_irc_control_chars). */
json_t *w33_adapter_eaa5eb_p1_sanitize_outbound(const json_t *req);

/* params: [str] -> "PONG :<first or empty>" (upstream _handle_line PING). */
json_t *w33_adapter_eaa5eb_p1_ping_reply(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif
