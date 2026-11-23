#ifndef NOC_CLARBATCH_H
#define NOC_CLARBATCH_H
#include "hermes_json.h"
/* ended == NULL represents Python ended=None. frame_result == NULL is None. */
json_t *clarbatch_settle_batch(json_t *locked, json_t *frame_result, const char *ended);
#endif /* NOC_CLARBATCH_H */
