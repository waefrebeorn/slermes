#ifndef NOC_W12_CLIINFOLI_04080C_H
#define NOC_W12_CLIINFOLI_04080C_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w12_cliinfoli_04080c_box @ hermes_cli/cli_info_lines.py:_box */
struct json_t *w12_cliinfoli_04080c_box(const char *title, int width);

/* PoP: w12_cliinfoli_04080c_toolsets_lines @ hermes_cli/cli_info_lines.py:toolsets_lines
 * The toolset catalog and the translation function are injected from the fixture. */
struct json_t *w12_cliinfoli_04080c_toolsets_lines(const struct json_t *spec);

/* PoP: w12_cliinfoli_04080c_platforms_lines @ hermes_cli/cli_info_lines.py:platforms_lines
 * Gateway config, platform enablement and home channels are injected; _t is identity. */
struct json_t *w12_cliinfoli_04080c_platforms_lines(const struct json_t *spec);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W12_CLIINFOLI_04080C_H */
