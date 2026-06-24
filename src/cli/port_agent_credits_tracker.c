/*
 * port_agent_credits_tracker.c — C port of agent/credits_tracker.py
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: cli_agent_credits_tracker_is_free_tier_model @ agent/credits_tracker.py:is_free_tier_model */

/* Port of Python agent/credits_tracker.py:is_free_tier_model */
/* Returns True when model is a Nous free-tier model (:free suffix check). */
bool cli_agent_credits_tracker_is_free_tier_model(const char *model, const char *base_url)
{
    (void)base_url;
    if (!model || !model[0]) return false;
    /* Check for :free suffix — the canonical Nous free SKU marker */
    size_t len = strlen(model);
    if (len >= 5 && strcmp(model + len - 5, ":free") == 0) return true;
    /* Pricing cache lookup omitted — CLI/TUI-specific, gateway uses :free suffix only */
    return false;
}
