#ifndef GATEWAY_PLATFORMS_HELPERS_H
#define GATEWAY_PLATFORMS_HELPERS_H
#include <stdbool.h>
char **gateway_platforms_helpers_split_markdown_table_row(const char *line, int *out_n);
bool gateway_platforms_helpers_is_table_row(const char *line);
char *gateway_platforms_helpers_render_table_block(char **table_block, int nblock);
char *gateway_platforms_helpers_convert_table_to_bullets(const char *text);

/* Mention-pattern compilation (port of gateway/platforms/helpers.py:compile_mention_patterns). */
typedef struct {
    void  **patterns;  /* hregex_t* elements */
    size_t  count;
    size_t  cap;
} mention_patterns_t;
mention_patterns_t *gateway_platforms_helpers_compile_mention_patterns(
    const char *raw_json, const char *log_prefix,
    const char *platform_label, const char *defaults_json);
void gateway_platforms_helpers_free_mention_patterns(mention_patterns_t *mp);
int gateway_platforms_helpers_match_mentions(const mention_patterns_t *mp, const char *text);
#endif
