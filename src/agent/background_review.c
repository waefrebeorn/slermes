/*
 * background_review.c — Port of Python agent/background_review.py
 *
 * Python API → C implementation mapping:
 *   background_review() / review_conversation()
 *       → llm_background_review() in llm_client.c (declared in hermes_agent.h:159)
 *   summarize_background_review_actions()
 *       → summarize_background_review_actions() in hermes_agent.h:167
 *
 * All ported functions already exist with their proper C names in
 * llm_client.c / hermes_agent.h. This file is name-parity only.
 * Background review is called from run_conversation in conversation_loop.c
 * when state->enable_background_review is true.
 *
 * Key signatures in hermes_agent.h:
 *   char *llm_background_review(llm_config_t *cfg, const char *tool_name,
 *                               const char *tool_args, const char *tool_result);
 *   char *summarize_background_review_actions(const char *review_messages_json,
 *                                             const char *prior_snapshot_json);
 */

#include "hermes_agent.h"   /* llm_background_review(), summarize_background_review_actions() */
