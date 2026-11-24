#ifndef NOC_W19_CUABACKEND_0E9D63_H
#define NOC_W19_CUABACKEND_0E9D63_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w19_cuabackend_0e9d63_resolve_app_path @ tools/computer_use/cua_backend_daemon.py:_resolve_cua_driver_app_path
 * The CuaDriver.app bundle carrying the resolved driver binary, or NULL.  ``realpath``
 * is ``os.path.realpath(driver_cmd)``; ``isfile`` / ``x_ok`` are the answers for the
 * one candidate executable path it derives (``<head>.app/Contents/MacOS/cua-driver``).
 * Returns a new string (caller frees). */
char *w19_cuabackend_0e9d63_resolve_app_path(const char *realpath, int isfile, int x_ok);

/* PoP: w19_cuabackend_0e9d63_validate_signature @ tools/computer_use/cua_backend_daemon.py:_validate_cua_driver_app_signature
 * Fail-closed codesign check: ``{"ok": bool, "error": <str|null>}`` with the live
 * RuntimeError text.  ``have_codesign`` is ``shutil.which("codesign")`` truthiness,
 * ``returncode``/``stderr`` the codesign -dv result, ``allow_unsigned`` the
 * ``computer_use.allow_unsigned_driver`` config flag. */
struct json_t *w19_cuabackend_0e9d63_validate_signature(const char *app_path, int have_codesign,
                                                        int returncode, const char *stderr_text,
                                                        int allow_unsigned);

/* PoP: w19_cuabackend_0e9d63_spawn_command @ tools/computer_use/cua_backend_daemon.py:_embedded_daemon_spawn_command
 * The private-daemon launch vector: ``{"ok": bool, "command": [...], "error": <str|null>}``.
 * Off darwin it is ``[driver_cmd, *serve_args]``; on darwin the app bundle is resolved
 * (``realpath``/``isfile``/``x_ok``), signature-checked and launched through
 * ``/usr/bin/open``.  ``app_path`` may be given directly (skips resolution).
 * ``serve_args`` is a JSON array of strings (may be NULL for none). */
struct json_t *w19_cuabackend_0e9d63_spawn_command(const char *driver_cmd, const struct json_t *serve_args,
                                                   const char *platform, const char *app_path,
                                                   const char *realpath, int isfile, int x_ok,
                                                   int have_codesign, int returncode,
                                                   const char *stderr_text, int allow_unsigned);

/* PoP: w19_cuabackend_0e9d63_daemon_init @ tools/computer_use/cua_backend_daemon.py:_EmbeddedCuaDaemon.__init__
 * The constructor's policy: ``{"ok", "error", "permission_mode", "capability_manifest",
 * "manifest_applies", "socket_path"}``.  ``manifest_abspath`` is what
 * ``os.path.abspath(os.path.expanduser(manifest))`` returns (only consulted for a
 * non-empty manifest), ``manifest_exists`` its ``os.path.isfile`` answer,
 * ``mode_independent`` the facade's ``_manifest_is_mode_independent`` answer,
 * ``token`` the 12 hex chars of the uuid the socket name carries and ``tmpdir`` the
 * ``tempfile.gettempdir()`` the non-Windows socket path is joined to. */
struct json_t *w19_cuabackend_0e9d63_daemon_init(const char *driver_cmd, const char *permission_mode,
                                                 const char *capability_manifest, const char *manifest_abspath,
                                                 int manifest_exists, int mode_independent,
                                                 const char *platform, const char *token,
                                                 const char *tmpdir);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W19_CUABACKEND_0E9D63_H */
