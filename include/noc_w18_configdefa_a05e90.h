#ifndef NOC_W18_CONFIGDEFA_A05E90_H
#define NOC_W18_CONFIGDEFA_A05E90_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w18_configdefa_a05e90_env @ hermes_cli/config_defaults_env.py:_env
 * One OPTIONAL_ENV_VARS entry: {"description","prompt", **keys}.  `keys` is an
 * object whose members are copied in order.  Caller frees. */
struct json_t *w18_configdefa_a05e90_env(const char *description, const char *prompt,
                                         const struct json_t *keys);

/* PoP: w18_configdefa_a05e90_category @ hermes_cli/config_defaults_env.py:_category
 * The `make` closure returned by _category, with its free variables supplied.
 * `password_default_code`: -1 == None, 0 == False, 1 == True.
 * `opts` (an object) supplies optional kwargs url/help/tools/password/advanced:
 *   url/help/tools ABSENT == _OMIT (key omitted); PRESENT (even null) is copied.
 *   password/advanced ABSENT == use the closure default; PRESENT overrides.
 * Key order: description, prompt, [help], [url], [tools], [password], category, [advanced].
 * Caller frees. */
struct json_t *w18_configdefa_a05e90_category(const char *category,
    int password_default_code, int advanced_default,
    const char *description, const char *prompt, const struct json_t *opts);

/* PoP: w18_configdefa_a05e90_base_url @ hermes_cli/config_defaults_env.py:_base_url
 * Provider *_BASE_URL override entry (advanced, not a secret).  Caller frees. */
struct json_t *w18_configdefa_a05e90_base_url(const char *name, const char *prompt_name);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W18_CONFIGDEFA_A05E90_H */
