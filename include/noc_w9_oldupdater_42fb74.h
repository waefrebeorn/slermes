#ifndef NOC_W9_OLDUPDATER_42FB74_H
#define NOC_W9_OLDUPDATER_42FB74_H

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w9_oldupdater_42fb74_is_historical_update_frame @ hermes_cli/_old_updater.py:_is_historical_update_frame
 * Recognise a historical updater entrypoint frame by its code name, module
 * name and (for __main__) source file. co_name / module / co_filename /
 * main_py_path are the frame's co_name, f_globals["__name__"],
 * f_code.co_filename and this checkout's hermes_cli/main.py path. */
int w9_oldupdater_42fb74_is_historical_update_frame(const char *co_name,
                                                    const char *module,
                                                    const char *co_filename,
                                                    const char *main_py_path);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W9_OLDUPDATER_42FB74_H */
