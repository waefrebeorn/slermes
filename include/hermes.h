/**
 * @defgroup hermes Main API
 * @brief Umbrella header for WuBu Slermes C implementation.
 *
 * This is a PURE RE-INCLUDE UMBRELLA — it contains NO protocol declarations
 * of its own. Every public symbol now lives in its focused subsystem header
 * (hermes_agent.h, hermes_cli.h, approval.h, hermes_cdp.h, hermes_vault.h,
 * hermes_audit.h, hermes_rate_limit.h, hermes_url_safety.h, hermes_redact.h,
 * hermes_sanitize.h, hermes_result_storage.h, hermes_config_setup.h,
 * hermes_delegate.h, hermes_provider_xai.h, hermes_gateway.h, ...).
 *
 * Translation units should include the focused header they actually need
 * instead of this umbrella (god-header elimination). This file exists only
 * for legacy/repo-wide consumers that want everything in one include.
 *
 * @{

 Historically this was a 1700+ line god header. It has been decomposed:
   - core types + constants  -> hermes_core_types.h (no circular includes)
   - agent/message/session   -> hermes_agent.h
   - CLI commands + cli_main -> hermes_cli.h
   - approval/clarify/cache  -> approval.h
   - browser CDP url config  -> hermes_cdp.h (-> src/tools/browser_tool_cdp.h)
   - redaction/sanitize      -> hermes_redact.h / hermes_sanitize.h
   - url blocklist/allowlist -> hermes_url_safety.h
   - credential vault        -> hermes_vault.h
   - audit log               -> hermes_audit.h
   - rate limiting           -> hermes_rate_limit.h
   - tool result storage     -> hermes_result_storage.h
   - config setup helpers    -> hermes_config_setup.h
   - delegate spawn pause    -> hermes_delegate.h
   - xAI model retirement    -> hermes_provider_xai.h
 * @}
 */

#ifndef HERMES_H
#define HERMES_H

/* Core types first (no circular includes). */
#include "hermes_core_types.h"

/* Dependency wrapper + subsystem headers (each self-contained). */
#include "hermes_json.h"
#include "hermes_yaml.h"
#include "hermes_http.h"
#include "hermes_crypto.h"
#include "hermes_db.h"
#include "hermes_display.h"
#include "hermes_skin.h"
#include "hermes_agent.h"
#include "hermes_credits_tracker.h"
#include "hermes_plugin.h"
#include "hermes_memory.h"
#include "hermes_tirith.h"
#include "hermes_media_cache.h"
#include "hermes_cli.h"
#include "hermes_cron.h"
#include "hermes_skills.h"

/* Focused subsystem protocol headers (the symbols that used to live here). */
#include "approval.h"
#include "hermes_cdp.h"
#include "hermes_redact.h"
#include "hermes_sanitize.h"
#include "hermes_url_safety.h"
#include "hermes_vault.h"
#include "hermes_audit.h"
#include "hermes_rate_limit.h"
#include "hermes_result_storage.h"
#include "hermes_config_setup.h"
#include "hermes_delegate.h"
#include "hermes_provider_xai.h"
#include "hermes_gateway.h"

/* Registry accessors — declared in registry.h. */
#include "registry.h"

#endif /* HERMES_H */
