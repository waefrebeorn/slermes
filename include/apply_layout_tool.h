/*
 * apply_layout_tool.h — Port of tools/apply_layout_tool.py.
 *
 * Apply a layout preset in the Hermes desktop GUI (desktop_ui toolset).
 * Python emits "layout.apply" through the shared desktop_ui bridge; the C
 * port routes the same event through an injectable emit callback so hosts
 * wire the real bridge and oracles drive a fake one. The renderer resolves
 * the preset id against its layouts registry and answers on the same bridge.
 */
#ifndef APPLY_LAYOUT_TOOL_H
#define APPLY_LAYOUT_TOOL_H

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: desktop_ui.emit @ tools/desktop_ui.py — host bridge.
 * event: "layout.apply"; payload_json: {"preset": name}.
 * Returns 1 = delivered/applied, 0 = bridge answered no (timeout note path).
 * A negative return models Python's raised exception. */
typedef int (*apply_layout_emit_fn)(const char *event, const char *payload_json,
                                    void *ctx);

/* PoP: apply_layout_tool @ tools/apply_layout_tool.py:apply_layout_tool
 * Ask the desktop GUI to apply layout preset `preset`. Returns a malloc'd
 * JSON tool-result string (caller frees): the success object
 * {"success": true, "preset": name} or a tool_error(...) JSON on the
 * empty-preset / exception / no-bridge paths. */
char *apply_layout_tool(const char *preset, apply_layout_emit_fn emit, void *emit_ctx);

/* PoP: APPLY_LAYOUT_SCHEMA @ tools/apply_layout_tool.py
 * Returns a malloc'd JSON serialization of the tool schema dict (caller
 * frees). */
char *apply_layout_schema(void);

/* PoP: _TIMEOUT_NOTE @ tools/apply_layout_tool.py — exposed for hosts. */
#define APPLY_LAYOUT_TIMEOUT_NOTE \
    "Layout apply is only available in the Hermes desktop app."

#ifdef __cplusplus
}
#endif

#endif /* APPLY_LAYOUT_TOOL_H */
