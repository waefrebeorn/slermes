#ifndef NOC_CLICOMPLETION_H
#define NOC_CLICOMPLETION_H
struct json_t;
/* Deterministic parser shape built by build_completion_parser. Caller owns node. */
struct json_t *clicompletion_build_parser_shape(void);
#endif
