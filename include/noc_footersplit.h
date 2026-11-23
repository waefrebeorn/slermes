#ifndef NOC_FOOTERSPLIT_H
#define NOC_FOOTERSPLIT_H
#include "hermes_json.h"

/* C port of hermes_cli/cli_footer_split.py: FooterSplit._divide_heights'
 * overflow path. Given the row budget and the children's minimum heights in
 * top-to-bottom order, returns the allocated rows per child (same order) as a
 * JSON array of integers. Caller owns the returned json_t. */
json_t *footersplit_divide_heights_overflow(long long budget, const json_t *mins);
#endif
