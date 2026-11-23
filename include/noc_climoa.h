#ifndef NOC_CLIMOA_H
#define NOC_CLIMOA_H
struct json_t;
/* Deterministic parser shape built by build_moa_parser. Caller owns node. */
struct json_t *climoa_build_parser_shape(void);
#endif
