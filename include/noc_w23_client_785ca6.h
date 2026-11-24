/*
 * noc_w23_client_785ca6.h - C surface for upstream agent/lsp/client.py
 * (gate w23_client_785ca6).
 *
 * The LSP transport (server process, stdio pipes, event loop) is a process
 * seam: this surface owns only the pure decisions the client makes about
 * paths, URIs, document versions and diagnostic identity.  os.name is passed
 * in so both the Windows and the POSIX branch of file_uri / uri_to_path are
 * reachable; os.path itself stays the host implementation (ntpath here),
 * exactly as it does when the Python module's os.name is swapped.
 */
#ifndef NOC_W23_CLIENT_785CA6_H
#define NOC_W23_CLIENT_785CA6_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Module-level retry / buffer policy mirrored from the Python source. */
#define W23_CLIENT_785CA6_MAX_CONTENT_MODIFIED_RETRIES 3
#define W23_CLIENT_785CA6_STDERR_TAIL_LINES 30
#define W23_CLIENT_785CA6_MAX_TRACKED_FILES 64
#define W23_CLIENT_785CA6_STREAM_LIMIT (16 * 1024 * 1024)

/* {"max_content_modified_retries":3, "stderr_tail_lines":30,
 *  "max_tracked_files":64, "stream_limit":16777216, "initialize_timeout":45.0,
 *  "diagnostics_document_wait":5.0, "diagnostics_full_wait":10.0,
 *  "diagnostics_request_timeout":3.0, "push_debounce":0.15,
 *  "shutdown_grace":1.0, "retry_base_delay":0.5,
 *  "live_states":["running","starting"]} */
json_t *w23_client_785ca6_limits(void);

/* file_uri(path): "file://" + quote(abspath(path), safe="/:"); the nt branch
 * also turns "\" into "/" and roots the path ("C:\x" -> "file:///C:/x").
 * Called with path == NULL the empty path is used (abspath("") == cwd).
 * Caller frees. */
char *w23_client_785ca6_file_uri(const char *path, const char *os_name);

/* uri_to_path(uri): non-file URIs pass through untouched; a file:// URI is
 * percent-decoded (invalid escapes kept literally, invalid bytes replaced
 * with U+FFFD) and normpath'd.  Caller frees. */
char *w23_client_785ca6_uri_to_path(const char *uri, const char *os_name);

/* _folder(root) -> {"name": basename(root.rstrip(os.sep)) or root,
 *                    "uri": file_uri(root)}.  os.sep is the host separator
 * ("\\"), independent of os_name, matching the Python. */
json_t *w23_client_785ca6_folder(const char *root, const char *os_name);

/* _end_position(text) -> {"line": N, "character": UTF-16 units}.  Text ending
 * in a line break ends at column 0 of the next line. */
json_t *w23_client_785ca6_end_position(const char *text);

/* as_disk_read(text): utf-8-sig decode (BOM dropped, undecodable bytes ->
 * U+FFFD) then CRLF/CR -> LF.  Caller frees. */
char *w23_client_785ca6_as_disk_read(const char *text);

/* _diagnostic_key(diag): NUL-joined severity / code / source / message.strip()
 * / "sl:sc-el:ec".  Python str()/repr() coercions.  The key is returned with
 * its byte length in *len (the separators are literal NUL bytes, so it is not
 * a C string).  Caller frees. */
char *w23_client_785ca6_diagnostic_key(const json_t *diag, size_t *len);

/* _dedupe(list_of_lists): first occurrence per diagnostic key, non-dict
 * entries skipped.  Owned array (never NULL). */
json_t *w23_client_785ca6_dedupe(const json_t *lists);

/* _DocState.fresh_push / fresh_pull / fresh.  doc keys: version (default 0),
 * push_version / pull_version (default -1); kind is "push" | "pull" | "any";
 * `at` is the optional version argument (null/absent -> the doc's version). */
int w23_client_785ca6_doc_fresh(const json_t *doc, const char *kind, const json_t *at);

/* diagnostics_for: req keys path (str), fresh_only (bool, default false),
 * docs (array of _DocState-shaped objects each carrying "path"; default
 * version 0, push/pull [], push_version/pull_version -1).  Owned array. */
json_t *w23_client_785ca6_diagnostics_for(const json_t *req);

/* document_text: req keys path, docs (same shape).  Returns the synced text
 * as a JSON string, or JSON null when the path was never opened. */
json_t *w23_client_785ca6_document_text(const json_t *req);

/* _config_section(item, init_options): dotted section walk; a falsy item,
 * section or init_options returns init_options or None.  Owned node. */
json_t *w23_client_785ca6_config_section(const json_t *item, const json_t *init_options);

/* workspace/configuration: [config_section(item) for item in params.items];
 * a non-dict params answers [None].  Owned array. */
json_t *w23_client_785ca6_workspace_configuration(const json_t *params,
                                                  const json_t *init_options);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W23_CLIENT_785CA6_H */
