#ifndef NOC_W18_DESKTOPBUI_E30F9A_H
#define NOC_W18_DESKTOPBUI_E30F9A_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w18_desktopbui_e30f9a_stamp_drift @ hermes_cli/desktop_build_reuse.py:_stamp_drift
 * binary (NULL = no installed node binary), raw_stdout and ok (no exception)
 * are injected seams for installed_package()/subprocess.run(). Returns the
 * parsed last line when it is a JSON list, else JSON null. */
struct json_t *w18_desktopbui_e30f9a_stamp_drift(const char *binary,
                                                 const char *raw_stdout, int ok);

/* PoP: w18_desktopbui_e30f9a_packaged_desktop_reusable @ hermes_cli/desktop_build_reuse.py:packaged_desktop_reusable
 * resources_present, baked/prepared stamps (NULL = unreadable), build_needed
 * and drift are injected seams for the filesystem, _desktop_build_needed and
 * the _stamp_drift call. Returns the commit string or JSON null. */
struct json_t *w18_desktopbui_e30f9a_packaged_desktop_reusable(
    int resources_present, const struct json_t *baked, const struct json_t *prepared,
    int build_needed, const struct json_t *drift);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W18_DESKTOPBUI_E30F9A_H */
