/*
 * noc_w27_adapter_04abb5_p4.h - C surface for the Buzz adapter's config
 * decisions and standalone send (gate w27_adapter_04abb5_p4).
 *
 * Ported: check_requirements / validate_config (relay + key gating), the
 * _YAML_BRIDGE seeding of buzz.extra, the env -> extra enablement seeding,
 * and the out-of-process standalone sender's target/reply/media argv, its
 * presentation-mention retry and its receipt handling.
 * The env store, the credentials file, the CLI binary probe and the buzz
 * subprocess are seams.
 */
#ifndef NOC_W27_ADAPTER_04ABB5_P4_H
#define NOC_W27_ADAPTER_04ABB5_P4_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* in: {relay, key} -> bool. */
json_t *w27_adapter_04abb5_check_requirements(const json_t *in);

/* in: {relay_env, extra, key} -> bool. */
json_t *w27_adapter_04abb5_validate_config(const json_t *in);

/* in: {buzz_cfg} -> the seeded extra, or null when nothing matched. */
json_t *w27_adapter_04abb5_apply_yaml_config(const json_t *in);

/* in: {env: {BUZZ_*: value}} -> the seeded extra, or null. */
json_t *w27_adapter_04abb5_env_enablement(const json_t *in);

/* in: {relay, key, cli_path, chat_id, home_channel, thread_id, reply_to_mode,
 *      media_files, message, auth_tag, script: [[rc,out,err]]}
 * -> {calls: [{args, input, auth_tag}], result: {...}}. */
json_t *w27_adapter_04abb5_standalone_send(const json_t *in);

#ifdef __cplusplus
}
#endif
#endif
