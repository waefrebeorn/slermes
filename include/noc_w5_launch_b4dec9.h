#ifndef NOC_W5_LAUNCH_B4DEC9_H
#define NOC_W5_LAUNCH_B4DEC9_H
#include "hermes_json.h"

/* C home for upstream Hermes pm/launch.py (gate w5_launch_b4dec9).
 * launch.py is a thin orchestrator: truststore injection + worker-lease +
 * delegation to pm.cli.main, plus one module-level sys.path insert. Both
 * observable behaviours are ported; the injected side effects are recorded
 * as events by the gate instead of being executed here. */

/* PoP: w5_launch_b4dec9_path_insert @ pm/launch.py:<module>
 * malloc'd str(Path(file).resolve().parents[1]) — Windows path semantics. */
char *w5_launch_b4dec9_path_insert(const char *file_path);

/* PoP: w5_launch_b4dec9_main @ pm/launch.py:main
 * Returns {"events": [...], "rc": <int>}: inject -> lease -> cli, then the
 * cli return code (the Python function returns cli_main() directly). */
json_t *w5_launch_b4dec9_main(int cli_rc);

/* PoP: w5_launch_b4dec9_free @ pm/launch.py:<module> */
void w5_launch_b4dec9_free(char *s);

#endif /* NOC_W5_LAUNCH_B4DEC9_H */
