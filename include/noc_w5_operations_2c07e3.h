#ifndef NOC_W5_OPERATIONS_2C07E3_H
#define NOC_W5_OPERATIONS_2C07E3_H

/* Pure name-validation / root-resolution primitive from pm/operations.py.
 * The environment-root branch that consults get_hermes_home() is only reached
 * when root is None; the gate always supplies a root (IO-free path). */

/* PoP: w5_operations_2c07e3_environment_root @ pm/operations.py:_environment_root
 * Validates name against [a-zA-Z0-9][a-zA-Z0-9_.-]* (fullmatch). On success
 * returns 1 and sets *out_path to a malloc'd Path(root).absolute() spelling
 * (separators canonicalised to '/'); on failure returns 0 and leaves
 * *out_path unset. CAVEAT: fixtures use clean absolute roots. */
int w5_operations_2c07e3_environment_root(const char *name, const char *root,
                                          char **out_path);

#endif /* NOC_W5_OPERATIONS_2C07E3_H */
