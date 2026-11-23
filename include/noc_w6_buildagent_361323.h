#ifndef NOC_W6_BUILDAGENT_361323_H
#define NOC_W6_BUILDAGENT_361323_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* C port of docker/build_agent.py (gate w6_buildagent_361323).
 * External calls (current_target(), sysconfig.get_path(), the shared
 * assembler) are explicit seams: `target`, `site` and `commands` come from
 * the caller/fixture instead of being read from the live process. Paths are
 * produced with '/' separators. */

/* PoP: w6_buildagent_361323_assemble_inputs @ docker/build_agent.py:assemble_image
 * The AgentInputs(...) value built by assemble_image(root), as a JSON object. */
struct json_t *w6_buildagent_361323_assemble_inputs(const char *root, const char *target, const char *site);

/* PoP: w6_buildagent_361323_link_target @ docker/build_agent.py:assemble_image
 * The symlink target written for each manifest command: "../../" + command. */
char *w6_buildagent_361323_link_target(const char *command);

/* PoP: w6_buildagent_361323_assemble_image @ docker/build_agent.py:assemble_image
 * Full post-assemble effect: {"inputs":..., "links":[{name,path,target},...]}. */
struct json_t *w6_buildagent_361323_assemble_image_ex(const char *root, const char *target, const char *site,
                                     const struct json_t *commands);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W6_BUILDAGENT_361323_H */
