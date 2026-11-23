#ifndef NOC_W12_CLIINITMI_95D44D_H
#define NOC_W12_CLIINITMI_95D44D_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w12_cliinitmi_95d44d_explicit_launch_skills @ hermes_cli/cli_init_mixin.py:_explicit_launch_skills
 * resolve_auto_load_skills() is injected as the auto_skills list. */
struct json_t *w12_cliinitmi_95d44d_explicit_launch_skills(const struct json_t *cli,
                                                           const struct json_t *auto_skills);

/* PoP: w12_cliinitmi_95d44d_gateway_launch @ hermes_cli/cli_init_mixin.py:_gateway_launch
 * CLI attributes and the auto-load skill list are injected from the fixture. */
struct json_t *w12_cliinitmi_95d44d_gateway_launch(const struct json_t *cli,
                                                   const struct json_t *auto_skills);

/* PoP: w12_cliinitmi_95d44d_adopt_gateway_toolsets @ hermes_cli/cli_init_mixin.py:_adopt_gateway_toolsets
 * Returns the mutated enabled_toolsets plus the lines the Python version prints. */
struct json_t *w12_cliinitmi_95d44d_adopt_gateway_toolsets(const struct json_t *cli,
                                                           const struct json_t *store);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W12_CLIINITMI_95D44D_H */
