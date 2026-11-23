#ifndef NOC_CLICHECKPOINTS_H
#define NOC_CLICHECKPOINTS_H
struct json_t;
/* Deterministic parser shape built by build_checkpoints_parser. Caller owns node. */
struct json_t *clicheckpoints_build_parser_shape(void);
#endif
