#ifndef AGENT_RETRY_UTILS_H
#define AGENT_RETRY_UTILS_H
#include <stdbool.h>
#include <stddef.h>
typedef struct { int status_code; char text[8192]; } retry_utils_err_t;
bool agent_retry_utils_is_zai_coding_overload_error(const char *base_url, const char *model, const retry_utils_err_t *err);
double agent_retry_utils_parse_retry_after_seconds(const char *value, int *ok);
#endif
