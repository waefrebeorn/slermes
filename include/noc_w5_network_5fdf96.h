#ifndef NOC_W5_NETWORK_5FDF96_H
#define NOC_W5_NETWORK_5FDF96_H
#include <stdbool.h>
#include <stddef.h>

/* C home for upstream Hermes pm/network.py (gate w5_network_5fdf96).
 * Exception classification, CDN-firewall explanation, and Retry-After backoff
 * are ported; the retry loop / wait / real I/O stay in Python. An exception is
 * described by a small tagged struct instead of a live Python exception. */

typedef struct w5_network_5fdf96_exc w5_network_5fdf96_exc_t;
struct w5_network_5fdf96_exc {
    /* http | url | ssl_eof | ssl_zero_return | ssl | gaierror | timeout
     * | connection | incomplete_read | os | other */
    const char *kind;
    int code;                       /* http status code */
    const char *errno_name;         /* os kind: symbolic errno name */
    bool eai_again;                 /* gaierror kind: errno == EAI_AGAIN */
    w5_network_5fdf96_exc_t *reason;/* url kind: wrapped reason */
};

/* PoP: w5_network_5fdf96_is_transient @ pm/network.py:is_transient */
bool w5_network_5fdf96_is_transient(const w5_network_5fdf96_exc_t *exc);

/* PoP: w5_network_5fdf96_edge_block @ pm/network.py:edge_block
 * Returns a malloc'd message or NULL (Python None). */
char *w5_network_5fdf96_edge_block(int code,
                                   const char *const *header_keys,
                                   const char *const *header_vals,
                                   size_t n_headers,
                                   const char *filename);

/* PoP: w5_network_5fdf96_delay @ pm/network.py:_delay
 * retry_after NULL/empty means no Retry-After header. */
double w5_network_5fdf96_delay(int attempt, const char *retry_after);

/* PoP: w5_network_5fdf96_free @ pm/network.py:<module> */
void w5_network_5fdf96_free(char *s);

#endif /* NOC_W5_NETWORK_5FDF96_H */
