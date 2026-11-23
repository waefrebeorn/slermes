#ifndef NOC_W12_MAIN_CONTAI_9649D2_H
#define NOC_W12_MAIN_CONTAI_9649D2_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w12_maincontai_9649d2_exec_plan @ hermes_cli/main_container.py:exec_in_container
 *
 * Pure seam over exec_in_container: given the container_info fields, cli_args, an
 * explicit environment (TERM/COLORTERM/LANG/LC_ALL/USER plus the injected results of
 * shutil.which(backend) as "runtime" and shutil.which("sudo") as "sudo_path"), the
 * tty flag and the two inspect-probe return codes, return either
 *   {"kind":"exec","argv":[...]}   -- the exact os.execvp argv, or
 *   {"kind":"error","message":"..."} -- the exact stderr text before sys.exit(1).
 * Caller frees. */
struct json_t *w12_maincontai_9649d2_exec_plan(const char *backend,
                                                const char *container_name,
                                                const char *exec_user,
                                                const char *hermes_bin,
                                                const struct json_t *cli_args,
                                                const struct json_t *env,
                                                int stdin_isatty,
                                                int probe1_rc,
                                                int probe2_rc);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W12_MAIN_CONTAI_9649D2_H */
