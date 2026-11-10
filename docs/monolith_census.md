# Slermes monolith census
Weighted by line count. A file is a *movable-island* candidate for a monolith split only if it has ZERO file-scope static tables / static const scalars / static state (`statics` column = function-statics, which do not block a split — only the table/scalar state does).

| file | lines | static tables | scalar/ptr statics | static fns | tables |
|---|---|---|---|---|---|
| src/cli/tui_fullscreen.c | 5169 | 0 | 26 | 103 | — |
| src/web_server.c | 3437 | 0 | 5 | 84 | — |
| src/agent/provider_anthropic.c | 3334 | 0 | 0 | 6 | — |
| src/cli/display_core.c | 3123 | 1 | 3 | 3 | SPINNER_CHARS |
| src/agent/provider_metadata.c | 2912 | 2 | 1 | 4 | MODELS, PROVIDERS |
| src/cli/config_setup.c | 2709 | 1 | 2 | 28 | SETUP_KEY_INFO |
| src/tools/skills.c | 2599 | 0 | 2 | 21 | — |
| src/agent/llm_client.c | 2548 | 0 | 0 | 11 | — |
| src/cli/kanban_workers.c | 2407 | 0 | 1 | 8 | — |
| src/agent/provider_google.c | 2222 | 0 | 0 | 1 | — |
| src/tools/computer_use.c | 2216 | 0 | 1 | 21 | — |
| src/skills_hub.c | 2115 | 0 | 10 | 6 | — |
| src/tools/terminal.c | 2110 | 0 | 0 | 5 | — |
| src/gateway/platforms/email.c | 2043 | 0 | 2 | 15 | — |
| src/cron/port_cron_jobs.c | 2012 | 0 | 0 | 20 | — |
| src/tools/mcp_tool.c | 1931 | 0 | 1 | 7 | — |
| src/gateway/platforms/weixin.c | 1849 | 0 | 1 | 20 | — |
| src/tools/browser.c | 1833 | 0 | 3 | 2 | — |
| src/cli/config.c | 1814 | 0 | 0 | 2 | — |
| src/cli/cli_cmd_system.c | 1806 | 0 | 0 | 0 | — |
| src/api_server.c | 1776 | 0 | 6 | 35 | — |
| src/agent/conversation_loop.c | 1775 | 0 | 0 | 1 | — |
| src/gateway/platforms/yuanbao.c | 1769 | 0 | 4 | 12 | — |
| src/web_dashboard.c | 1754 | 0 | 7 | 55 | — |
| src/agent/auxiliary_client.c | 1671 | 5 | 7 | 3 | AUX_MODEL_FALLBACK, CHAIN_LABELS, PROVIDER_ALIASES, TASK_LABELS, VISION_MODELS |
| src/agent/provider_bedrock.c | 1613 | 0 | 0 | 4 | — |
| src/provider/token_exchange.c | 1610 | 0 | 2 | 3 | — |
| src/tools/port_process_registry.c | 1592 | 0 | 4 | 2 | — |
| src/tools/port_skills_sync.c | 1592 | 0 | 0 | 1 | — |
| src/agent/prompt_builder.c | 1586 | 1 | 2 | 3 | THREAT_PATTERNS |
| src/cli/cli_cmd_session.c | 1561 | 0 | 1 | 0 | — |
| src/gateway/server.c | 1518 | 0 | 0 | 0 | — |
| src/cli/port_web_server.c | 1496 | 0 | 0 | 3 | — |
| src/tools/port_image_generation_tool.c | 1489 | 1 | 0 | 0 | FAL_MODELS |
| src/gateway/port_gateway_run.c | 1393 | 0 | 0 | 5 | — |
| src/gateway/platforms/telegram.c | 1387 | 0 | 2 | 0 | — |
| src/cli/cli_cmd_config.c | 1383 | 0 | 0 | 0 | — |
| src/tools/voice_mode.c | 1372 | 0 | 5 | 3 | — |
| src/gateway/status.c | 1361 | 0 | 1 | 19 | — |
| src/agent/coding_context.c | 1335 | 0 | 0 | 3 | — |
| src/tools/registry.c | 1309 | 0 | 4 | 5 | — |
| src/cli/port_tools_session_search_tool.c | 1305 | 0 | 0 | 5 | — |
| src/cli/tui_json_rpc.c | 1297 | 0 | 2 | 3 | — |
| src/tools/curator_backup.c | 1270 | 0 | 0 | 15 | — |
| src/gateway/config.c | 1268 | 0 | 2 | 16 | — |
| src/cli/port_cli_profiles.c | 1267 | 0 | 3 | 4 | — |
| src/desktop_app_common.c | 1261 | 0 | 13 | 0 | — |
| src/tools/memory_storage.c | 1251 | 0 | 0 | 40 | — |
| src/cli/cli.c | 1238 | 0 | 7 | 9 | — |
| src/agent/insights.c | 1187 | 0 | 0 | 12 | — |
| src/cli/port_learning_graph_render.c | 1173 | 0 | 0 | 6 | — |
| src/tools/mixture_of_agents.c | 1171 | 7 | 1 | 7 | MOA_DEVIL_REFS, MOA_MATH_REFS, MOA_MEGA_TOTEM_REFS, MOA_PROVIDERS, MOA_STANDARD_REFS, MOA_TOKEN_MAXX_REFS |
| src/gateway/gateway_runner.c | 1168 | 0 | 0 | 0 | — |
| src/tools/patch.c | 1163 | 0 | 0 | 9 | — |
| src/tools/url_safety.c | 1140 | 0 | 4 | 9 | — |
| src/cli/port_web_git.c | 1132 | 0 | 0 | 7 | — |
| src/tools/port_approval.c | 1104 | 0 | 4 | 3 | — |
| src/agent/agent_message_sanitize.c | 1103 | 0 | 0 | 4 | — |
| src/tools/delegate.c | 1100 | 0 | 6 | 24 | — |
| src/cli/port_platforms_base_helpers.c | 1076 | 0 | 3 | 2 | — |
| src/tools/process_registry.c | 1060 | 0 | 2 | 10 | — |
| src/cli/cli_gaps.c | 1049 | 0 | 0 | 0 | — |
| src/tools/web.c | 1049 | 0 | 0 | 4 | — |
| src/acp/server.c | 1029 | 0 | 0 | 2 | — |
| src/agent/context_references.c | 1023 | 0 | 1 | 9 | — |
| src/tools/skill_mgmt.c | 1013 | 0 | 0 | 6 | — |
| src/gateway/platforms/api_server_adapter.c | 1003 | 0 | 0 | 4 | — |
| src/plugins/plugin_kanban.c | 1000 | 0 | 6 | 9 | — |
| src/agent/plugin_llm.c | 994 | 0 | 0 | 5 | — |
| src/gateway/platforms/webhook.c | 994 | 0 | 4 | 6 | — |
| src/agent/skill_commands.c | 992 | 0 | 3 | 9 | — |
| src/gateway/run_pure.c | 992 | 0 | 0 | 1 | — |
| src/cli/commands.c | 984 | 0 | 0 | 3 | — |
| src/window_win32.c | 976 | 0 | 5 | 6 | — |
| src/tools/moa_performance.c | 972 | 0 | 5 | 8 | — |
| src/agent/curator.c | 968 | 0 | 0 | 1 | — |
| src/agent/shell_hooks.c | 946 | 0 | 1 | 6 | — |
| src/tools/memory.c | 946 | 0 | 3 | 13 | — |
| src/cli/port_kanban_db.c | 942 | 0 | 0 | 2 | — |
| src/cli/kanban_lifecycle.c | 924 | 0 | 0 | 3 | — |
| src/tools/file.c | 914 | 0 | 0 | 0 | — |
| src/chat_render.c | 912 | 0 | 1 | 7 | — |
| src/cli/kanban_util.c | 912 | 0 | 0 | 4 | — |
| src/agent/agent_loop.c | 911 | 0 | 0 | 3 | — |
| src/cli/port_auth_na.c | 911 | 0 | 0 | 0 | — |
| src/cron/cron_extras.c | 910 | 0 | 4 | 0 | — |
| src/agent/image_routing.c | 907 | 0 | 0 | 5 | — |
| src/cli/port_blueprint_catalog_helpers.c | 905 | 0 | 0 | 2 | — |
| src/cli/port_goals_manager.c | 903 | 0 | 0 | 3 | — |
| src/cli/port_models_pure.c | 900 | 0 | 0 | 0 | — |
| src/cli/port_config_py_pure.c | 882 | 0 | 0 | 4 | — |
| src/cli/port_web_server_schema_path.c | 882 | 0 | 4 | 0 | — |
| src/tools/checkpoint_manager.c | 878 | 0 | 0 | 24 | — |
| src/cli/setup_wizard.c | 875 | 2 | 0 | 28 | BACKENDS, PROVIDERS |
| src/gateway/port_gateway_platforms_base_media.c | 875 | 0 | 0 | 6 | — |
| src/gateway/run_pure2.c | 870 | 0 | 0 | 3 | — |
| src/gateway/platforms/feishu.c | 869 | 0 | 6 | 1 | — |
| src/gateway/helpers.c | 863 | 1 | 0 | 2 | G_SECRET_SLOTS |
| src/tools/file_batch.c | 852 | 0 | 0 | 8 | — |
| src/tools/port_send_message_tool.c | 852 | 0 | 0 | 1 | — |
| src/agent/codex_app_server_session.c | 849 | 0 | 0 | 6 | — |
| src/tools/account_usage.c | 845 | 0 | 0 | 1 | — |
| src/agent/file_safety.c | 841 | 0 | 0 | 8 | — |
| src/cli/model_catalog.c | 840 | 5 | 0 | 2 | PROVIDER_ALIASES, PROVIDER_GROUPS, PROVIDER_LABELS, PROVIDER_MODELS, SILENT_DEFAULTS |
| src/tools/image_gen.c | 838 | 0 | 0 | 4 | — |
| src/tools/session_crud.c | 838 | 0 | 0 | 0 | — |
| src/agent/agent_message_repair.c | 835 | 0 | 0 | 1 | — |
| src/tools/port_mcp_tool.c | 830 | 0 | 0 | 0 | — |
| src/cli/port_yuanbao_proto_helpers.c | 826 | 0 | 2 | 16 | — |
| src/cli/port_fuzzy_match_helpers.c | 819 | 1 | 0 | 12 | UNICODE_MAP |
| src/provider/google_oauth.c | 819 | 0 | 3 | 4 | — |
| src/chat_composer.c | 807 | 0 | 3 | 13 | — |
| src/cli/port_cli.c | 798 | 0 | 0 | 0 | — |
| src/gateway/gateway_runtime.c | 794 | 0 | 8 | 1 | — |
| src/agent/context.c | 790 | 0 | 0 | 1 | — |
| src/gateway/port_gateway_relay_ws_transport.c | 788 | 0 | 0 | 3 | — |
| src/tools/port_memory_tool.c | 776 | 0 | 0 | 6 | — |
| src/agent/models_dev.c | 773 | 0 | 1 | 8 | — |
| src/cli/port_provider_registry.c | 773 | 1 | 0 | 3 | CATALOG |
| src/tools/send_message.c | 768 | 0 | 0 | 3 | — |
| src/terminal.c | 765 | 0 | 0 | 14 | — |
| src/tools/port_tools_read_extract.c | 764 | 0 | 0 | 5 | — |
| src/agent/redact.c | 762 | 1 | 2 | 8 | BUILTIN_PATTERNS |
| src/gateway/port_gateway_relay_adapter.c | 758 | 0 | 4 | 3 | — |
| src/tools/discord.c | 746 | 0 | 0 | 0 | — |
| src/agent/tool_guardrails.c | 736 | 0 | 0 | 3 | — |
| src/tools/feishu_tools.c | 735 | 0 | 4 | 0 | — |
| src/cli/kanban_tasks.c | 731 | 0 | 0 | 2 | — |
| src/gateway/platforms/signal.c | 731 | 0 | 3 | 6 | — |
| src/hermes_cli/projects_db.c | 728 | 0 | 0 | 2 | — |
| src/agent/usage_pricing.c | 725 | 1 | 0 | 7 | PRICING_TABLE |
| src/tools/tool_search.c | 722 | 0 | 0 | 1 | — |
| src/agent/markdown_tables.c | 720 | 0 | 0 | 0 | — |
| src/cli/port_vault_login_classifier.c | 719 | 0 | 0 | 15 | — |
| src/cli/tui_eventpub.c | 718 | 0 | 0 | 4 | — |
| src/tools/yuanbao_tools.c | 714 | 1 | 0 | 2 | YB_STICKERS |
| src/tools/approval.c | 709 | 0 | 4 | 5 | — |
| src/agent/provider_codex_responses.c | 706 | 0 | 0 | 1 | — |

**Honest reading:** every big file carries file-scope table/static, so none is a `provider_anthropic.c`-style island (that one was special: its endpoint/auth classifiers had zero file-state and a public header).
The `provider_endpoint_detect.c` split (`36277da`) is the one provable monolith split; the durability gate (full build RC=0 + oracle 95/95) rejects any forced split that ships table state to a god-header, and the oracle suite cannot runtime-verify curses/web/TUI behavior, so coercing those risks silent regressions.
Generated by the file-scope-table census; regenerable with the same static+table regex on the striped source.