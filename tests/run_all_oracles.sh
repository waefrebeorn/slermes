#!/usr/bin/env bash
# run_all_oracles.sh - verify EVERY committed oracle in one command.
#
# Differential-testing regression gate (the "land it green" step for a port):
# compiles+links each tests/t_port_<name>.c, pipes it into tests/oracle_<name>.py,
# and requires ``ORACLE <name>: N cases, 0 mismatches`` + exit 0 for every pair.
# Any drift (a regression, a Smart-App-Control refusal, a missing object) fails
# the run and names the port.
#
# Dependency default: src/cli/port_<name>.o + lib/libjson/json.o + (regex if the
# harness includes hermes_regex.h). Override per port with the ORACLE_OBJS_<NAME>
# env var or an entry below under OVERRIDES (function is a module-not-name-safe
# hook for the handful of oracles whose harness needs extra/custom objects).
#
# Usage:
#   bash tests/run_all_oracles.sh                 # run all
#   bash tests/run_all_oracles.sh name1 name2     # run a subset
#   ORACLE_OBJS_IDLE_WATCH="src/cli/port_idle_watch.o lib/libregex/hermes_regex.o lib/libjson/json.o" \
#     bash tests/run_all_oracles.sh idle_watch

set -u
# Failure-log artifacts must describe THIS run only.
rm -f .testbin/oracle_fail_*.log .testbin/retried_gates.*.txt
cd "$(dirname "$0")/.." || exit 1
mkdir -p .testbin/ao
RUNNING_FULL=0

# legacy: discovery name -> harness basename (when the oracle filename and the
# t_port_<name>.c differ for a pre-existing port).
declare -A HARNESS_ALIAS=(
  [audit_bounded]="audit"
  [callbacks_skip]="callbacks"
)

# special deps: name -> FULL MINIMAL_OBJS (ports whose own code pulls extra lib
# objects the default auto-set misses). Override any on the CLI with
# ORACLE_OBJS_<NAME>.
declare -A DEPS=(
  [api_server_flow]="FULL"
  [cch_flow]="FULL"
  [cli_gaps_revive2]="FULL"
  [cli_gaps_revive1]="FULL"
  [config_migration_flow]="FULL"
  [conversation_compression_flow]="FULL"
  [gateway_flow]="FULL"
  [gateway_inbound_flow]="FULL"
  [gateway_revive]="FULL"
  [main_cli_flow]="FULL"
  [plugin_revive]="FULL"
  [run_gaps_fix]="FULL"
  [session_policy_flow3]="FULL"
  [setup_revive]="FULL"
  [skills_guard_flow]="FULL"
  [tail_revive4]="FULL"
  # aliased harness names: the object follows the HARNESS name, not the discovery
  # oracle name.
  [aux_event_has_content]="src/agent/port_auxiliary_client.o src/hermes_cli/port_version_info.o lib/libjson/json.o"
  [cron_delivery_preds]="src/cron/scheduler_gaps.o lib/libjson/json.o"
  [bucket_limit]="src/tools/web_result_cache_gaps.o lib/libjson/json.o"
  [cache_point_allowed]="src/agent/bedrock_adapter_gaps.o lib/libjson/json.o"
  [get_context_timeout]="src/agent/prompt_builder_gaps.o lib/libjson/json.o"
  [owner_matches]="src/tools/skills_hub_gaps.o lib/libjson/json.o"
  [refresh_horizon]="src/hermes_cli/nous_auth_keepalive_gaps.o lib/libjson/json.o"
  [autoclose]="src/hermes_cli/browser_connect_gaps.o lib/libjson/json.o"
  [cron_drain]="src/hermes_cli/gateway_gaps2.o lib/libjson/json.o"
  [whatsapp_identity]="src/hermes_cli/whatsapp_identity.o lib/libjson/json.o"
  [session_db_timeout]="src/cron/scheduler_gaps.o lib/libjson/json.o"
  [real_profile_pin]="src/hermes_cli/browser_connect_gaps.o lib/libjson/json.o"
  [delivery_platform_routed]="src/cron/scheduler_gaps.o lib/libjson/json.o"
  [pid_record_belongs]="src/gateway/status_gaps.o lib/libjson/json.o"
  [memory_available]="src/tools/memory_available_port.o lib/libjson/json.o"
  [profiles_warn]="src/hermes_cli/profiles_warn_port.o lib/libjson/json.o"
  [codex_event_content]="src/agent/auxiliary_client_gaps.o lib/libjson/json.o"
  [ac3_native_fallback]="src/agent/auxiliary_client_gaps.o lib/libjson/json.o"
  [auxiliary_client_flow3]="FULL"
  [acp_tools_flow2]="lib/libjson/json.o"
  [acp_modes]="src/acp_adapter/modes.o lib/libjson/json.o"
  [cfgturn]="src/hermes_cli/config_turn_limit.o lib/libjson/json.o"
  [crontri]="src/cron/jobs_tristate.o lib/libjson/json.o"
  [touttrunc]="src/tools/tool_output_truncate.o lib/libjson/json.o"
  [runcsv]="src/gateway/run_common.o lib/libjson/json.o"
  [urlfmt]="src/hermes_cli/url_utils.o lib/libjson/json.o"
  [cronconst]="src/cron/constants.o lib/libjson/json.o"
  [w6_acpclient_6f530e]="src/agent/acp_client.o lib/libjson/json.o"
  [w6_legacycli_b0ce00]="src/agent/legacy_cli.o lib/libjson/json.o"
  [w6_turnusage_47b7c5]="src/agent/turn_usage.o lib/libjson/json.o"
  [w6_missedruns_9b981e]="src/cron/missed_runs.o lib/libjson/json.o"
  [w6_occurrences_d831fa]="src/cron/occurrences.o lib/libjson/json.o"
  [w6_promptscan_439e13]="src/cron/prompt_scan.o lib/libjson/json.o"
  [w6_init_0d6087]="src/gateway/__init__.o lib/libjson/json.o"
  [w6_runturn_6f520a]="src/gateway/run_turn.o lib/libjson/json.o"
  [w6_vault_9ab785]="src/hermes_cli/vault.o lib/libjson/json.o"
  [w6_init_d99929]="src/plugins/__init__.o lib/libjson/json.o"
  [w6_plugininput_ee7719]="src/pm/plugin_inputs.o lib/libjson/json.o"
  [w6_pluginsstat_d01aee]="src/pm/plugins_state.o lib/libjson/json.o"
  [w6_runtimestag_fecd45]="src/pm/runtime_stage.o lib/libjson/json.o"
  [w6_skilllock_842861]="src/tools/skill_lock.o lib/libjson/json.o"
  [w6_voicelive_605a89]="src/tools/voice_live.o lib/libjson/json.o"
  [w6_entry_b16419]="src/acp_adapter/entry.o lib/libjson/json.o"
  [w6_tools_9c1fca]="src/acp_adapter/tools.o lib/libjson/json.o"
  [w6_contextpin_633375]="src/agent/context_pin.o lib/libjson/json.o"
  [w6_i18nlayers_f42323]="src/agent/i18n_layers.o lib/libjson/json.o"
  [w6_sleepclock_919d0b]="src/agent/sleep_clock.o lib/libjson/json.o"
  [w6_turnauthor_9c0bab]="src/agent/turn_author.o lib/libjson/json.o"
  [w6_turnfacade_bf7ace]="src/agent/turn_facade.o lib/libjson/json.o"
  [w6_vaultstore_6d2c23]="src/agent/vault_store.o lib/libjson/json.o"
  [w6_cancellation_b52bf7]="src/cron/cancellation.o lib/libjson/json.o"
  [w6_storehealth_0f6948]="src/cron/store_health.o lib/libjson/json.o"
  [w6_rungoals_ec4200]="src/gateway/run_goals.o lib/libjson/json.o"
  [w6_runvoice_5cc499]="src/gateway/run_voice.o lib/libjson/json.o"
  [w6_webapp_879137]="src/hermes_cli/webapp.o lib/libjson/json.o"
  [w6_carriedextr_6bdda2]="src/pm/carried_extras.o lib/libjson/json.o"
  [w6_downloadsta_5c6fdc]="src/pm/download_state.o lib/libjson/json.o"
  [w6_installstat_b9e0b6]="src/pm/install_states.o lib/libjson/json.o"
  [w6_uvcachepru_3aea47]="src/pm/uv_cache_prune.o lib/libjson/json.o"
  [w6_feishulark_1501ff]="src/tools/feishu_lark.o lib/libjson/json.o"
  [w6_entry_fb4871]="src/tui_gateway/entry.o lib/libjson/json.o"
  [w6_events_756cad]="src/acp_adapter/events.o lib/libjson/json.o"
  [w6_server_e95893]="src/acp_adapter/server.o lib/libjson/json.o"
  [w6_cachebounds_876475]="src/agent/cache_bounds.o lib/libjson/json.o"
  [w6_imagereplay_7778b4]="src/agent/image_replay.o lib/libjson/json.o"
  [w6_proxybypass_aae960]="src/agent/proxy_bypass.o lib/libjson/json.o"
  [w6_redactzhipu_4cd0af]="src/agent/redact_zhipu.o lib/libjson/json.o"
  [w6_secretscope_bc3fee]="src/agent/secret_scope.o lib/libjson/json.o"
  [w6_servedmodel_e72b96]="src/agent/served_model.o lib/libjson/json.o"
  [w6_usageanchor_c3af05]="src/agent/usage_anchor.o lib/libjson/json.o"
  [w6_usagegauges_1dd126]="src/agent/usage_gauges.o lib/libjson/json.o"
  [w6_dispatchhol_96dc54]="src/cron/dispatch_hold.o lib/libjson/json.o"
  [w6_buildagent_361323]="src/docker/build_agent.o lib/libjson/json.o"
  [w6_runtopics_d5aa88]="src/gateway/run_topics.o lib/libjson/json.o"
  [w6_steward_ec934f]="src/hermes_cli/steward.o lib/libjson/json.o"
  [w6_buildcompil_8eb661]="src/pm/build_compilers.o lib/libjson/json.o"
  [w6_pluginevict_1966c8]="src/pm/plugin_eviction.o lib/libjson/json.o"
  [w6_uvcachehea_d7f380]="src/pm/uv_cache_health.o lib/libjson/json.o"
  [w6_init_cd8903]="src/providers/__init__.o lib/libjson/json.o"
  [w6_discordtool_3ef112]="src/tools/discord_tool.o lib/libjson/json.o"
  [w6_mcpliveness_08bec8]="src/tools/mcp_liveness.o lib/libjson/json.o"
  [w6_tourpresets_052f0d]="src/tools/tour_presets.o lib/libjson/json.o"
  [w6_render_d7d0eb]="src/tui_gateway/render.o lib/libjson/json.o"
  [w6_server_8d971d]="src/tui_gateway/server.o lib/libjson/json.o"
  [w6_catalog_c1c3fb]="src/acp_adapter/catalog.o lib/libjson/json.o"
  [w6_content_cc45ae]="src/acp_adapter/content.o lib/libjson/json.o"
  [w6_session_5810af]="src/acp_adapter/session.o lib/libjson/json.o"
  [w6_idleschedul_b709d3]="src/agent/idle_schedule.o lib/libjson/json.o"
  [w6_providerbas_bb26d7]="src/agent/provider_base.o lib/libjson/json.o"
  [w6_redactdbur_875c0b]="src/agent/redact_db_url.o lib/libjson/json.o"
  [w6_routebindin_0a632a]="src/agent/route_binding.o lib/libjson/json.o"
  [w6_runtimescop_c06314]="src/agent/runtime_scope.o lib/libjson/json.o"
  [w6_turnapical_c014d7]="src/agent/turn_api_call.o lib/libjson/json.o"
  [w6_turnoverflo_10ab04]="src/agent/turn_overflow.o lib/libjson/json.o"
  [w6_turnrecover_012061]="src/agent/turn_recovery.o lib/libjson/json.o"
  [w6_jobdefiniti_2ab816]="src/cron/job_definition.o lib/libjson/json.o"
  [w6_hostattach_55be4c]="src/gateway/host_attach.o lib/libjson/json.o"
  [w6_mediafetch_d0155c]="src/gateway/media_fetch.o lib/libjson/json.o"
  [w6_neuttsynth_668f10]="src/tools/neutts_synth.o lib/libjson/json.o"
  [w5_hermesyaml_f6b6a1]="src/hermes_yaml.o lib/libjson/json.o"
  [w5_hermesscope_980e36]="src/hermes_scope.o lib/libjson/json.o"
  [w5_hermesdira_b128e3]="src/hermes_dir_acl.o lib/libjson/json.o"
  [w5_sessionresc_d445b0]="src/session_rescue.o lib/libjson/json.o"
  [w5_lineageadop_be48a0]="src/lineage_adoption.o lib/libjson/json.o"
  [w5_storageuser_ed737a]="src/storage_user_copy.o lib/libjson/json.o"
  [w5_persistence_4744ac]="src/persistence_assembly.o lib/libjson/json.o"
  [w5_hermesstate_732fec]="src/hermes_state_registry.o lib/libjson/json.o"
  [w5_toolsetdist_3d9ebc]="src/toolset_distributions.o lib/libjson/json.o"
  [w5_hermesstart_a84a61]="src/hermes_startup_watchdog.o lib/libjson/json.o"
  [w5_hermesstate_a0163a]="src/hermes_state_loop_guard.o lib/libjson/json.o"
  [w5_hermesconst_d195ca]="src/hermes_constants_scratch.o lib/libjson/json.o"
  [w5_hermesconst_b6148b]="src/hermes_constants_scratch_stuck.o lib/libjson/json.o"
  [w5_hermesconst_e6f362]="src/hermes_constants_scratch_detach.o lib/libjson/json.o"
  [w5_uv_f7190a]="src/pm/_uv.o lib/libjson/json.o"
  [w5_cli_d7cb5a]="src/pm/cli.o lib/libjson/json.o"
  [w5_lock_32c99e]="src/pm/lock.o lib/libjson/json.o"
  [w5_paths_f8daf3]="src/pm/paths.o lib/libjson/json.o"
  [w5_shell_7b6f0b]="src/pm/shell.o lib/libjson/json.o"
  [w5_store_63be39]="src/pm/store.o lib/libjson/json.o"
  [w5_client_039ee5]="src/pm/client.o lib/libjson/json.o"
  [w5_extras_3292f9]="src/pm/extras.o lib/libjson/json.o"
  [w5_launch_b4dec9]="src/pm/launch.o lib/libjson/json.o"
  [w5_update_6d82c1]="src/pm/update.o lib/libjson/json.o"
  [w5_worker_9c3898]="src/pm/worker.o lib/libjson/json.o"
  [w5_env_ee14b1]="src/utils/env.o lib/libjson/json.o"
  [w5_install_184801]="src/pm/install.o lib/libjson/json.o"
  [w5_network_5fdf96]="src/pm/network.o lib/libjson/json.o"
  [w5_package_78a049]="src/pm/package.o lib/libjson/json.o"
  [w5_receipt_6c967d]="src/pm/receipt.o lib/libjson/json.o"
  [w5_runtime_3f1397]="src/pm/runtime.o lib/libjson/json.o"
  [w5_testenv_ca5da8]="src/pm/testenv.o lib/libjson/json.o"
  [w5_urls_e1571a]="src/utils/urls.o lib/libjson/json.o"
  [w5_init_703dd3]="src/pm/__init__.o lib/libjson/json.o"
  [w5_defaults_7e72be]="src/pm/defaults.o lib/libjson/json.o"
  [w5_features_c0a63b]="src/pm/features.o lib/libjson/json.o"
  [w5_packages_6d8d5a]="src/pm/packages.o lib/libjson/json.o"
  [w5_progress_88711f]="src/pm/progress.o lib/libjson/json.o"
  [w5_recovery_15f063]="src/pm/recovery.o lib/libjson/json.o"
  [w5_registry_8fd451]="src/pm/registry.o lib/libjson/json.o"
  [w5_files_19ae74]="src/utils/files.o lib/libjson/json.o"
  [w5_buildenv_1eb2c8]="src/pm/build_env.o lib/libjson/json.o"
  [w5_libatomic_4bd942]="src/pm/libatomic.o lib/libjson/json.o"
  [w5_pipcheck_88ae1a]="src/pm/pip_check.o lib/libjson/json.o"
  [w5_workspace_b403c1]="src/pm/workspace.o lib/libjson/json.o"
  [w5_init_7fb779]="src/cron/__init__.o lib/libjson/json.o"
  [w5_jobargs_45f9c5]="src/cron/job_args.o lib/libjson/json.o"
  [w5_downloader_69e11a]="src/pm/downloader.o lib/libjson/json.o"
  [w5_filesystem_427a52]="src/pm/filesystem.o lib/libjson/json.o"
  [w5_operations_2c07e3]="src/pm/operations.o lib/libjson/json.o"
  [w5_init_6fa241]="src/agent/__init__.o lib/libjson/json.o"
  [w5_venventry_82a592]="src/pm/_venv_entry.o lib/libjson/json.o"
  [w5_environment_032cbd]="src/pm/environment.o lib/libjson/json.o"
  [w5_publication_1af7eb]="src/pm/publication.o lib/libjson/json.o"
  [w5_termuxlibs_3b3b3b]="src/pm/termux_libs.o lib/libjson/json.o"
  [w5_userfacing_57dafa]="src/pm/user_facing.o lib/libjson/json.o"
  [w5_base_c4cc9e]="src/providers/base.o lib/libjson/json.o"
  [w5_init_e47994]="src/tools/__init__.o lib/libjson/json.o"
  [w5_ws_735c83]="src/tui_gateway/ws.o lib/libjson/json.o"
  [w5_init_7d1849]="src/utils/__init__.o lib/libjson/json.o"
  [w5_botchain_6aa11f]="src/agent/bot_chain.o lib/libjson/json.o"
  [w5_nouswire_774ca4]="src/agent/nous_wire.o lib/libjson/json.o"
  [w5_relaycwd_c03229]="src/agent/relay_cwd.o lib/libjson/json.o"
  [w5_extrafire_54bebe]="src/cron/extra_fire.o lib/libjson/json.o"
  [w5_quotahold_3054bf]="src/cron/quota_hold.o lib/libjson/json.o"
  [w5_runapi_7d29d9]="src/gateway/run_api.o lib/libjson/json.o"
  [w5_markereval_a9d132]="src/pm/_marker_eval.o lib/libjson/json.o"
  [w5_environments_407599]="src/pm/environments.o lib/libjson/json.o"
  [w5_indexconfig_5a61c5]="src/pm/index_config.o lib/libjson/json.o"
  [w5_nativebuild_671b76]="src/pm/native_build.o lib/libjson/json.o"
  [w5_sttlease_a1f613]="src/tools/stt_lease.o lib/libjson/json.o"
  [w5_auth_845ce1]="src/acp_adapter/auth.o lib/libjson/json.o"
  [auxkeyrot]="src/agent/auxiliary_key_rotation.o lib/libjson/json.o"
  [auxlocal]="src/agent/auxiliary_local_runtime.o lib/libjson/json.o"
  [auxwire]="src/agent/auxiliary_wire.o lib/libjson/json.o"
  [ctxprompt]="src/agent/context_compressor_prompt_rules.o lib/libjson/json.o"
  [convscaf]="src/agent/conversation_compression_scaffolding.o lib/libjson/json.o"
  [memobounds]="src/agent/model_metadata_memo_bounds.o lib/libjson/json.o"
  [sesscomplete]="src/gateway/session_complete_path.o lib/libjson/json.o"
  [sessinfo]="src/gateway/session_info_route.o lib/libjson/json.o"
  [sessclose]="src/gateway/session_local_close.o lib/libjson/json.o"
  [clarifylock]="src/gateway/session_clarify_lock.o lib/libjson/json.o"
  [ctrldisp]="src/gateway/control_dispatch.o lib/libjson/json.o"
  [cfgwenv]="src/gateway/config_env.o lib/libjson/json.o"
  [bridgedenv]="src/gateway/bridged_env.o lib/libjson/json.o"
  [botloop]="src/gateway/bot_loop_guard.o lib/libjson/json.o"
  [cfgdiag]="src/hermes_cli/config_check_diagnostics.o lib/libjson/json.o"
  [modelslock]="src/hermes_cli/models_cache_lock.o lib/libjson/json.o"
  [profilecache]="src/hermes_cli/models_profile_cache.o lib/libjson/json.o"
  [plugcmd]="src/hermes_cli/plugin_command_dispatch.o lib/libjson/json.o"
  [updcommon]="src/hermes_cli/update_cmd_common.o lib/libjson/json.o"
  [wsstoken]="src/hermes_cli/web_server_session_token.o lib/libjson/json.o"
  [jobstime]="src/cron/jobs_time.o lib/libjson/json.o"
  [schedtick]="src/cron/scheduler_tick.o lib/libjson/json.o"
  [appdetect]="src/tools/approval_detection.o lib/libjson/json.o"
  [fileopscommon]="src/tools/file_operations_common.o lib/libjson/json.o"
  [imgcatalog]="src/tools/image_generation_catalog.o lib/libjson/json.o"
  [skillsearch]="src/tools/skills_hub_search.o lib/libjson/json.o"
  [tgdraft]="src/plugins/platforms/telegram/telegram_draft_budget.o lib/libjson/json.o"
  [slslash]="src/plugins/platforms/slack/adapter_slash_payload.o lib/libjson/json.o"
  [connrefuse]="src/tui_gateway/connector_refusals.o lib/libjson/json.o"
  [themeb]="src/tui_gateway/theme_bridge.o lib/libjson/json.o"
  [cliworktree]="src/hermes_cli/cli_commands_worktree.o lib/libjson/json.o"
  [cliundo]="src/hermes_cli/cli_undo_hint.o lib/libjson/json.o"
  [planlimits]="src/agent/plan_limits.o lib/libjson/json.o"
  [statusout]="src/agent/status_output.o lib/libjson/json.o"
  [toollabels]="src/tools/tool_labels.o lib/libjson/json.o"
  [argcoerce]="src/tools/arg_coercion.o lib/libjson/json.o"
  [cn_eb2be70575]="src/agent/agent_init_params.o lib/libjson/json.o"
  [cn_f3e135e7d4]="src/agent/chat_completion_stream_monitor.o lib/libjson/json.o"
  [cn_ceab6d16ce]="src/agent/context_compressor_prellm.o lib/libjson/json.o"
  [cn_19fc4acbec]="src/agent/delegation_provider.o lib/libjson/json.o"
  [cn_66abc92c44]="src/agent/search_policy.o lib/libjson/json.o"
  [cn_ead89e817f]="src/agent/vault_backends/bitwarden.o lib/libjson/json.o"
  [cn_00cfb3ece3]="src/gateway/platforms/access_policy_mixin.o lib/libjson/json.o"
  [cn_325783c15a]="src/gateway/runtime_bootstrap.o lib/libjson/json.o"
  [cn_821f2df8af]="src/gateway/session_hosted_controls.o lib/libjson/json.o"
  [cn_4fad7bc068]="src/gateway/session_hosted_rpc.o lib/libjson/json.o"
  [cn_1c03c4c0d9]="src/gateway/stream_consumer_transport.o lib/libjson/json.o"
  [cn_5a9ff58aea]="src/gateway/turn_executor.o lib/libjson/json.o"
  [cn_70050f77ca]="src/hermes_cli/cli_backtrack.o lib/libjson/json.o"
  [cn_b0510ae425]="src/hermes_cli/dashboard_auth/public_paths.o lib/libjson/json.o"
  [cn_207bbdad81]="src/hermes_cli/proxy/adapters/xai.o lib/libjson/json.o"
  [cn_42052f4e41]="src/plugins/browser/_common.o lib/libjson/json.o"
  [cn_eff47defba]="src/plugins/browser/browserbase/provider.o lib/libjson/json.o"
  [cn_d2cfe3135a]="src/plugins/browser/firecrawl/provider.o lib/libjson/json.o"
  [cn_9c9d9ed12b]="src/plugins/cron_providers/chronos/_nas_client.o lib/libjson/json.o"
  [cn_a2ea107778]="src/plugins/model-providers/commandcode/__init__.o lib/libjson/json.o"
  [cn_0727c5d06d]="src/plugins/model-providers/deepseek/__init__.o lib/libjson/json.o"
  [cn_0cc94385e1]="src/plugins/model-providers/nebius-token-factory/__init__.o lib/libjson/json.o"
  [cn_51f149a2c6]="src/plugins/platforms/buzz/buzz_cursors.o lib/libjson/json.o"
  [cn_c9aed165f4]="src/plugins/platforms/discord/adapter_live_gates.o lib/libjson/json.o"
  [cn_187d876a40]="src/plugins/platforms/discord/adapter_thread_titles.o lib/libjson/json.o"
  [cn_27753cf9ee]="src/plugins/platforms/discord/dashboard_card.o lib/libjson/json.o"
  [cn_3145e44260]="src/plugins/platforms/discord/recovery.o lib/libjson/json.o"
  [cn_1978b51517]="src/plugins/platforms/feishu/adapter_choice_cards.o lib/libjson/json.o"
  [cn_405f7929b1]="src/plugins/platforms/feishu/dashboard_card.o lib/libjson/json.o"
  [cn_fabf8dbd87]="src/plugins/platforms/matrix/dashboard_card.o lib/libjson/json.o"
  [cn_966d9e89b2]="src/plugins/platforms/slack/dashboard_card.o lib/libjson/json.o"
  [cn_fad49a65cf]="src/plugins/platforms/telegram/dashboard_card.o lib/libjson/json.o"
  [cn_62ae795a05]="src/plugins/web/brave_free/provider.o lib/libjson/json.o"
  [cn_052b37daf8]="src/tools/approval_config_set.o lib/libjson/json.o"
  [cn_97ac1634a4]="src/tools/approval_hermes_store.o lib/libjson/json.o"
  [cn_d6a5548205]="src/tools/approval_variation_selectors.o lib/libjson/json.o"
  [cn_0eaab1f03b]="src/tools/bot_desktop/rfb_filter.o lib/libjson/json.o"
  [cn_1495dc4c38]="src/tools/computer_use/schema.o lib/libjson/json.o"
  [cn_19ed142069]="src/tools/environments/base_wait.o lib/libjson/json.o"
  [cn_8d43370842]="src/tools/mcp_tool_health.o lib/libjson/json.o"
  [cn_037e736778]="src/tools/pty_query_responder.o lib/libjson/json.o"
  [cn_de44865c90]="src/tui_gateway/event_publisher.o lib/libjson/json.o"
  [mstarget]="src/plugins/platforms/matrix/send_targets.o lib/libjson/json.o"
  [sltarget]="src/plugins/platforms/slack/send_targets.o lib/libjson/json.o"
  [whtarget]="src/gateway/webhook_targets.o lib/libjson/json.o"
  [cmdunknown]="src/hermes_cli/cli_unknown_command.o lib/libjson/json.o"
  [dashproc]="src/persistence/sqlite/_dashboard_procs.o lib/libjson/json.o"
  [storlim]="src/storage_limits.o lib/libjson/json.o"
  [lazyfwd]="src/agent/lazy_forward.o lib/libjson/json.o"
  [memokeys]="src/agent/model_metadata_memo_keys.o lib/libjson/json.o"
  [redhome]="src/agent/redact_hermes_home.o lib/libjson/json.o"
  [onfacts]="src/agent/onboarding_facts.o lib/libjson/json.o"
  [artmirror]="src/pm/artifact_mirror.o lib/libjson/json.o"
  [qfile]="src/hermes_cli/main_query_file.o lib/libjson/json.o"
  [jobsnorm]="src/cron/jobs_normalize.o lib/libjson/json.o"
  [jobspin]="src/cron/jobs_model_pin.o lib/libjson/json.o"
  [scheddiag]="src/cron/scheduler_diagnostics.o lib/libjson/json.o"
  [secdisplay]="src/hermes_cli/config_secret_display.o lib/libjson/json.o"
  [sessowner]="src/hermes_cli/sessions_owner.o lib/libjson/json.o"
  [footersplit]="src/hermes_cli/cli_footer_split.o lib/libjson/json.o"
  [sesscron]="src/gateway/session_cron_commands.o lib/libjson/json.o"
  [sessworker]="src/gateway/session_worker_spawn.o lib/libjson/json.o"
  [localroute]="src/gateway/session_local_route.o lib/libjson/json.o"
  [sessop]="src/gateway/session_operator.o lib/libjson/json.o"
  [cmdsink]="src/gateway/session_command_sink.o lib/libjson/json.o"
  [apid]="src/gateway/platforms/api_server_response_identity.o lib/libjson/json.o"
  [kantool]="src/tools/kanban_toolset_context.o lib/libjson/json.o"
  [connuse]="src/tools/connectors/data_use.o lib/libjson/json.o"
  [procwsl]="src/tools/process_registry_wsl.o lib/libjson/json.o"
  [appquote]="src/tools/approval_quoted_newlines.o lib/libjson/json.o"
  [apppipe]="src/tools/approval_pipe_launchers.o lib/libjson/json.o"
  [sract]="src/session_rules/activity.o lib/libjson/json.o"
  [corehealth]="src/persistence/core/health.o lib/libjson/json.o"
  [receipts]="src/persistence/workers/receipts.o lib/libjson/json.o"
  [wguards]="src/persistence/sqlite/conversation/_store/write_guards.o lib/libjson/json.o"
  [slackcfg]="src/plugins/platforms/slack/adapter_config_readers.o lib/libjson/json.o"
  [gmeetjson]="src/plugins/google_meet/_jsonfile.o lib/libjson/json.o"
  [deadline]="src/agent/tool_executor_deadline.o lib/libjson/json.o"
  [cn_08669ae43f]="src/gateway/relay/descriptor.o lib/libjson/json.o"
  [cn_d06d1d1eb5]="src/gateway/session_contract.o lib/libjson/json.o"
  [cn_9a0d4bbb84]="src/hermes_agent/compat/moves.o lib/libjson/json.o"
  [cn_a1ee975a5f]="src/hermes_agent/compat/required_names.o lib/libjson/json.o"
  [cn_640c50c0d5]="src/hermes_cli/config_defaults_voice.o lib/libjson/json.o"
  [cn_44a08ba51b]="src/hermes_cli/gateway_connections_ssh_programs.o lib/libjson/json.o"
  [cn_d5adfd101d]="src/persistence/conversation/search.o lib/libjson/json.o"
  [cn_6978a463a1]="src/persistence/core/admin.o lib/libjson/json.o"
  [cn_43db546fa4]="src/persistence/core/artifacts.o lib/libjson/json.o"
  [cn_a9758b01cb]="src/persistence/housekeeping/maintenance.o lib/libjson/json.o"
  [cn_fac8f63e4d]="src/plugins/model-providers/actual/__init__.o lib/libjson/json.o"
  [cn_3b62e2f7a6]="src/plugins/platforms/bluebubbles/gateway_env.o lib/libjson/json.o"
  [cn_36c6acdf64]="src/plugins/platforms/dingtalk/gateway_env.o lib/libjson/json.o"
  [cn_2d1baa314e]="src/plugins/platforms/discord/adapter_hint.o lib/libjson/json.o"
  [cn_79c4321417]="src/plugins/platforms/discord/gateway_env.o lib/libjson/json.o"
  [cn_06e4137121]="src/plugins/platforms/email/gateway_env.o lib/libjson/json.o"
  [cn_79db0ebe9f]="src/plugins/platforms/feishu/adapter_hint.o lib/libjson/json.o"
  [cn_a7309cf7dc]="src/plugins/platforms/feishu/gateway_env.o lib/libjson/json.o"
  [cn_291bf91deb]="src/plugins/platforms/matrix/adapter_hint.o lib/libjson/json.o"
  [cn_21f12b8852]="src/plugins/platforms/mattermost/gateway_env.o lib/libjson/json.o"
  [cn_5f6deb5549]="src/plugins/platforms/qqbot/constants.o lib/libjson/json.o"
  [cn_eba4177a5b]="src/plugins/platforms/signal/gateway_env.o lib/libjson/json.o"
  [cn_610166e97e]="src/plugins/platforms/slack/adapter_hint.o lib/libjson/json.o"
  [cn_3745770082]="src/plugins/platforms/telegram/adapter_hint.o lib/libjson/json.o"
  [cn_23f115c7d5]="src/plugins/platforms/wecom/gateway_env.o lib/libjson/json.o"
  [cn_0760b3e14f]="src/plugins/platforms/weixin/gateway_env.o lib/libjson/json.o"
  [cn_0feb49f5fa]="src/plugins/platforms/whatsapp_cloud/gateway_env.o lib/libjson/json.o"
  [cn_a0b4567d4c]="src/plugins/teams_pipeline/graph_client.o lib/libjson/json.o"
  [cn_dc50a692e7]="src/tools/browser_supervisor_frames.o lib/libjson/json.o"
  [cn_14640a1ed1]="src/tools/connectors/operation.o lib/libjson/json.o"
  [tscope]="src/hermes_cli/toolset_scope.o lib/libjson/json.o"
  [mediapl]="src/gateway/platforms/base_media_flags.o lib/libjson/json.o"
  [keynotice]="src/hermes_cli/config_key_notices.o lib/libjson/json.o"
  [imagesniff]="src/gateway/platforms/base_image_sniff.o lib/libjson/json.o"
  [procwait]="src/tools/process_registry_wait.o lib/libjson/json.o"
  [modfinger]="src/hermes_cli/models_process_fingerprint.o lib/libjson/json.o"
  [tenv]="src/tui_gateway/_env.o lib/libjson/json.o"
  [ciss]="src/hermes_cli/config_issues.o lib/libjson/json.o"
  [botnotice]="src/gateway/bot_peer_notices.o lib/libjson/json.o"
  [locmig]="src/persistence/sqlite/conversation/_store/local_migration.o lib/libjson/json.o"
  [bedtier]="src/agent/bedrock_service_tiers.o lib/libjson/json.o"
  [shrprof]="src/hermes_cli/shared_profile_warning.o lib/libjson/json.o"
  [cronenv]="src/cron/env_settings.o lib/libjson/json.o"
  [stool]="src/gateway/session_tool_events.o lib/libjson/json.o"
  [colors]="src/hermes_cli/colors.o lib/libjson/json.o"
  [ssurf]="src/session_rules/surfaces.o lib/libjson/json.o"
  [clarbatch]="src/tools/clarify_batch.o lib/libjson/json.o"
  [plugcompat]="src/hermes_cli/plugin_compat.o lib/libjson/json.o"
  [rtcontract]="src/gateway/runtime_contract.o lib/libjson/json.o"
  [fastregion]="src/hermes_cli/models_fast_regions.o lib/libjson/json.o"
  [clipanel]="src/hermes_cli/cli_panel.o lib/libjson/json.o"
  [auxprof]="src/agent/auxiliary_profile_gates.o lib/libjson/json.o"
  [credtstamp]="src/agent/credential_pool_timestamps.o lib/libjson/json.o"
  [profcfg]="src/hermes_cli/profiles_config.o lib/libjson/json.o"
  [sanitizeid]="src/tools/environments/path_utils.o lib/libjson/json.o"
  [session_skin]="src/gateway/session_skin.o lib/libjson/json.o"
  [clibundles]="src/hermes_cli/subcommands/bundles.o lib/libjson/json.o"
  [clicheckpoints]="src/hermes_cli/subcommands/checkpoints.o lib/libjson/json.o"
  [clicompletion]="src/hermes_cli/subcommands/completion.o lib/libjson/json.o"
  [climoa]="src/hermes_cli/subcommands/moa.o lib/libjson/json.o"
  [conpayload]="src/tui_gateway/connector_payload.o lib/libjson/json.o"
  [clituiwrap]="src/hermes_cli/cli_tui_wrap.o lib/libjson/json.o"
  [hrti]="src/gateway/hosted_room_task_input.o lib/libjson/json.o"
  [canon]="src/persistence/core/canonical.o lib/libjson/json.o"
  [sesskeys]="src/persistence/core/keys.o lib/libjson/json.o"
  [botstate]="src/gateway/session_bot_state.o lib/libjson/json.o"
  [same_writer]="src/hermes_cli/active_sessions_gaps.o lib/libjson/json.o"
  [same_auth_store]="src/hermes_cli/auth_gaps.o lib/libjson/json.o"
  [abort_complete]="src/hermes_cli/update_abort_recovery_gaps.o lib/libjson/json.o"
  [compress_fast_lane]="src/agent/auxiliary_client_gaps.o lib/libjson/json.o"
  [mcp_filter]="src/hermes_cli/mcp_startup_gaps.o lib/libjson/json.o"
  [validate_page]="src/gateway/hosted_room_replicas_gaps.o lib/libjson/json.o"
  [worktree_pushed]="src/cli_gaps.o lib/libjson/json.o"
  [effective_ttl]="src/agent/prompt_caching_gaps.o lib/libjson/json.o"
  [prune_rooms]="src/gateway/hosted_rooms_gaps3.o lib/libjson/json.o lib/libdb/sqlite3.o"
  [gateway_windows_pure]="src/cli/gateway_windows_pure.o lib/libjson/json.o"
  [cron_jobs_rewrite]="src/cron/cron_jobs_rewrite.o lib/libjson/json.o"
  [kanban_adapter_flow]="FULL"
  [plugin_commands_gap]="FULL"
  [delegation_pins]="FULL"
  [hermes_home_defaults]="src/cli/hermes_home_defaults.o lib/libjson/json.o"
  [hermes_home_override]="src/cli/hermes_home_override.o lib/libjson/json.o"
  [hermes_time_env]="src/cli/hermes_time_ports.o lib/libjson/json.o"
  [systemd_stop_sec]="src/gateway/restart_gaps.o lib/libjson/json.o"
  [managed_local_endpoint]="src/agent/auxiliary_client_gaps.o lib/libjson/json.o"
  [audit_bounded]="src/cli/port_audit.o lib/libjson/json.o"
  [callbacks_skip]="src/cli/port_callbacks.o lib/libjson/json.o"
  # backfilled: harness/object name differs from the oracle discovery name.
  [gateway_whatsapp_identity]="src/hermes_cli/whatsapp_identity.o lib/libjson/json.o"
  [session_server_requests]="src/cli/port_session_server_requests.o lib/libjson/json.o"
  [side_question_request]="src/agent/port_side_question_request.o lib/libjson/json.o"
  [gateway_profile_routing]="src/gateway/port_gateway_profile_routing.o lib/libjson/json.o"
  [session_activity]="src/agent/port_session_activity.o lib/libjson/json.o"
  [turn_liveness]="src/agent/port_turn_liveness.o lib/libjson/json.o"
  [hermes_auth]="src/hermes_cli/port_hermes_auth.o lib/libjson/json.o"
  [auth_error_copy]="src/hermes_cli/port_auth_error_copy.o lib/libjson/json.o"
  [partial_compress_keep]="FULL"
  [input_sanitize]="src/hermes_cli/port_input_sanitize.o lib/libjson/json.o"
  [memory_status]="src/gateway/port_memory_status.o lib/libjson/json.o"
  [repetition_guard]="src/agent/port_repetition_guard.o lib/libjson/json.o"
  [url_safety_helpers]="src/tools/port_url_safety_helpers.o lib/libjson/json.o"
  [gateway_platform_activation]="src/gateway/port_gateway_platform_activation.o lib/libjson/json.o"
  [anthropic_credentials]="FULL"
  [agent_oneshot]="FULL"
  [ccd]="FULL"
  [cronjob_prompt_scan]="FULL"
  [image_token_cost_lic]="src/agent/port_image_token_cost.o src/cli/port_anthropic_credentials.o lib/libjson/json.o"
  [relay_media]="src/gateway/relay/port_media_labels.o src/cli/port_aux_outcome_event.o lib/libcrypto/crypto.o lib/libjson/json.o"
  [inventory]="src/hermes_cli/port_inventory.o lib/libjson/json.o"
  [browser_connect]="src/hermes_cli/port_browser_connect.o lib/libjson/json.o"
  [banner]="src/hermes_cli/port_banner.o lib/libjson/json.o"
  [nous_account]="src/hermes_cli/port_nous_account.o lib/libjson/json.o"
  [nous_subscription]="src/hermes_cli/port_nous_subscription.o lib/libjson/json.o"
  [secrets_cli]="src/hermes_cli/port_secrets_cli.o lib/libjson/json.o"
  [dashboard_register]="src/hermes_cli/port_dashboard_register.o lib/libjson/json.o"
  [codex_models]="src/hermes_cli/port_codex_models.o lib/libjson/json.o"
  [dingtalk_auth]="src/hermes_cli/port_dingtalk_auth.o lib/libjson/json.o"
  [pt_input_extras]="src/hermes_cli/port_pt_input_extras.o lib/libjson/json.o"
  [telegram_managed_bot]="src/hermes_cli/port_telegram_managed_bot.o lib/libjson/json.o"
  [clipboard]="src/hermes_cli/port_clipboard.o lib/libjson/json.o"
  [container_boot]="src/hermes_cli/port_container_boot.o lib/libjson/json.o"
  [relaunch]="src/hermes_cli/port_relaunch.o lib/libjson/json.o"
  [profile_describer]="src/cli/port_profile_describer.o lib/libjson/json.o"
  [slack_cli]="src/cli/port_slack_cli.o lib/libjson/json.o"
  [send_cmd]="src/cli/port_send_cmd.o lib/libjson/json.o"
  [portal_cli]="src/cli/port_portal_cli.o lib/libjson/json.o"
  [skill_commands]="src/agent/port_skill_commands.o lib/libjson/json.o"
  [skill_bundles]="src/agent/port_skill_bundles.o src/agent/port_skill_commands.o lib/libjson/json.o lib/libhash/hash.o"
  [shared_metrics_contract]="src/hermes_cli/observability/port_shared_metrics_contract.o lib/libjson/json.o"
  [chat_completion_helpers]="src/agent/port_chat_completion_helpers.o lib/libjson/json.o lib/libregex/hermes_regex.o"
  [hosted_rooms_common]="src/gateway/port_hosted_rooms_common.o lib/libjson/json.o lib/libregex/hermes_regex.o"
  [hosted_rooms]="src/cli/port_hosted_rooms.o src/gateway/port_hosted_rooms_common.o lib/libjson/json.o lib/libregex/hermes_regex.o lib/libhash/hash.o"
  [prompt_builder]="src/agent/port_prompt_builder.o lib/libjson/json.o"
  [gemini_native_adapter]="src/agent/port_gemini_native_adapter.o lib/libjson/json.o"
  [terminal_tool]="src/tools/port_terminal_tool.o lib/libjson/json.o"
  [early_recovery]="src/hermes_cli/port_early_recovery.o lib/libjson/json.o"
  [kanban_db_dispatch]="src/hermes_cli/port_kanban_db_dispatch.o lib/libjson/json.o"
  [local_models]="src/hermes_cli/web_routers/port_local_models.o lib/libjson/json.o"
  [browser_launcher]="src/tools/port_browser_launcher.o lib/libjson/json.o"
  [microsoft_graph_auth]="src/plugins/teams_pipeline/microsoft_graph_auth.o src/agent/logger.o lib/libjson/json.o"
  [methods_session]="src/cli/port_methods_session.o lib/libjson/json.o"
  [methods_tools]="src/tui_gateway/port_methods_tools.o lib/libjson/json.o"
  [server]="src/tui_gateway/port_server.o lib/libjson/json.o"
  [update_windows]="src/cli/port_update_windows.o lib/libjson/json.o"
  [gateway_status]="src/gateway/port_gateway_status.o lib/libjson/json.o lib/libregex/hermes_regex.o .testbin/hash_O0.o"
  [update_cmd]="src/hermes_cli/port_update_cmd.o lib/libjson/json.o"
  [update_receipt]="src/hermes_cli/port_update_receipt.o lib/libjson/json.o"
  [state_repair]="src/persistence/sqlite/_state/port_state_repair.o lib/libjson/json.o"
  [worktree_ops]="src/hermes_cli/port_worktree_ops.o lib/libjson/json.o"
  [approval_transport]="src/cli/port_approval_transport.o lib/libjson/json.o"
  [idle_watch]="src/cli/port_idle_watch.o lib/libjson/json.o"
  [image_eviction_policy]="src/agent/port_image_eviction_policy.o"
  [compaction_events]="src/agent/port_compaction_events.o lib/libjson/json.o"
  [web_common]="src/plugins/web/port_web_common.o lib/libjson/json.o"
  [model_picker_usage]="src/hermes_cli/port_model_picker_usage.o src/hermes_cli/port_models_pricing.o lib/libjson/json.o"
  [anon_auth]="src/hermes_cli/port_anon_auth.o lib/libjson/json.o"
  [credits_free_tier]="src/agent/credits_tracker.o src/hermes_cli/models_gaps.o src/hermes_cli/models_helpers.o src/hermes_cli/port_anon_auth.o lib/libjson/json.o .testbin/hash_O0.o lib/libcredentialfiles/credential_files.o lib/libcrypto/crypto.o lib/libhttp/http.o src/agent/logger.o src/hermes_cli/auth_helpers.o src/hermes_cli/auth_store.o src/hermes_cli/config_py_io.o src/hermes_cli/config_py_pure.o src/hermes_cli/fs_replace.o src/hermes_cli/model_catalog.o src/hermes_cli/model_normalize.o src/hermes_cli/models_net.o src/hermes_cli/models_pure_impl.o src/hermes_cli/provider_meta.o lib/libhive/hive.o lib/libyaml/yaml.o src/agent/credential_pool_persistence.o src/cli/paths.o src/cron/jobs.o src/hermes_cli/auth.o src/hermes_cli/config.o src/hermes_cli/config_pure.o src/hermes_cli/models.o src/hermes_cli/personality.o src/hermes_cli/provider_registry.o lib/libcron/cron.o lib/libdatetime/datetime.o lib/libuuid/uuid.o src/agent/auxiliary_client_helpers.o src/agent/credential_persistence.o src/cron/cron_extras.o src/cron/cron_sqlite.o src/cron/lifecycle_guard.o src/cron/scheduler_runtime.o src/hermes_cli/kanban_db.o lib/libdb/sqlite3.o src/agent/auxiliary_client.o src/agent/context.o src/agent/llm_client.o src/agent/provider.o src/agent/provider_metadata.o src/cli/config.o src/cron/scheduler_delivery.o src/gateway/lifecycle_ledger.o src/gateway/memory_status.o src/hermes_cli/kanban_boards.o src/hermes_cli/kanban_runs.o src/hermes_cli/kanban_schema.o src/hermes_cli/kanban_tasks.o src/hermes_cli/kanban_workers.o src/hermes_cli/lifecycle.o src/hermes_cli/profiles.o src/hermes_cli/session_title.o src/infrastructure_stubs.o"
  [tools_process_registry]="src/tools/process_registry_gaps.o src/tools/process_registry.o lib/libjson/json.o lib/libansi/ansi_strip.o src/agent/browser_redact.o src/agent/logger.o src/gateway/status.o src/hermes_cli/config_py_io.o .testbin/hash_O0.o lib/libyaml/yaml.o src/cli/paths.o src/cron/jobs.o src/hermes_cli/banner_pure.o src/hermes_cli/config.o src/hermes_cli/config_py_pure.o src/hermes_cli/fs_replace.o lib/libcron/cron.o lib/libdatetime/datetime.o lib/libuuid/uuid.o src/agent/auxiliary_client_helpers.o src/cron/cron_extras.o src/cron/cron_sqlite.o src/cron/lifecycle_guard.o src/cron/scheduler_runtime.o src/hermes_cli/config_defaults.o src/hermes_cli/config_ports.o src/hermes_cli/config_pure.o src/hermes_cli/dashboard_procs.o src/hermes_cli/personality.o src/hermes_cli/port_config.o src/hermes_cli/port_config_nested.o src/hermes_cli/web_server_paths.o src/tools/skills_tool.o lib/libproc/proc.o src/agent/auxiliary_client.o src/agent/context.o src/agent/llm_client.o src/agent/provider.o src/agent/provider_metadata.o src/cli/config.o src/cron/scheduler_delivery.o src/hermes_cli/profiles.o src/hermes_cli/session_title.o lib/libdb/db.o lib/liberrorclassifier/error_classifier.o lib/libhttp/http.o src/agent/credential_pool_engine.o src/agent/models_dev.o src/agent/port_agent_secret_scope.o src/agent/prompt_caching.o src/agent/provider_profiles_builtin.o src/agent/relay_llm.o src/cli/config_env_file.o src/cron/scheduler_helpers.o src/hermes_cli/config_profile.o src/hermes_cli/config_setup.o src/plugins/platforms/bluebubbles/redact.o src/provider/token_exchange.o src/tools/url_safety.o lib/libcrypto/crypto.o lib/libcurses_widget/curses_widget.o lib/libproviderauth/provider_auth.o src/agent/portal_tags.o src/agent/provider_profile.o src/agent/relay_runtime.o src/hermes_cli/config_io.o src/hermes_cli/config_platforms.o src/hermes_cli/models_pure_impl.o lib/libomap/omap.o lib/libtoml/toml.o src/hermes_cli/config_schema.o src/hermes_cli/models_net.o src/hermes_cli/relay_plugin_cutover.o src/hermes_cli/model_catalog.o src/hermes_cli/models.o src/hermes_cli/models_helpers.o lib/libcredentialfiles/credential_files.o src/agent/provider_anthropic.o src/agent/provider_azure.o src/agent/provider_bedrock.o src/agent/provider_codex_responses.o src/agent/provider_custom.o src/agent/provider_deepseek.o src/agent/provider_google.o src/agent/provider_openai.o src/agent/provider_openrouter.o src/agent/provider_xai.o src/hermes_cli/model_normalize.o lib/libbase64/base64.o lib/libhive/hive.o src/agent/api_error_summary.o src/agent/codex_responses_adapter.o src/agent/provider_profile_apply.o src/agent/transports/transport_common.o src/infrastructure_stubs.o"
  [config_pure]="FULL"
  [state_errors]="lib/libjson/json.o"
  [state_guard]="lib/libjson/json.o"
  [jobs_pure]="lib/libjson/json.o"
  [display]="src/agent/display_ports.o src/agent/port_display.o lib/libjson/json.o lib/libansi/ansi.o"
  [version_info]="src/hermes_cli/port_version_info.o"
  [shared_metrics_norm]="src/hermes_cli/shared_metrics_contract.o src/hermes_cli/observability/port_shared_metrics_contract.o src/hermes_cli/observability/smc_event_extractors.o lib/libjson/json.o"
  [update_pause_record]="src/hermes_cli/port_update_pause_record.o .testbin/hash_O0.o lib/libjson/json.o"
  [left_core_migration]="src/hermes_cli/port_left_core_migration.o src/hermes_cli/provider_registry.o tests/stubs/lcm_deps_stub.o lib/libyaml/yaml.o lib/libjson/json.o"
  [pm_environments]="src/pm/port_pm_environments.o .testbin/hash_O0.o lib/libjson/json.o"
  [model_switch]="src/hermes_cli/port_model_switch.o lib/libjson/json.o"
  [gateway_windows]="src/hermes_cli/windows.o src/cli/gateway_windows_pure.o lib/libregex/hermes_regex.o lib/libjson/json.o"
  [conversation_compression]="FULL"
  [model_metadata]="src/agent/port_model_metadata.o lib/libjson/json.o"
  [cch_ports]="src/agent/chat_completion_helpers_pure.o src/agent/reasoning_timeouts.o lib/libjson/json.o"
  [hermes_state_keys]="src/persistence/sqlite/_state/port_hermes_state_keys.o .testbin/hash_O0.o lib/libjson/json.o"
  [kanban_schemas]="src/tools/port_kanban_schemas.o lib/libjson/json.o"
  [rfb_filter]="src/cli/port_rfb_filter.o lib/libjson/json.o"
  [session_managed_worker_log]="src/cli/port_session_managed_worker_log.o lib/libjson/json.o"
  [hosted_room_driver]="FULL"
  [service_manager]="src/hermes_cli/port_service_manager.o lib/libjson/json.o"
  [cli_config_load]="src/cli/port_cli_config_load.o src/cli/port_fast_mode.o lib/libjson/json.o"
  [scheduler_tick]="src/cli/port_scheduler_tick.o lib/libjson/json.o"
  [hermes_cli_misc]="src/cli/port_hermes_cli_misc.o lib/libjson/json.o"
  [gateway_wake]="src/gateway/port_gateway_wake.o lib/libjson/json.o"
  [hosted_room_execution_policy]="src/gateway/port_hosted_room_execution_policy.o lib/libcrypto/crypto.o lib/libjson/json.o"
  [vault_login_classifier]="src/agent/port_vault_login_classifier.o src/cli/vault_login_tokens.o lib/libjson/json.o"
  [api_store_error]="src/cli/port_api_store_error.o"
  [parked_notice]="src/cli/port_parked_notice.o lib/libjson/json.o"
  [producer_lifecycle]="src/cli/port_producer_lifecycle.o lib/libjson/json.o"
  [fast_mode]="src/cli/port_fast_mode.o"
  [bot_loop_guard]="src/cli/port_bot_loop_guard.o lib/libjson/json.o"
  [media_labels]="FULL"
  [error_surface]="src/agent/port_error_surface.o lib/libjson/json.o"
  [acp_client]="src/agent/port_acp_client.o lib/libjson/json.o"
  [anthropic_message_convert]="src/agent/port_anthropic_message_convert.o lib/libjson/json.o"
  [micro_env]="src/cli/port_micro_env.o lib/libjson/json.o"
  [image_gen_path]="lib/libjson/json.o"
  [xai_expires]="FULL"
  [web_base64_img]="FULL"
  [image_token_cost]="FULL"
  [comp_telemetry]="src/cli/port_comp_telemetry.o lib/libjson/json.o"
  [audit]="src/cli/port_audit.o lib/libjson/json.o"
  [access_policy]="src/cli/port_access_policy.o lib/libjson/json.o"
  [aux_task_builder]="src/cli/port_aux_task_builder.o lib/libjson/json.o"
  [auth_providers]="src/cli/port_auth_providers.o lib/libjson/json.o"
  [blueprint_plugins]="src/cli/port_blueprint_plugins.o lib/libjson/json.o"
  [callbacks]="src/cli/port_callbacks.o lib/libjson/json.o"
  [aux_outcome_event]="src/cli/port_aux_outcome_event.o lib/libjson/json.o"
  [cli_process_dock]="FULL"
)

fail=0
count=0

# --changed / ORACLE_ONLY=changed must be read BEFORE names=("$@"), otherwise
# the flag itself lands in the port list and is treated as an oracle name.
WANT_CHANGED=0
for a in "$@"; do
  [ "$a" = "--changed" ] && WANT_CHANGED=1
done
names=()
for a in "$@"; do
  [ "$a" = "--changed" ] && continue
  names+=("$a")
done

# If no subset given, discover all committed oracles.
# fuzzy_match_helpers is EXCLUDED: its harness consumes its OWN stdin (op-value
# lines), so the no-stdin pipe here can't drive it — the standalone
# tests/gate_fuzzy_match_helpers.sh is the authority for that port.
SUITE_SKIP="fuzzy_match_helpers file_type github_provider"
# File-arg harnesses: these open their own fixture instead of reading the
# stdin cases the runner pipes, so the suite cannot drive them. They are
# excluded here rather than reported as false failures.
SUITE_SKIP="$SUITE_SKIP approval_transport input_sanitize memory_status repetition_guard turn_summary url_safety_helpers"
# drift_allowlist is NOT a differential oracle: it is the drift gate's
# allowlist DATA module (generated by tests/verify_oracle_drift.py). Discovery
# matches its oracle_*.py name, but it has no harness and no cases.
SUITE_SKIP="$SUITE_SKIP drift_allowlist"
if [ ${#names[@]} -eq 0 ]; then
  RUNNING_FULL=1
  for o in tests/oracle_*.py; do
    n="${o#tests/oracle_}"; n="${n%.py}"
    case " $SUITE_SKIP " in *" $n "*) continue;; esac
    names+=("$n")
  done
fi

# --changed / ORACLE_ONLY=changed: gate only the ports the current work touches.
#
# The full 95-oracle gate costs ~450s and is I/O/spawn-bound (measured: 4-way
# 466s, 8-way 444s, sequential 43s for 7 oracles), so parallelism does not help.
# A commit usually touches one or two ports, so gating just those is the
# difference between a 10s check and an 8-minute one.
#
# Maps changed paths back to oracle names: src/**/port_<name>.c and
# include/port_<name>.h both mean oracle <name>. Falls back to the full set
# when nothing maps (a change to a shared header, a lib, or the build files can
# affect any port) - say so rather than silently passing.
if [ "${1:-}" = "--changed" ] || [ "${ORACLE_ONLY:-}" = "changed" ]; then
  shift 2>/dev/null || true
  # git is NOT on the login shell's PATH (it is /mingw64/bin/git here), so
  # resolve it explicitly and fall back to the full gate rather than silently
  # reporting "nothing changed".
  GIT="$(command -v git 2>/dev/null || true)"
  if [ -z "$GIT" ]; then
    for c in /mingw64/bin/git /mingw64/bin/git.exe /usr/bin/git /usr/bin/git.exe \
             "/c/Program Files/Git/cmd/git.exe" "/c/Program Files/Git/bin/git.exe" \
             "/c/Users/eman5/AppData/Local/hermes/tools/git-2.53.0-win32-x64/mingw64/bin/git.exe" \
             "$(cygpath -u "$USERPROFILE" 2>/dev/null)/AppData/Local/hermes/tools/git-2.53.0-win32-x64/mingw64/bin/git.exe"; do
      [ -x "$c" ] && { GIT="$c"; break; }
    done
  fi
  if [ -n "$GIT" ]; then
    changed=$(
      { "$GIT" diff --name-only HEAD 2>/dev/null
        "$GIT" diff --name-only --staged 2>/dev/null
      } | sort -u
    )
  else
    echo "could not locate git; running the FULL gate" >&2
    changed=""
  fi
  mapped=()
  while IFS= read -r f; do
    case "$f" in
      */port_*.c|port_*.c)
        n="$(basename "$f" | sed -E 's/^port_//; s/\.c$//')"
        [ -f "tests/oracle_$n.py" ] && mapped+=("$n")
        ;;
      include/port_*.h)
        n="$(basename "$f" | sed -E 's/^port_//; s/\.h$//')"
        [ -f "tests/oracle_$n.py" ] && mapped+=("$n")
        ;;
      *.c)
        # Any other changed .c (e.g. *_gaps.c, port_*.c with a prefixed name):
        # find oracles whose DEPS link its object. Covers the in-place re-port
        # pattern where the oracle name differs from the C file basename.
        obj="${f%.c}.o"
        for cand in "${!DEPS[@]}"; do
          case "${DEPS[$cand]}" in
            *"$obj"*) [ -f "tests/oracle_$cand.py" ] && mapped+=("$cand") ;;
          esac
        done
        for cand in "${!HARNESS_ALIAS[@]}"; do
          case "$f" in *"${HARNESS_ALIAS[$cand]}"*) [ -f "tests/oracle_$cand.py" ] && mapped+=("$cand") ;; esac
        done
        ;;
    esac
  done <<< "$changed"
  if [ ${#mapped[@]} -gt 0 ]; then
    echo "changed ports: ${mapped[*]}"
    names=("${mapped[@]}")
  else
    echo "no port files changed; running the FULL gate" >&2
  fi
fi

# Resolve each name to "<name>\t<harness>\t<deps>" in the PARENT shell (bash
# cannot export the HARNESS_ALIAS / DEPS associative arrays to the subshells
# xargs spawns), then fan the lines out to tests/_oracle_one.sh workers.
# The borrowed third-party lib dir is needed to LINK each harness, and it must
# travel as an ARGUMENT rather than an environment variable: the ccache wrapper
# strips LIBRARY_PATH from the compiler it execs, and this box's bash drops
# exported vars in nested children. build/winlibs-env.sh sets it when the
# caller sourced it; otherwise fall back to the repo-local default.
if [ -z "${ORACLE_LIBDIR:-}" ]; then
  if [ -f build/winlibs-env.sh ]; then
    # shellcheck disable=SC1091
    . ./build/winlibs-env.sh >/dev/null 2>&1 || true
  fi
fi
if [ -z "${ORACLE_LIBDIR:-}" ]; then
  for c in /c/winlibs/extra-libs "C:/winlibs/extra-libs"; do
    [ -d "$c" ] && { ORACLE_LIBDIR="$c"; break; }
  done
fi

resolve_one() {
  local name="$1"
  local harness="${HARNESS_ALIAS[$name]:-$name}"
  local deps="src/cli/port_$name.o lib/libjson/json.o"
  # Flow harnesses #include their port .c directly (self-contained). Defaulting
  # them to src/cli/port_<name>.o points at a file that does not exist and
  # breaks the prebuild with "cannot find ... No such file". Explicit DEPS
  # entries below still win for harnesses that need extra objects.
  if grep -qE '^#include "[A-Za-z0-9_/]+\.c"' "tests/t_port_${harness}.c" 2>/dev/null; then
    deps="lib/libjson/json.o"
  elif [ ! -f "src/cli/port_$name.c" ]; then
    # Linked (not included) harness: derive its object from the first header
    # whose basename matches an object registered in build/objects.mk.
    for hdr in $(grep -oE '^#include "[A-Za-z0-9_/]+\.h"' "tests/t_port_${harness}.c" 2>/dev/null \
                 | sed -e 's/^#include "//' -e 's/"$//'); do
      case "$hdr" in
        hermes_json.h|hermes_core_types.h|hermes_logger.h|hermes_win32.h|libjson/json.h|sqlite3.h) continue ;;
      esac
      base=$(basename "$hdr" .h)
      cand=$(grep -hoE "[A-Za-z0-9_/]*${base}\.o" build/objects.mk 2>/dev/null | sort -u | head -1)
      if [ -n "$cand" ]; then deps="$cand lib/libjson/json.o"; break; fi
    done
  fi
  if grep -q 'hermes_regex.h' "tests/t_port_${harness}.c" 2>/dev/null \
     || grep -q 'hermes_regex.h' "src/cli/port_$name.c" 2>/dev/null \
     || grep -q 'hermes_regex.h' "include/port_$name.h" 2>/dev/null; then
    deps="$deps lib/libregex/hermes_regex.o"
  fi
  local varname="ORACLE_OBJS_$(echo "$name" | tr '[:lower:]' '[:upper:]' | tr '/' '_')"
  if [ -n "${!varname:-}" ]; then deps="${!varname}"; fi
  if [ -n "${DEPS[$name]:-}" ]; then deps="${DEPS[$name]}"; fi
  printf '%s\t%s\t%s\t%s\n' "$name" "$harness" "$deps" "${ORACLE_LIBDIR:-}"
}

# --- prebuilt harness cache ------------------------------------------------------
# run_one_oracle.sh reuses .testbin/harnesses/<harness>.exe when that binary is
# newer than every object and harness source. Warm the cache in one make -j
# batch when it has gone stale, but only for a FULL gate: a subset run would
# pay the whole build to save a couple of 2s compiles, so it lets the per-oracle
# check rebuild just what it needs.
STAMP=".testbin/harnesses/.stamp"
cache_stale=0
if [ ! -f "$STAMP" ]; then
  cache_stale=1
elif [ -n "$(find src lib -name '*.o' -newer "$STAMP" -print -quit 2>/dev/null)" ]; then
  cache_stale=1
elif [ -n "$(find tests -name 't_port_*.c' -newer "$STAMP" -print -quit 2>/dev/null)" ]; then
  cache_stale=1
fi
if [ "$cache_stale" = "1" ] && [ "$RUNNING_FULL" = "1" ]; then
  SPECS_FILE=".testbin/harness_specs.$$.tsv"
  : > "$SPECS_FILE"
  for name in "${names[@]}"; do
    resolve_one "$name" | cut -f1-3 >> "$SPECS_FILE"
  done
  echo "prebuilt harness cache stale; rebuilding $(wc -l < "$SPECS_FILE") specs..." >&2
  bash tests/build_all_harnesses.sh "$SPECS_FILE" >&2
  rm -f "$SPECS_FILE"
fi
# A full gate has now either confirmed the cache or rebuilt it wholesale, so
# the per-oracle workers can skip their own freshness finds.
if [ "$RUNNING_FULL" = "1" ]; then
  ORACLE_PREBUILT_OK=1
  export ORACLE_PREBUILT_OK
fi

# Parallelism: ORACLE_JOBS (default 4). ORACLE_JOBS=1 gives the sequential
# behaviour. 12-way died with MSYS2 fork exhaustion ("Resource temporarily
# unavailable" / 0xC0000142) - but that turned out to be CAUSED by the shared
# .testbin/tt_$NAME / oracle_tmp_$NAME paths, not by the parallelism itself.
# With those made per-invocation ($$), 4-way is clean. Workers touch only their
# own compile, scratch dir and log, so ccache stays safe.
JOBS="${ORACLE_JOBS:-4}"

if [ "$JOBS" -le 1 ]; then
  # Sequential: capture the same PASS/MISS/FAIL lines the parallel branch
  # parses, so the summary counters exist on both paths. Previously this
  # branch only set `fail`, leaving fail_n/miss_n unbound under `set -u`.
  results=$(
    for name in "${names[@]}"; do
      bash tests/_oracle_one.sh "$(resolve_one "$name")" || true
    done
  )
  echo "$results"
  pass_n=$(printf '%s\n' "$results" | grep -c '^PASS ')
  miss_n=$(printf '%s\n' "$results" | grep -c '^MISS ')
  fail_n=$(printf '%s\n' "$results" | grep -c '^FAIL ')
  count=$((pass_n + fail_n + miss_n))
  if [ "$fail_n" -gt 0 ] || [ "$miss_n" -gt 0 ]; then fail=1; fi
else
  results=$(
    for name in "${names[@]}"; do resolve_one "$name"; done |
      xargs -P "$JOBS" -d '\n' -I {} bash tests/_oracle_one.sh {}
  )
  echo "$results"
  pass_n=$(printf '%s\n' "$results" | grep -c '^PASS ')
  miss_n=$(printf '%s\n' "$results" | grep -c '^MISS ')
  fail_n=$(printf '%s\n' "$results" | grep -c '^FAIL ')
  count=$((pass_n + fail_n + miss_n))
  if [ "$fail_n" -gt 0 ] || [ "$miss_n" -gt 0 ]; then fail=1; fi
fi

# Batch-built harness binaries can be refused by Smart App Control on this
# host (rc=127, no ORACLE line). Retry every FAIL once with a forced relink;
# individual relinks have consistently cleared these.
# Clear stale retry logs on EVERY run: a green suite must not report retries
# from an earlier run whose summary artifact was left behind.
rm -f .testbin/retried_gates.*.txt
if [ "${fail_n:-0}" -gt 0 ]; then
  fixed=""
  RETRY_LOG=".testbin/retried_gates.$$.txt"
  : > "$RETRY_LOG"
  while read -r line; do
    case "$line" in
      FAIL\ *)
        n=${line#FAIL }; n=${n%% *}
        printf '%s\n' "$n" >> "$RETRY_LOG"
        retry=$(ORACLE_REBUILD=1 bash tests/_oracle_one.sh "$(resolve_one "$n")" 2>/dev/null || true)
        case "$retry" in
          PASS\ *) fixed="$fixed$retry"$'\n' ;;
          *) fixed="$fixed$line"$'\n' ;;
        esac
        ;;
      *) fixed="$fixed$line"$'\n' ;;
    esac
  done <<< "$results"
  results=${fixed%$'\n'}
  echo "$results"
  pass_n=$(printf '%s\n' "$results" | grep -c '^PASS ')
  miss_n=$(printf '%s\n' "$results" | grep -c '^MISS ')
  fail_n=$(printf '%s\n' "$results" | grep -c '^FAIL ')
  count=$((pass_n + fail_n + miss_n))
  if [ "$fail_n" -gt 0 ] || [ "$miss_n" -gt 0 ]; then fail=1; else fail=0; fi
fi

echo "----"
if [ -s .testbin/retried_gates.*.txt ]; then
  echo "retried gates (infrastructure defects, not clean passes):"
  cat .testbin/retried_gates.*.txt | sort -u | sed 's/^/  /'
fi
if [ "${fail:-0}" -eq 0 ]; then
  echo "oracles run: $count  (exit 0 = all green, no drift)"
else
  echo "oracles run: $count  (NOT all green: $fail_n failed, $miss_n missing)"
fi
exit "$fail"
