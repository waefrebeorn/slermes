#ifndef NOC_W14_SOURCERELE_3EDD99_H
#define NOC_W14_SOURCERELE_3EDD99_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w14_sourcerele_3edd99_valid_tag @ hermes_cli/source_releases.py:_valid_tag */
struct json_t *w14_sourcerele_3edd99_valid_tag(const struct json_t *in);

/* PoP: w14_sourcerele_3edd99_published @ hermes_cli/source_releases.py:_published */
struct json_t *w14_sourcerele_3edd99_published(const struct json_t *in);

/* PoP: w14_sourcerele_3edd99_origin @ hermes_cli/source_releases.py:source_repository
 * GitHub-origin regex -> repository path, else OFFICIAL_REPOSITORY. */
struct json_t *w14_sourcerele_3edd99_origin(const struct json_t *in);

/* PoP: w14_sourcerele_3edd99_target_label @ hermes_cli/source_releases.py:SourceTarget.label */
struct json_t *w14_sourcerele_3edd99_target_label(const struct json_t *in);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W14_SOURCERELE_3EDD99_H */
