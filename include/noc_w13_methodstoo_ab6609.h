#ifndef NOC_W13_METHODSTOO_AB6609_H
#define NOC_W13_METHODSTOO_AB6609_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w13_methodstoo_ab6609_str_arg @ tui_gateway/methods_tools.py:_str_arg
 * Python: str(params.get(key) or "").strip() */
struct json_t *w13_methodstoo_ab6609_str_arg(const struct json_t *params, const char *key);

/* PoP: w13_methodstoo_ab6609_joined_output @ tui_gateway/methods_tools.py:_joined_output
 * Python: "\n".join(p for p in (stdout, stderr) if p).strip() */
struct json_t *w13_methodstoo_ab6609_joined_output(const struct json_t *stdout_v,
                                                   const struct json_t *stderr_v);

/* PoP: w13_methodstoo_ab6609_is_snapshot_restore @ tui_gateway/methods_tools.py:_is_snapshot_restore */
int w13_methodstoo_ab6609_is_snapshot_restore(const struct json_t *arg);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W13_METHODSTOO_AB6609_H */
