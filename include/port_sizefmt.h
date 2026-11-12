#ifndef PORT_SIZEFMT_H
#define PORT_SIZEFMT_H

/* Pure helpers from hermes_cli/sizefmt.py. */

/* format_bytes: human size (B/KB/MB/GB/TB), one decimal above 1024, integer
 * bytes below. Returns "?" for NULL or unparseable input (the live Python
 * raises on None/non-numeric). Returns malloc'd string. */
char *format_bytes(const char *n);

#endif /* PORT_SIZEFMT_H */
