#ifndef NOC_W16_CONTEXTDEF_BAF639_H
#define NOC_W16_CONTEXTDEF_BAF639_H

#include "hermes_json.h"

/* Faithful C11 port of upstream agent/context_default_record.py.
 * The profile home and the contextvar flag/outer state are injected explicitly. */
json_t *w16_contextdef_baf639_key(const char *profile_home, const char *model, const char *base_url);
json_t *w16_contextdef_baf639_was_context_defaulted(const json_t *records, const char *profile_home,
                                                    const char *context_length, const char *model,
                                                    const char *base_url);
json_t *w16_contextdef_baf639_record_resolution(const json_t *records, const char *profile_home,
                                                long long max_records, int outer_present, int flag,
                                                const char *context_length, const char *model,
                                                const char *base_url);

#endif /* NOC_W16_CONTEXTDEF_BAF639_H */
