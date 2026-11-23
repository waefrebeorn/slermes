#ifndef NOC_W12_SKILLSHUB_A4DFFC_H
#define NOC_W12_SKILLSHUB_A4DFFC_H

#include "hermes_json.h"

/* PoP port of tools/skills_hub_skillssh.py (gate w12_skillshub_a4dffc). */

json_t *w12_skillshub_a4dffc_strip_html(json_t *value);              /* string */
json_t *w12_skillshub_a4dffc_extract_repo_slug(json_t *repo_value);  /* string | null */
json_t *w12_skillshub_a4dffc_normalize_identifier(json_t *identifier); /* string */
json_t *w12_skillshub_a4dffc_wrap_identifier(json_t *identifier);    /* string */
json_t *w12_skillshub_a4dffc_detail_to_metadata(json_t *canonical, json_t *detail); /* object */
json_t *w12_skillshub_a4dffc_token_variants(json_t *value);          /* sorted unique array */

#endif /* NOC_W12_SKILLSHUB_A4DFFC_H */
