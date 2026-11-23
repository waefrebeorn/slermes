#ifndef NOC_W7_PLATFORMENV_955637_H
#define NOC_W7_PLATFORMENV_955637_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w7_platformenv_955637_csv_list @ gateway/platform_env.py:csv_list
 * [part.strip() for part in value.split(",") if part.strip()] -> JSON array. */
struct json_t *w7_platformenv_955637_csv_list(const char *value);

/* PoP: w7_platformenv_955637_strip_slash @ gateway/platform_env.py:strip_slash
 * value.rstrip("/") -> JSON string. */
struct json_t *w7_platformenv_955637_strip_slash(const char *value);

/* PoP: w7_platformenv_955637_strip_lower @ gateway/platform_env.py:strip_lower
 * value.strip().lower() -> JSON string. */
struct json_t *w7_platformenv_955637_strip_lower(const char *value);

/* PoP: w7_platformenv_955637_truthy_token @ gateway/platform_env.py:truthy_token
 * value.lower() in {"true", "1", "yes", "on"} -> JSON bool. */
struct json_t *w7_platformenv_955637_truthy_token(const char *value);

/* PoP: w7_platformenv_955637_int_or @ gateway/platform_env.py:int_or
 * int_or(default)(raw) == int(str(raw).strip(), 10) with `default` on failure.
 * default is a decimal string (int64); result is a JSON number. */
struct json_t *w7_platformenv_955637_int_or(const char *value, const char *default_str);

/* PoP: w7_platformenv_955637_mention_patterns @ gateway/platform_env.py:mention_patterns
 * json.loads(value), else csv_list(value.replace("\n", ",")) -> any JSON value. */
struct json_t *w7_platformenv_955637_mention_patterns(const char *value);

/* PoP: w7_platformenv_955637_env_first @ gateway/platform_env.py:env_first
 * First truthy getenv(name) among names (a string or tuple). `values` is an
 * explicit JSON object standing in for os.environ (avoids CRT setenv quirks):
 * getenv(name) == values[name] when it is a string, else "". */
struct json_t *w7_platformenv_955637_env_first(const struct json_t *names,
                                                const struct json_t *values);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W7_PLATFORMENV_955637_H */
