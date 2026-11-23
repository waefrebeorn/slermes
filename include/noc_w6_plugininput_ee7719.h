#ifndef NOC_W6_PLUGININPUT_EE7719_H
#define NOC_W6_PLUGININPUT_EE7719_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w6_plugininput_decode @ pm/plugin_inputs.py:decode
 * `value` is the JSON wire form; returns a normalized descriptor of the
 * decoded dataclass ("type" + "dirs"/"sources"/"data"), or JSON null for
 * None, or a JSON string carrying the ValueError text for an unknown kind.
 * Caller frees. */
struct json_t *w6_plugininput_decode(const struct json_t *value);

/* PoP: w6_plugininput_encode_ex @ pm/plugin_inputs.py:encode
 * `spec` is a normalized descriptor (as returned by decode); `cwd` is the
 * injected current working directory that Python's Path.absolute() resolves
 * against (Windows semantics).  Returns the JSON wire form, or JSON null for
 * the None input.  Caller frees. */
struct json_t *w6_plugininput_encode_ex(const struct json_t *spec, const char *cwd);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W6_PLUGININPUT_EE7719_H */
