#ifndef NOC_CRONTRI_H
#define NOC_CRONTRI_H
/* 0 = null, 1 = true, 2 = false, -1 = invalid */
enum { CRONTRI_NULL = 0, CRONTRI_TRUE = 1, CRONTRI_FALSE = 2, CRONTRI_INVALID = -1 };
struct json_t;
int crontri_normalize_catch_up(const struct json_t *value);
int crontri_normalize_wrap_response(const struct json_t *value);
#endif
