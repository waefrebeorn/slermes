#ifndef NOC_STOOL_H
#define NOC_STOOL_H
#include "hermes_json.h"
json_t *stool_bounded_args(json_t *args);          /* returns json_copy or a new object */
json_t *stool_bounded_result(json_t *result, int is_error); /* returns a json string */
#endif /* NOC_STOOL_H */
