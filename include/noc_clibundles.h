#ifndef NOC_CLIBUNDLES_H
#define NOC_CLIBUNDLES_H
struct json_t;
/* Deterministic parser shape built by build_bundles_parser. Caller owns node. */
struct json_t *clibundles_build_parser_shape(void);
#endif
