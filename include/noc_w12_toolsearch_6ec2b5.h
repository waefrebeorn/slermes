#ifndef NOC_W12_TOOLSEARCH_6EC2B5_H
#define NOC_W12_TOOLSEARCH_6EC2B5_H

#include "hermes_json.h"

/* PoP port of tools/tool_search_catalog.py (gate w12_toolsearch_6ec2b5). */

json_t *w12_toolsearch_6ec2b5_short_desc(json_t *description, long long max_chars);
json_t *w12_toolsearch_6ec2b5_listing_group_label(json_t *source_name);
json_t *w12_toolsearch_6ec2b5_fn(json_t *td);
json_t *w12_toolsearch_6ec2b5_required_term_coverage(json_t *n);
json_t *w12_toolsearch_6ec2b5_entry_search_text(json_t *td, json_t *source_label);

#endif /* NOC_W12_TOOLSEARCH_6EC2B5_H */
