#ifndef NOC_W14_KANBANTOOL_FF0170_H
#define NOC_W14_KANBANTOOL_FF0170_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w14_kanbantool_ff0170_prop @ tools/kanban_tools_schemas.py:_prop */
struct json_t *w14_kanbantool_ff0170_prop(const struct json_t *type_,
                                          const struct json_t *description);

/* PoP: w14_kanbantool_ff0170_board_schema_prop @ tools/kanban_tools_schemas.py:_board_schema_prop */
struct json_t *w14_kanbantool_ff0170_board_schema_prop(void);

/* PoP: w14_kanbantool_ff0170_schema @ tools/kanban_tools_schemas.py:_schema */
struct json_t *w14_kanbantool_ff0170_schema(const struct json_t *name,
                                            const struct json_t *description,
                                            const struct json_t *properties,
                                            const struct json_t *required);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W14_KANBANTOOL_FF0170_H */
