#ifndef GATEWAY_PLATFORMS_HELPERS_H
#define GATEWAY_PLATFORMS_HELPERS_H
#include <stdbool.h>
char **gateway_platforms_helpers_split_markdown_table_row(const char *line, int *out_n);
bool gateway_platforms_helpers_is_table_row(const char *line);
char *gateway_platforms_helpers_render_table_block(char **table_block, int nblock);
char *gateway_platforms_helpers_convert_table_to_bullets(const char *text);
#endif
