#ifndef NOC_W7_REDACTCONNS_217BF3_H
#define NOC_W7_REDACTCONNS_217BF3_H
struct json_t;
struct json_t *w7_redactconns_217bf3_closes_wrapper(const char *line, int j);
struct json_t *w7_redactconns_217bf3_kv_value_end(const char *line, int i, int semicolon);
struct json_t *w7_redactconns_217bf3_redact_line(const char *line, int code_file);
struct json_t *w7_redactconns_217bf3_redact_strings(const char *text, int code_file);
#endif
