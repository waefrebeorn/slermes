#ifndef NOC_PROCWLS_H
#define NOC_PROCWLS_H
/* C port of tools/process_registry_wsl.py (gate: procwsl). */
/* _is_wsl_launcher_command(command): NULL (non-str) -> 0. */
int procwsl_is_wsl_launcher_command(const char *command);

#endif /* NOC_PROCWLS_H */
