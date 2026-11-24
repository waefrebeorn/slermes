#ifndef NOC_W20_SANDBOXIMA_0498DB_H
#define NOC_W20_SANDBOXIMA_0498DB_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of hermes_cli/sandbox_image_switch.py (gate w20_sandboxima_0498db).
 *
 * Only the decision logic is exposed: the docker/ps probe, the config reads
 * and the terminal-scope read arrive as explicit JSON seams so the semantics
 * are testable without docker, a config file or a live environment. The
 * interactive prompt loop (offer_interactive) stays Python: it owns cprint and
 * input().
 */

/* PoP: w20_sandboxima_0498db_image_pinned @ hermes_cli/sandbox_image_switch.py:_image_pinned
 * The file's own terminal.docker_image key wins; then the bound terminal scope
 * (only the string "1" pins); else a launch env image that differs from the
 * effective value. `raw_terminal` is read_raw_config().get("terminal") (absent
 * or non-dict = no key), `scope` NULL/JSON null = no bound scope, `env_image`
 * NULL/JSON null = TERMINAL_DOCKER_IMAGE unset. */
int w20_sandboxima_0498db_image_pinned(const json_t *terminal_cfg,
                                       const json_t *raw_terminal,
                                       const json_t *scope,
                                       const json_t *env_image);

/* PoP: w20_sandboxima_0498db_pending @ hermes_cli/sandbox_image_switch.py:pending
 * `pending()` with the docker probe and the two config reads scripted:
 * `config` = load_config_readonly(), `listing` = docker ps stdout (NULL/JSON
 * null = the probe failed), `docker_found` = bool(find_docker()). Answers the
 * PendingSwitch as {"current_image","target_image","containers"} or null. */
json_t *w20_sandboxima_0498db_pending(const json_t *config,
                                      const json_t *raw_terminal,
                                      const json_t *scope,
                                      const json_t *env_image,
                                      int docker_found,
                                      const json_t *listing);

/* PoP: w20_sandboxima_0498db_explain_lines @ hermes_cli/sandbox_image_switch.py:explain_lines
 * The notice's six lines, verbatim (the plural is chosen by len(containers)). */
json_t *w20_sandboxima_0498db_explain_lines(const json_t *pending_switch);

/* PoP: w20_sandboxima_0498db_decide_image @ hermes_cli/sandbox_image_switch.py:decide
 * The image decide() pins: target when approving, current when keeping. */
const char *w20_sandboxima_0498db_decide_image(const json_t *pending_switch, int approve);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W20_SANDBOXIMA_0498DB_H */
