#ifndef NOC_W10_SKILLSINDE_BC8D57_H
#define NOC_W10_SKILLSINDE_BC8D57_H

#include <hermes_json.h>

/* C11 ports of agent/skills_index_delta.py.  Every symbol is prefixed
 * w10_skillsinde_bc8d57_.  Each takes one borrowed json_t *args object and
 * returns a NEW json_t * (string, object, or array).  These are the pure
 * index-row / note codecs that carry the mid-conversation skills delta. */
json_t *w10_skillsinde_bc8d57_row_name(json_t *a);
json_t *w10_skillsinde_bc8d57_index_entries(json_t *a);
json_t *w10_skillsinde_bc8d57_render(json_t *a);
json_t *w10_skillsinde_bc8d57_parse(json_t *a);

#endif /* NOC_W10_SKILLSINDE_BC8D57_H */
