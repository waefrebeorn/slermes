#ifndef NOC_W13_DESKTOPCON_98238A_H
#define NOC_W13_DESKTOPCON_98238A_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w13_desktopcon_98238a_launch_notice @ hermes_cli/desktop_console.py:desktop_launch_notice
 * platform = value of sys.platform, source_mode = the keyword-only flag.
 * Returns {"sink":"log"|"print","message":<message>}: win32 + not source_mode logs.
 */
struct json_t *w13_desktopcon_98238a_launch_notice(const char *platform, int source_mode,
                                                   const char *message);

/* PoP: w13_desktopcon_98238a_output_stream_keys @ hermes_cli/desktop_console.py:desktop_console_output
 * Sorted keys of the mapping the context manager yields (empty off packaged Windows).
 */
struct json_t *w13_desktopcon_98238a_output_stream_keys(const char *platform, int source_mode);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W13_DESKTOPCON_98238A_H */
