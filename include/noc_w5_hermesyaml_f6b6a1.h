#ifndef NOC_W5_HERMESYAML_F6B6A1_H
#define NOC_W5_HERMESYAML_F6B6A1_H

#include "hermes_json.h"

/* C port of the policy scalars + scalar emitter of hermes_yaml.py.
 * All exported symbols are hermesyaml_-prefixed.
 * Returned char* values are malloc'd and caller-owned (free()). */

/* _Yaml11Resolver.processing_version -> [1, 1]. */
json_t *hermesyaml_processing_version(void);

/* ROUNDTRIP_YAML_WIDTH (2**31 - 1): the anti-fold emitter width. */
long long hermesyaml_roundtrip_width(void);

/* safe_dump of ONE JSON scalar (null / bool / number / string), using the
 * module's YAML 1.1 safe dumper (single-line, no line folding). */
char *hermesyaml_safe_dump_scalar(json_t *v);

#endif /* NOC_W5_HERMESYAML_F6B6A1_H */
