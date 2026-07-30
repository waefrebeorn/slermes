#ifndef SLERMES_PORT_FILE_OPERATIONS_H
#define SLERMES_PORT_FILE_OPERATIONS_H

#include <stdbool.h>
#include <stddef.h>

typedef struct json_t json_t;
typedef struct port_file_operations_state port_file_operations_state_t;

/* Lifecycle */
port_file_operations_state_t *port_file_operations_state_init(void);
void port_file_operations_state_cleanup(port_file_operations_state_t *state);

/* Public API */
char *file_ops_strip_terminal_fence_leaks(const char *text);
char *file_ops_detect_line_ending(const char *text);
char *file_ops_normalize_line_endings(const char *text, const char *target);
char *file_ops_strip_bom(const char *text);
bool file_ops_has_bom(const char *text);

#endif /* SLERMES_PORT_FILE_OPERATIONS_H */
