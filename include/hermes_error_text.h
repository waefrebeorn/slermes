#ifndef HERMES_ERROR_TEXT_H
#define HERMES_ERROR_TEXT_H

#include <stddef.h>

/* Flatten provider error objects into a single lowercased string.
 * Returns a malloc'd buffer; caller must free(). */
char *error_text(int status_code, const char *msg, const char *body, const char *response);

#endif /* HERMES_ERROR_TEXT_H */
