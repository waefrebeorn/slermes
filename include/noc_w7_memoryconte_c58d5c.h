#ifndef NOC_W7_MEMORYCONTE_C58D5C_H
#define NOC_W7_MEMORYCONTE_C58D5C_H
struct json_t;
struct json_t *w7_memoryconte_c58d5c_fence_recalled_context(const char *text);
struct json_t *w7_memoryconte_c58d5c_drop_repeated_recall_lines(const char *text);
struct json_t *w7_memoryconte_c58d5c_scrub_stream(struct json_t *chunks);
struct json_t *w7_memoryconte_c58d5c_build_memory_context_block(const char *raw);
#endif
