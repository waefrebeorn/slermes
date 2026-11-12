#ifndef PORT_URL_UTILS_H
#define PORT_URL_UTILS_H

/* Pure helpers from hermes_cli/url_utils.py. */

/* format_url_host: return host in HTTP URL-authority form. IPv6 literals get
 * brackets (and '%' is escaped as '%25'); an already-bracketed literal keeps
 * its brackets unless it has no colon inside. Returns malloc'd string, NULL on
 * NULL input (the live Python raises on None; NULL is the C convention). */
char *format_url_host(const char *host);

#endif /* PORT_URL_UTILS_H */
