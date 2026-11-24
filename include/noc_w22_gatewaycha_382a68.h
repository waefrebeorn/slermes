#ifndef NOC_W22_GATEWAYCHA_382A68_H
#define NOC_W22_GATEWAYCHA_382A68_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of hermes_cli/gateway_chat_startup.py (gate w22_gatewaycha_382a68).
 *
 * The two import-heavy entry points are decisions once their seams are named:
 *
 *   ensure_launch_provider
 *     bypass_launch(args)          -> config/profile seam (bypass)
 *     HERMES_TUI_GATEWAY_URL       -> environment seam (injected string)
 *     run_bootstrap(announce=False)-> side effect (bootstrap step)
 *     _has_any_provider_configured -> config seam (provider_configured)
 *     _first_run_setup_guard(args) -> side effect (guard step)
 *
 *   launch_gateway_chat
 *     the `from hermes_cli.main/gateway_chat import ...` block -> import seam
 *     _confirm_startup_expensive_model_override(args)          -> side effect
 *     launch_from_args(args)                                   -> gateway seam
 *     emit_partial_update_hint(exc)                            -> hint seam
 *
 * Every decision is real here; only those seams are inputs.
 */

/* PoP: w22_gatewaycha_382a68_ensure_launch_provider @ hermes_cli/gateway_chat_startup.py:ensure_launch_provider
 * A JSON object {"ready", "bootstrap_called", "provider_checked",
 * "guard_called"}. `bypass` true short-circuits before any probe (so the
 * bootstrap/guard flags stay false). gateway_url NULL means the variable is
 * unset; whitespace-only reads empty after .strip(). `args` carries "resume",
 * "base_url" and "api_key" (truthy tests, absent key = None). */
json_t *w22_gatewaycha_382a68_ensure_launch_provider(int bypass, const json_t *args,
                                                     const char *gateway_url,
                                                     int provider_configured);

/* PoP: w22_gatewaycha_382a68_launch_gateway_chat @ hermes_cli/gateway_chat_startup.py:launch_gateway_chat
 * `import_ok` false is the ImportError from the import block; `hint_handled` is
 * `emit_partial_update_hint(exc)`; `code` is what launch_from_args returned.
 * Returns {"outcome":"returned","code":N,"confirmed":bool,"hint_called":bool}
 * or {"outcome":"raised","confirmed":bool,"hint_called":bool} (the except
 * re-raises, whatever ImportError subclass the missing module produced).
 * Consent runs before the launch unless the launch was a bypass launch. */
json_t *w22_gatewaycha_382a68_launch_gateway_chat(int bypass, int import_ok,
                                                  int hint_handled, long code);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W22_GATEWAYCHA_382A68_H */
