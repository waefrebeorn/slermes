/*
 * port_hermes_cli_sizefmt.c — C11 port of hermes_cli/sizefmt.py
 *
 * Small shared byte-formatting helper for CLI/agent output. Sibling of the
 * timefmt port.
 */
#ifndef PORT_HERMES_CLI_SIZEFMT_H
#define PORT_HERMES_CLI_SIZEFMT_H

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: format_bytes @ hermes_cli/sizefmt.py:format_bytes
 * 1234567 -> "1.2 MB" (B/KB/MB/GB/TB; integer bytes, one decimal above).
 * Returns malloc'd string: "?" for None/unparseable input. Caller frees. */
char *szf_format_bytes(const char *input);

#ifdef __cplusplus
}
#endif

#endif /* PORT_HERMES_CLI_SIZEFMT_H */
