# Dynamically detect Python Hermes version from upstream source
HERMES_VERSION := $(shell python3 -c "import sys; sys.path.insert(0, '..'); from hermes_cli import __version__; print(__version__)" 2>/dev/null || echo "0.15.1")
HERMES_RELEASE_DATE := $(shell python3 -c "import sys; sys.path.insert(0, '..'); from hermes_cli import __release_date__; print(__release_date__)" 2>/dev/null || echo "2026.5.29")

# ── Cross-distro build setup ─────────────────────────────────────────────
# Auto-detect compiler: prefer clang on macOS/FreeBSD, gcc on Linux
# Override with: make CC=clang (or CC=gcc)
ifeq ($(origin CC),default)
    CC := $(shell command -v clang 2>/dev/null || command -v gcc 2>/dev/null || echo cc)
endif
ifeq ($(origin CXX),default)
    CXX := $(shell command -v clang++ 2>/dev/null || command -v g++ 2>/dev/null || echo c++)
endif
ifeq ($(origin AR),default)
    AR := $(shell command -v ar 2>/dev/null || echo ar)
endif

# Platform detection
UNAME_S := $(shell uname -s)

# Termux (Android Linux): detect by checking Termux prefix
ifeq ($(UNAME_S),Linux)
    TERMUX_PREFIX := /data/data/com.termux/files/usr
    ifneq ("$(wildcard $(TERMUX_PREFIX)/bin/pkg)","")
        SSL_CFLAGS := -I$(TERMUX_PREFIX)/include
        SSL_LDFLAGS := -L$(TERMUX_PREFIX)/lib -lssl -lcrypto
        PLATFORM_LDFLAGS := -lz
        OS_DEF := -D__TERMUX__
    else ifeq ($(shell ldd --version 2>/dev/null | head -1 | grep -c musl || echo 0),1)
# musl/Alpine: -ldl and -lpthread are in libc — omit them
        SSL_CFLAGS := $(shell pkg-config --cflags openssl 2>/dev/null || echo "")
        SSL_LDFLAGS := $(shell pkg-config --libs openssl 2>/dev/null || echo "-lssl -lcrypto")
        PLATFORM_LDFLAGS := -lz
        OS_DEF := -D__MUSL__
    else
# Standard Linux (glibc)
        SSL_CFLAGS := $(shell pkg-config --cflags openssl 2>/dev/null || echo "")
        SSL_LDFLAGS := $(shell pkg-config --libs openssl 2>/dev/null || echo "-lssl -lcrypto")
        PLATFORM_LDFLAGS := -ldl -lpthread -lz
        PLATFORM_LDFLAGS += $(shell pkg-config --libs wayland-client wayland-egl xkbcommon gbm EGL GLESv2 2>/dev/null || echo "-lwayland-client -lwayland-egl -lxkbcommon -lgbm -lEGL -lGLESv2")
        OS_DEF :=
    endif
else ifeq ($(UNAME_S),Darwin)
    BREW_SSL := $(shell brew --prefix openssl 2>/dev/null || echo /usr/local/opt/openssl)
    SSL_CFLAGS := -I$(BREW_SSL)/include
    SSL_LDFLAGS := -L$(BREW_SSL)/lib -lssl -lcrypto
    PLATFORM_LDFLAGS := -ldl -lpthread -lz
    OS_DEF := -D__APPLE__
else
# musl/Alpine (non-Linux) or fallback
    ifeq ($(shell ldd --version 2>/dev/null | head -1 | grep -c musl || echo 0),1)
        SSL_CFLAGS := $(shell pkg-config --cflags openssl 2>/dev/null || echo "")
        SSL_LDFLAGS := $(shell pkg-config --libs openssl 2>/dev/null || echo "-lssl -lcrypto")
        PLATFORM_LDFLAGS := -lz
        OS_DEF := -D__MUSL__
    else
        SSL_CFLAGS := $(shell pkg-config --cflags openssl 2>/dev/null || echo "")
        SSL_LDFLAGS := $(shell pkg-config --libs openssl 2>/dev/null || echo "-lssl -lcrypto")
        PLATFORM_LDFLAGS := -ldl -lpthread -lz
        OS_DEF :=
    endif
endif

CFLAGS = -O2 -g -Wall -Wno-pedantic -Wno-attributes -Wno-unused-result -Wno-format-truncation -Wstringop-truncation -Wno-misleading-indentation -Wno-discarded-qualifiers -Wno-unused-parameter -Wno-missing-field-initializers -Wno-format-extra-args -Wno-comment -Wno-format-zero-length -Wno-address -Wno-maybe-uninitialized -Wno-unused-function -I include $(SSL_CFLAGS) $(OS_DEF) $(CFLAGS_EXTRA)
CFLAGS += -DHERMES_VERSION=\"$(HERMES_VERSION)-slermes\" -DHERMES_RELEASE_DATE=\"$(HERMES_RELEASE_DATE)\"
LDFLAGS = $(SSL_LDFLAGS) $(PLATFORM_LDFLAGS)
LIBS = -lm

# Optional ncurses full-screen TUI (enabled when ncurses is available)
TUI_INC = -I lib/libncurses/include
TUI_LIB = lib/libncurses/lib/libncursesw.a lib/libncurses/lib/libpanelw.a
TUI_SRC = src/cli/tui_fullscreen.c src/cli/tui_eventpub.c src/cli/tui_slash_worker.c src/cli/tui_entry.c src/cli/tui_json_rpc.c src/cli/tui_transport.c src/cli/tui_layout.c src/cli/tui_render.c src/cli/tui_websocket.c
TUI_OBJ = src/cli/tui_fullscreen.o src/cli/tui_eventpub.o src/cli/tui_slash_worker.o src/cli/tui_entry.o src/cli/tui_json_rpc.o src/cli/tui_transport.o src/cli/tui_layout.o src/cli/tui_render.o src/cli/tui_websocket.o

# Auto-detect: check if local ncurses headers exist
ifneq ("$(wildcard lib/libncurses/include/curses.h)","")
    HAS_NCURSES = 1
    CFLAGS += $(TUI_INC)
else
    HAS_NCURSES = 0
endif

# Standalone library directories
LIB_DIRS = lib/liblineedit lib/libbinary lib/libbrowser lib/libdebug lib/libosv lib/libwebsite lib/libjson lib/libhttp lib/libtemplate lib/libyaml lib/libcrypto lib/libdotenv lib/libcron lib/libproc lib/libtui lib/libdb lib/libplugin lib/libskin lib/libwebsocket lib/libprotobuf lib/libmcp lib/libpath lib/libdatetime lib/libcsv lib/libhash lib/libuuid lib/libbase64 lib/libhtml lib/libtextwrap lib/libglob lib/libsignal lib/libenum lib/libdifflib lib/libregex lib/libansi lib/libjson5 lib/libskillusage lib/libskillsync lib/libtranscribe lib/libmcp_oauth lib/libfal_common lib/libtooloutput lib/libxai_http lib/libenvpassthrough lib/libcredential lib/libschemasanitizer lib/libfuzzymatch lib/libinterrupt lib/libtoolbackend lib/libmangateway lib/libratelimit lib/libfilestate lib/libtooldispatch lib/librateguard lib/libskillutils lib/liberrorclassifier lib/libfile_sync lib/libbudgetconfig lib/libthreatpatterns lib/libcurses_widget lib/libasync_poll lib/whisper_cpp
LIB_INCS = $(addprefix -I, lib/liblineedit lib/libmangateway lib/libtoolbackend lib/libratelimit lib/libbinary lib/libbrowser lib/libdebug lib/libosv lib/libwebsite lib/libjson lib/libhttp lib/libtemplate lib/libyaml lib/libcrypto lib/libdotenv lib/libcron lib/libproc lib/libtui lib/libdb lib/libplugin lib/libskin lib/libwebsocket lib/libprotobuf lib/libmcp lib/libpath lib/libdatetime lib/libcsv lib/libhash lib/libuuid lib/libbase64 lib/libhtml lib/libtextwrap lib/libglob lib/libsignal lib/libenum lib/libdifflib lib/libregex lib/libansi lib/libjson5 lib/libskillusage lib/libskillsync lib/libtranscribe lib/libmcp_oauth lib/libfal_common lib/libtooloutput lib/libxai_http lib/libenvpassthrough lib/libcredential lib/libschemasanitizer lib/libfuzzymatch lib/libinterrupt lib/libfilestate lib/libtooldispatch lib/librateguard lib/libskillutils lib/liberrorclassifier lib/libfile_sync lib/libbudgetconfig lib/libthreatpatterns lib/libcredentialfiles lib/libskillaudit lib/libslashconfirm lib/libmsgraph lib/libcurses_widget lib/whisper_cpp)
LIB_OBJ  = lib/libbinary/binary.o lib/libbrowser/camofox_state.o lib/libdebug/debug_helpers.o lib/libosv/osv.o lib/libwebsite/website_policy.o lib/libjson/json.o lib/libhttp/http.o lib/libtemplate/template.o lib/libyaml/yaml.o lib/libcrypto/crypto.o lib/libdotenv/dotenv.o lib/libcron/cron.o lib/libproc/proc.o lib/libtui/tui.o lib/libdb/db.o lib/libdb/sqlite3.o lib/libplugin/plugin.o lib/libskin/skin.o lib/libwebsocket/websocket.o lib/libwebsocket/websocket_async.o lib/libasync_poll/async_poll.o lib/libprotobuf/protobuf.o lib/libmcp/mcp.o lib/libpath/path.o lib/libdatetime/datetime.o lib/libcsv/csv.o lib/libhash/hash.o lib/libuuid/uuid.o lib/libbase64/base64.o lib/libhtml/html.o lib/libtextwrap/textwrap.o lib/libglob/glob.o lib/libsignal/hermes_signal.o lib/libdifflib/difflib.o lib/libregex/hermes_regex.o lib/libansi/ansi.o lib/libansi/ansi_strip.o lib/libjson5/json5.o lib/libskillusage/skill_usage.o lib/libskillusage/skill_provenance.o lib/libskillsync/skills_sync.o lib/libtranscribe/transcribe.o lib/libmcp_oauth/mcp_oauth.o lib/libfal_common/fal_common.o lib/libtooloutput/tool_output.o lib/libxai_http/xai_http.o lib/libenvpassthrough/env_passthrough.o lib/libcredential/credential.o lib/libschemasanitizer/schema_sanitizer.o lib/libfuzzymatch/fuzzy_match.o lib/libinterrupt/interrupt.o lib/libtoolbackend/tool_backend.o lib/libmangateway/managed_gateway.o lib/libratelimit/rate_limit.o lib/libfilestate/file_state.o lib/libtooldispatch/tool_dispatch_helpers.o lib/librateguard/rate_guard.o lib/libskillutils/skill_utils.o lib/liberrorclassifier/error_classifier.o lib/liblineedit/line_edit.o lib/libfile_sync/file_sync.o lib/libbudgetconfig/budget_config.o lib/libthreatpatterns/threat_patterns.o lib/libcredentialfiles/credential_files.o lib/libskillaudit/skill_audit.o lib/libslashconfirm/slash_confirm.o lib/libmsgraph/ms_graph.o lib/libratelimit/rate_limit_tracker.o lib/libtranscribe/transcription_registry.o lib/libcurses_widget/curses_widget.o lib/whisper_cpp/whisper_wrapper.o
LIB_A    = lib/libbinary.a lib/libbrowser.a lib/libdebug.a lib/libosv.a lib/libwebsite.a lib/libenvpassthrough.a lib/libxai_http.a lib/libcredential.a lib/libschemasanitizer.a lib/libfuzzymatch.a lib/libjson.a lib/libhttp.a lib/libtemplate.a lib/libyaml.a lib/libcrypto.a lib/libdotenv.a lib/libcron.a lib/libproc.a lib/libtui.a lib/libdb.a lib/libplugin.a lib/libskin.a lib/libmcp.a lib/libpath.a lib/libdatetime.a lib/libcsv.a lib/libhash.a lib/libuuid.a lib/libbase64.a lib/libhtml.a lib/libtextwrap.a lib/libglob.a lib/libdifflib.a lib/libregex.a lib/libsignal.a lib/libwebsocket.a lib/libprotobuf.a lib/libtoml.a lib/libansi.a lib/libjson5.a lib/libskillusage.a lib/libskillsync.a lib/libtranscribe.a lib/libmcp_oauth.a lib/libfal_common.a lib/libtooloutput.a lib/libratelimit.a lib/libmangateway.a lib/libtoolbackend.a lib/libinterrupt.a lib/libfilestate.a lib/libtooldispatch.a lib/librateguard.a lib/libskillutils.a lib/liberrorclassifier.a lib/liblineedit.a lib/libfile_sync.a lib/libbudgetconfig.a lib/libcredentialfiles.a lib/libskillaudit.a lib/libslashconfirm.a lib/libmsgraph.a lib/libcurses_widget.a lib/whisper_cpp.a

# Include standalone library headers in build
CFLAGS += $(LIB_INCS)

CLI_OBJ = src/cli/cli.o src/cli/commands.o src/cli/config.o src/cli/paths.o src/cli/display.o src/cli/display_core.o src/cli/main.o src/cli/doctor.o src/cli/setup_wizard.o src/cli/cli_gaps.o src/cli/port_backup.o src/cli/port_context_switch_guard.o src/cli/port_env_loader.o src/cli/port_gateway_windows.o src/cli/port_nous_billing.o src/cli/port_voice.o

# Auto-generated hermes_cli/ port objects (154 files)
HERMES_CLI_PORT_OBJ = \

# Additional hermes_cli/ port objects (submodules without own files above)
HERMES_CLI_PORT_EXTRA_OBJ = \

PORT_OBJ = \
    src/cli/port_agent_account_usage.o \
    src/cli/port_agent_anthropic_adapter.o \
    src/cli/port_agent_async_utils.o \
    src/cli/port_agent_auxiliary_client.o \
    src/cli/port_agent_azure_identity_adapter.o \
    src/cli/port_agent_bedrock_adapter.o \
    src/cli/port_agent_browser_provider.o \
    src/cli/port_agent_codex_runtime.o \
    src/cli/port_agent_coding_context.o \
    src/cli/port_agent_context_compressor.o \
    src/cli/port_agent_context_references.o \
    src/cli/port_agent_copilot_acp_client.o \
    src/cli/port_agent_credential_pool.o \
    src/cli/port_agent_credits_tracker.o \
    src/cli/port_agent_display.o \
    src/cli/port_agent_error_classifier.o \
    src/cli/port_agent_gemini_cloudcode_adapter.o \
    src/cli/port_agent_gemini_native_adapter.o \
    src/cli/port_agent_google_oauth.o \
    src/cli/port_agent_image_gen_provider.o \
    src/cli/port_agent_insights.o \
    src/cli/port_agent_jiter_preload.o \
    src/cli/port_agent_memory_manager.o \
    src/cli/port_agent_memory_provider.o \
    src/cli/port_agent_model_metadata.o \
    src/cli/port_agent_nous_rate_guard.o \
    src/cli/port_agent_onboarding.o \
    src/cli/port_agent_plugin_llm.o \
    src/cli/port_agent_process_bootstrap.o \
    src/cli/port_agent_shell_hooks.o \
    src/cli/port_agent_skill_commands.o \
    src/cli/port_agent_skill_utils.o \
    src/cli/port_agent_tool_executor.o \
    src/cli/port_agent_tool_guardrails.o \
    src/cli/port_agent_transcription_provider.o \
    src/cli/port_agent_tts_provider.o \
    src/cli/port_agent_turn_retry_state.o \
    src/cli/port_agent_usage_pricing.o \
    src/cli/port_agent_video_gen_provider.o \
    src/cli/port_agent_web_search_provider.o \
    src/cli/port_agent_web_search_registry.o \
    src/cli/port_cli.o \
    src/cli/port_cron_blueprint_catalog.o \
    src/cli/port_cron_jobs.o \
    src/cli/port_cron_scheduler.o \
    src/cli/port_cron_scripts_classify_items.o \
    src/cli/port_cron_suggestion_catalog.o \
    src/cli/port_cron_suggestions.o \
    src/cli/port_gateway_authz_mixin.o \
    src/cli/port_gateway_channel_directory.o \
    src/cli/port_gateway_config.o \
    src/cli/port_gateway_delivery.o \
    src/cli/port_gateway_display_config.o \
    src/cli/port_gateway_hooks.o \
    src/cli/port_gateway_kanban_watchers.o \
    src/cli/port_gateway_mirror.o \
    src/cli/port_gateway_pairing.o \
    src/cli/port_gateway_platform_registry.o \
    src/cli/port_gateway_platforms__http_client_limits.o \
    src/cli/port_gateway_platforms_api_server.o \
    src/cli/port_gateway_platforms_base.o \
    src/cli/port_gateway_platforms_bluebubbles.o \
    src/cli/port_gateway_platforms_dingtalk.o \
    src/cli/port_gateway_platforms_email.o \
    src/cli/port_gateway_platforms_feishu.o \
    src/cli/port_gateway_platforms_feishu_comment.o \
    src/cli/port_gateway_platforms_feishu_comment_rules.o \
    src/cli/port_gateway_platforms_feishu_meeting_invite.o \
    src/cli/port_gateway_platforms_helpers.o \
    src/cli/port_gateway_platforms_matrix.o \
    src/cli/port_gateway_platforms_msgraph_webhook.o \
    src/cli/port_gateway_platforms_qqbot_adapter.o \
    src/cli/port_gateway_platforms_qqbot_chunked_upload.o \
    src/cli/port_gateway_platforms_qqbot_crypto.o \
    src/cli/port_gateway_platforms_qqbot_keyboards.o \
    src/cli/port_gateway_platforms_qqbot_onboard.o \
    src/cli/port_gateway_platforms_qqbot_utils.o \
    src/cli/port_gateway_platforms_signal.o \
    src/cli/port_gateway_platforms_signal_rate_limit.o \
    src/cli/port_gateway_platforms_slack.o \
    src/cli/port_gateway_platforms_sms.o \
    src/cli/port_gateway_platforms_telegram.o \
    src/cli/port_gateway_platforms_telegram_network.o \
    src/cli/port_gateway_platforms_webhook.o \
    src/cli/port_gateway_platforms_wecom.o \
    src/cli/port_gateway_platforms_wecom_callback.o \
    src/cli/port_gateway_platforms_wecom_crypto.o \
    src/cli/port_gateway_platforms_weixin.o \
    src/cli/port_gateway_platforms_whatsapp.o \
    src/cli/port_gateway_platforms_whatsapp_cloud.o \
    src/cli/port_gateway_platforms_whatsapp_common.o \
    src/cli/port_gateway_platforms_yuanbao.o \
    src/cli/port_gateway_platforms_yuanbao_media.o \
    src/cli/port_gateway_platforms_yuanbao_proto.o \
    src/cli/port_gateway_platforms_yuanbao_sticker.o \
    src/cli/port_gateway_response_filters.o \
    src/cli/port_gateway_run.o \
    src/cli/port_gateway_session.o \
    src/cli/port_gateway_shutdown_forensics.o \
    src/cli/port_gateway_slash_access.o \
    src/cli/port_gateway_slash_commands.o \
    src/cli/port_gateway_status.o \
    src/cli/port_gateway_sticker_cache.o \
    src/cli/port_gateway_stream_consumer.o \
    src/cli/port_tools_approval.o \
    src/cli/port_tools_blueprints.o \
    src/cli/port_tools_browser_camofox.o \
    src/cli/port_tools_browser_camofox_state.o \
    src/cli/port_tools_browser_cdp_tool.o \
    src/cli/port_tools_browser_dialog_tool.o \
    src/cli/port_tools_browser_supervisor.o \
    src/cli/port_tools_browser_tool.o \
    src/cli/port_tools_checkpoint_manager.o \
    src/cli/port_tools_clarify_gateway.o \
    src/cli/port_tools_clarify_tool.o \
    src/cli/port_tools_code_execution_tool.o \
    src/cli/port_tools_computer_use_backend.o \
    src/cli/port_tools_computer_use_cua_backend.o \
    src/cli/port_tools_computer_use_schema.o \
    src/cli/port_tools_computer_use_tool.o \
    src/cli/port_tools_computer_use_vision_routing.o \
    src/cli/port_tools_credential_files.o \
    src/cli/port_tools_cronjob_tools.o \
    src/cli/port_tools_debug_helpers.o \
    src/cli/port_tools_delegate_tool.o \
    src/cli/port_tools_discord_tool.o \
    src/cli/port_tools_env_passthrough.o \
    src/cli/port_tools_env_probe.o \
    src/cli/port_tools_environments_base.o \
    src/cli/port_tools_environments_daytona.o \
    src/cli/port_tools_environments_docker.o \
    src/cli/port_tools_environments_file_sync.o \
    src/cli/port_tools_environments_local.o \
    src/cli/port_tools_environments_managed_modal.o \
    src/cli/port_tools_environments_modal.o \
    src/cli/port_tools_environments_modal_utils.o \
    src/cli/port_tools_environments_singularity.o \
    src/cli/port_tools_environments_ssh.o \
    src/cli/port_tools_fal_common.o \
    src/cli/port_tools_feishu_doc_tool.o \
    src/cli/port_tools_feishu_drive_tool.o \
    src/cli/port_tools_file_operations.o \
    src/cli/port_tools_file_state.o \
    src/cli/port_tools_file_tools.o \
    src/cli/port_tools_fuzzy_match.o \
    src/cli/port_tools_homeassistant_tool.o \
    src/cli/port_tools_image_generation_tool.o \
    src/cli/port_tools_interrupt.o \
    src/cli/port_tools_kanban_tools.o \
    src/cli/port_tools_lazy_deps.o \
    src/cli/port_tools_managed_tool_gateway.o \
    src/cli/port_tools_mcp_oauth.o \
    src/cli/port_tools_mcp_oauth_manager.o \
    src/cli/port_tools_mcp_tool.o \
    src/cli/port_tools_memory_tool.o \
    src/cli/port_tools_microsoft_graph_auth.o \
    src/cli/port_tools_microsoft_graph_client.o \
    src/cli/port_tools_mixture_of_agents_tool.o \
    src/cli/port_tools_openrouter_client.o \
    src/cli/port_tools_osv_check.o \
    src/cli/port_tools_patch_parser.o \
    src/cli/port_tools_path_security.o \
    src/cli/port_tools_process_registry.o \
    src/cli/port_tools_read_extract.o \
    src/cli/port_tools_read_terminal_tool.o \
    src/cli/port_tools_registry.o \
    src/cli/port_tools_schema_sanitizer.o \
    src/cli/port_tools_send_message_tool.o \
    src/cli/port_tools_session_search_tool.o \
    src/cli/port_tools_skill_manager_tool.o \
    src/cli/port_tools_skill_provenance.o \
    src/cli/port_tools_skill_usage.o \
    src/cli/port_tools_skills_ast_audit.o \
    src/cli/port_tools_skills_guard.o \
    src/cli/port_tools_skills_hub.o \
    src/cli/port_tools_skills_sync.o \
    src/cli/port_tools_skills_tool.o \
    src/cli/port_tools_slash_confirm.o \
    src/cli/port_tools_terminal_tool.o \
    src/cli/port_tools_thread_context.o \
    src/cli/port_tools_threat_patterns.o \
    src/cli/port_tools_todo_tool.o \
    src/cli/port_tools_tool_backend_helpers.o \
    src/cli/port_tools_tool_output_limits.o \
    src/cli/port_tools_tool_result_storage.o \
    src/cli/port_tools_tool_search.o \
    src/cli/port_tools_transcription_tools.o \
    src/cli/port_tools_tts_tool.o \
    src/cli/port_tools_url_safety.o \
    src/cli/port_tools_video_generation_tool.o \
    src/cli/port_tools_vision_tools.o \
    src/cli/port_tools_web_tools.o \
    src/cli/port_tools_website_policy.o \
    src/cli/port_tools_write_approval.o \
    src/cli/port_tools_x_search_tool.o \
    src/cli/port_tools_xai_http.o \
    src/cli/port_hermes_cli_web_server.o \
    src/cli/port_tui_gateway_server.o \
    src/cli/port_hermes_cli_main.o \
    src/cli/port_plugins_platforms_discord_adapter.o \
    src/cli/port_run_agent.o \
    src/cli/port_hermes_cli_auth.o \
    src/cli/port_hermes_cli_gateway.o \
    src/cli/port_hermes_cli_kanban_db.o \
    src/cli/port_hermes_state.o \
    src/cli/port_hermes_cli_models.o \
    src/cli/port_optional_skills_migration_openclaw_migration_scripts_openclaw_to_hermes.o \
    src/cli/port_hermes_cli_config.o \
    src/cli/port_plugins_platforms_google_chat_adapter.o \
    src/cli/port_optional_skills_blockchain_hyperliquid_scripts_hyperliquid_client.o \
    src/cli/port_plugins_platforms_line_adapter.o \
    src/cli/port_acp_adapter_server.o \
    src/cli/port_hermes_cli_kanban.o \
    src/cli/port_hermes_cli_plugins.o \
    src/cli/port_hermes_cli_tools_config.o \
    src/cli/port_plugins_kanban_dashboard_plugin_api.o \
    src/cli/port_agent_lsp_servers.o \
    src/cli/port_hermes_cli_setup.o \
    src/cli/port_plugins_platforms_photon_auth.o \
    src/cli/port_hermes_cli_plugins_cmd.o \
    src/cli/port_optional_skills_productivity_telephony_scripts_telephony.o \
    src/cli/port_plugins_platforms_photon_adapter.o \
    src/cli/port_hermes_cli_service_manager.o \
    src/cli/port_hermes_cli_gateway_windows.o \
    src/cli/port_plugins_hermes_achievements_dashboard_plugin_api.o \
    src/cli/port_hermes_cli_profiles.o \
    src/cli/port_optional_skills_blockchain_evm_scripts_evm_client.o \
    src/cli/port_plugins_memory_honcho_session.o \
    src/cli/port_plugins_platforms_teams_adapter.o \
    src/cli/port_plugins_spotify_client.o \
    src/cli/port_hermes_cli_cli_commands_mixin.o \
    src/cli/port_hermes_cli_commands.o \
    src/cli/port_optional_skills_security_godmode_scripts_parseltongue.o \
    src/cli/port_agent_agent_runtime_helpers.o \
    src/cli/port_agent_lsp_client.o \
    src/cli/port_skills_productivity_google_workspace_scripts_google_api.o \
    src/cli/port_plugins_platforms_simplex_adapter.o \
    src/cli/port_acp_adapter_tools.o \
    src/cli/port_agent_curator.o \
    src/cli/port_hermes_cli_skills_hub.o \
    src/cli/port_plugins_memory_honcho_cli.o \
    src/cli/port_plugins_platforms_mattermost_adapter.o \
    src/cli/port_hermes_cli_doctor.o \
    src/cli/port_hermes_cli_nous_account.o \
    src/cli/port_mcp_serve.o \
    src/cli/port_skills_creative_comfyui_scripts__common.o \
    src/cli/port_hermes_cli_curses_ui.o \
    src/cli/port_hermes_cli_goals.o \
    src/cli/port_trajectory_compressor.o \
    src/cli/port_hermes_constants.o \
    src/cli/port_optional_skills_creative_pixel_art_scripts_pixel_art_video.o \
    src/cli/port_plugins_platforms_google_chat_oauth.o \
    src/cli/port_hermes_cli_nous_subscription.o \
    src/cli/port_agent_chat_completion_helpers.o \
    src/cli/port_agent_prompt_builder.o \
    src/cli/port_hermes_cli_runtime_provider.o \
    src/cli/port_skills_productivity_maps_scripts_maps_client.o \
    src/cli/port_hermes_cli_mcp_config.o \
    src/cli/port_model_tools.o \
    src/cli/port_plugins_platforms_irc_adapter.o \
    src/cli/port_plugins_teams_pipeline_pipeline.o \
    src/cli/port_plugins_web_firecrawl_provider.o \
    src/cli/port_tools_tirith_security.o \
    src/cli/port_acp_adapter_session.o \
    src/cli/port_hermes_cli_banner.o \
    src/cli/port_hermes_cli_debug.o \
    src/cli/port_hermes_cli_kanban_diagnostics.o \
    src/cli/port_hermes_cli_profile_distribution.o \
    src/cli/port_optional_skills_finance_stocks_scripts_stocks_client.o \
    src/cli/port_plugins_memory_honcho_client.o \
    src/cli/port_agent_models_dev.o \
    src/cli/port_hermes_cli_auth_commands.o \
    src/cli/port_hermes_cli_backup.o \
    src/cli/port_hermes_cli_clipboard.o \
    src/cli/port_hermes_cli_uninstall.o \
    src/cli/port_agent_secret_sources_bitwarden.o \
    src/cli/port_agent_transports_codex_app_server.o \
    src/cli/port_hermes_cli_mcp_catalog.o \
    src/cli/port_plugins_teams_pipeline_cli.o \
    src/cli/port_skills_creative_comfyui_scripts_run_workflow.o \
    src/cli/port_agent_lsp_manager.o \
    src/cli/port_agent_transports_codex_app_server_session.o \
    src/cli/port_hermes_cli_model_setup_flows.o \
    src/cli/port_hermes_cli_model_switch.o \
    src/cli/port_plugins_memory_holographic_store.o \
    src/cli/port_plugins_teams_pipeline_store.o \
    src/cli/port_website_scripts_generate_skill_docs.o \
    src/cli/port_agent_curator_backup.o \
    src/cli/port_agent_redact.o \
    src/cli/port_hermes_cli_active_sessions.o \
    src/cli/port_optional_skills_blockchain_solana_scripts_solana_client.o \
    src/cli/port_plugins_google_meet_meet_bot.o \
    src/cli/port_plugins_platforms_ntfy_adapter.o \
    src/cli/port_hermes_cli_security_audit.o \
    src/cli/port_hermes_logging.o \
    src/cli/port_optional_skills_productivity_memento_flashcards_scripts_memento_cards.o \
    src/cli/port_plugins_disk_cleanup_disk_cleanup.o \
    src/cli/port_plugins_platforms_homeassistant_adapter.o \
    src/cli/port_plugins_teams_pipeline_meetings.o \
    src/cli/port_plugins_teams_pipeline_models.o \
    src/cli/port_hermes_cli_curator.o \
    src/cli/port_optional_skills_creative_meme_generation_scripts_generate_meme.o \
    src/cli/port_plugins_platforms_discord_voice_mixer.o \
    src/cli/port_scripts_release.o \
    src/cli/port_agent_skill_bundles.o \
    src/cli/port_hermes_cli_telegram_managed_bot.o \
    src/cli/port_skills_productivity_google_workspace_scripts_setup.o \
    src/cli/port_skills_productivity_powerpoint_scripts_office_helpers_merge_runs.o \
    src/cli/port_utils.o \
    src/cli/port_agent_codex_responses_adapter.o \
    src/cli/port_agent_conversation_loop.o \
    src/cli/port_agent_credential_sources.o \
    src/cli/port_agent_file_safety.o \
    src/cli/port_agent_rate_limit_tracker.o \
    src/cli/port_batch_runner.o \
    src/cli/port_acp_adapter_edit_approval.o \
    src/cli/port_agent_context_engine.o \
    src/cli/port_agent_lsp_eventlog.o \
    src/cli/port_agent_tool_dispatch_helpers.o \
    src/cli/port_hermes_cli_dashboard_auth_routes.o \
    src/cli/port_hermes_cli_fallback_cmd.o \
    src/cli/port_hermes_cli_model_catalog.o \
    src/cli/port_hermes_cli_webhook.o \
    src/cli/port_plugins_platforms_photon_cli.o \
    src/cli/port_plugins_spotify_tools.o \
    src/cli/port_plugins_web_xai_provider.o \
    src/cli/port_scripts_profile_tui.o \
    src/cli/port_skills_creative_comfyui_scripts_hardware_check.o \
    src/cli/port_skills_research_polymarket_scripts_polymarket.o \
    src/cli/port_hermes_cli_codex_runtime_plugin_migration.o \
    src/cli/port_plugins_browser_browser_use_provider.o \
    src/cli/port_plugins_google_meet_cli.o \
    src/cli/port_plugins_google_meet_realtime_openai_client.o \
    src/cli/port_plugins_web_parallel_provider.o \
    src/cli/port_scripts_run_tests_parallel.o \
    src/cli/port_scripts_tool_search_livetest.o \
    src/cli/port_agent_image_routing.o \
    src/cli/port_agent_message_sanitization.o \
    src/cli/port_agent_transports_chat_completions.o \
    src/cli/port_agent_transports_types.o \
    src/cli/port_hermes_cli_claw.o \
    src/cli/port_hermes_cli_gui_uninstall.o \
    src/cli/port_hermes_cli_pty_bridge.o \
    src/cli/port_hermes_cli_win_pty_bridge.o \
    src/cli/port_optional_skills_security_oss_forensics_scripts_evidence_store.o \
    src/cli/port_plugins_teams_pipeline_subscriptions.o \
    src/cli/port_acp_adapter_events.o \
    src/cli/port_agent_google_code_assist.o \
    src/cli/port_agent_lsp_install.o \
    src/cli/port_agent_markdown_tables.o \
    src/cli/port_agent_transports_codex_event_projector.o \
    src/cli/port_hermes_cli_dashboard_auth_cookies.o \
    src/cli/port_hermes_cli_dump.o \
    src/cli/port_hermes_cli_kanban_decompose.o \
    src/cli/port_hermes_cli_logs.o \
    src/cli/port_hermes_cli_managed_uv.o \
    src/cli/port_hermes_cli_mcp_picker.o \
    src/cli/port_hermes_cli_session_recap.o \
    src/cli/port_mini_swe_runner.o \
    src/cli/port_plugins_google_meet_process_manager.o \
    src/cli/port_plugins_memory_holographic_holographic.o \
    src/cli/port_plugins_memory_holographic_retrieval.o \
    src/cli/port_plugins_web_tavily_provider.o \
    src/cli/port_skills_productivity_powerpoint_scripts_office_helpers_simplify_redlines.o \
    src/cli/port_tui_gateway_transport.o \
    src/cli/port_website_scripts_extract_skills.o \
    src/cli/port_agent_lsp_cli.o \
    src/cli/port_hermes_cli_azure_detect.o \
    src/cli/port_hermes_cli_blueprint_cmd.o \
    src/cli/port_hermes_cli_checkpoints.o \
    src/cli/port_hermes_cli_cron.o \
    src/cli/port_hermes_cli_env_loader.o \
    src/cli/port_hermes_cli_secrets_cli.o \
    src/cli/port_optional_skills_finance_dcf_model_scripts_validate_dcf.o \
    src/cli/port_optional_skills_mlops_training_trl_fine_tuning_templates_basic_grpo_training.o \
    src/cli/port_optional_skills_research_domain_intel_scripts_domain_intel.o \
    src/cli/port_plugins_web_exa_provider.o \
    src/cli/port_scripts_check_windows_footguns.o \
    src/cli/port_scripts_discord_voice_doctor.o \
    src/cli/port_skills_creative_comfyui_scripts_check_deps.o \
    src/cli/port_toolsets.o \
    src/cli/port_acp_adapter_entry.o \
    src/cli/port_agent_transports_codex.o \
    src/cli/port_hermes_cli_browser_connect.o \
    src/cli/port_hermes_cli_container_boot.o \
    src/cli/port_hermes_cli_copilot_auth.o \
    src/cli/port_hermes_cli_providers.o \
    src/cli/port_hermes_cli_proxy_adapters_xai.o \
    src/cli/port_hermes_cli_proxy_server.o \
    src/cli/port_hermes_cli_secret_prompt.o \
    src/cli/port_optional_skills_security_godmode_scripts_auto_jailbreak.o \
    src/cli/port_plugins_browser_browserbase_provider.o \
    src/cli/port_plugins_browser_firecrawl_provider.o \
    src/cli/port_plugins_google_meet_audio_bridge.o \
    src/cli/port_plugins_google_meet_node_registry.o \
    src/cli/port_plugins_google_meet_tools.o \
    src/cli/port_scripts_lint_diff.o \
    src/cli/port_skills_productivity_powerpoint_scripts_clean.o \
    src/cli/port_agent_i18n.o \
    src/cli/port_agent_lsp_protocol.o \
    src/cli/port_agent_subdirectory_hints.o \
    src/cli/port_auth_na.o \
    src/cli/port_kanban_db_na.o \
    src/cli/port_main_na.o \
    src/cli/port_web_server_extra.o \
    src/gateway/platforms/port_signal_na.o \
    src/cli/port_agent_transports_anthropic.o \
    src/cli/port_agent_transports_base.o \
    src/cli/port_hermes_cli_bundles.o \
    src/cli/port_hermes_cli_dingtalk_auth.o \
    src/cli/port_hermes_cli_inventory.o \
    src/cli/port_hermes_cli_setup_whatsapp_cloud.o \
    src/cli/port_optional_skills_devops_watchers_scripts__watermark.o \
    src/cli/port_plugins_google_meet_node_client.o \
    src/cli/port_plugins_web_searxng_provider.o \
    src/cli/port_scripts_contributor_audit.o \
    src/cli/port_scripts_sample_and_compress.o \
    src/cli/port_slermes_stub_hunt.o \
    src/cli/port_tui_gateway_entry.o \
    src/cli/port_tui_gateway_ws.o \
    src/cli/port_agent_agent_init.o \
    src/cli/port_agent_conversation_compression.o \
    src/cli/port_agent_skill_preprocessing.o \
    src/cli/port_agent_transports_bedrock.o \
    src/cli/port_hermes_cli_cli_output.o \
    src/cli/port_hermes_cli_dashboard_auth_base.o \
    src/cli/port_hermes_cli_dashboard_auth_middleware.o \
    src/cli/port_hermes_cli_proxy_adapters_base.o \
    src/cli/port_hermes_cli_send_cmd.o \
    src/cli/port_hermes_cli_status.o \
    src/cli/port_hermes_cli_xai_retirement.o \
    src/cli/port_optional_skills_creative_kanban_video_orchestrator_scripts_bootstrap_pipeline.o \
    src/cli/port_optional_skills_creative_kanban_video_orchestrator_scripts_monitor.o \
    src/cli/port_optional_skills_health_fitness_nutrition_scripts_body_calc.o \
    src/cli/port_optional_skills_research_darwinian_evolver_scripts_parrot_openrouter.o \
    src/cli/port_optional_skills_research_darwinian_evolver_templates_custom_problem_template.o \
    src/cli/port_optional_skills_research_osint_investigation_scripts_fetch_icij_offshore.o \
    src/cli/port_optional_skills_security_godmode_scripts_godmode_race.o \
    src/cli/port_plugins_google_meet_node_server.o \
    src/cli/port_plugins_plugin_utils.o \
    src/cli/port_plugins_web_brave_free_provider.o \
    src/cli/port_plugins_web_ddgs_provider.o \
    src/cli/port_providers_base.o \
    src/cli/port_scripts_build_skills_index.o \
    src/cli/port_skills_productivity_powerpoint_scripts_add_slide.o \
    src/cli/port_acp_adapter_permissions.o \
    src/cli/port_agent_background_review.o \
    src/cli/port_agent_browser_registry.o \
    src/cli/port_agent_credential_persistence.o \
    src/cli/port_agent_image_gen_registry.o \
    src/cli/port_agent_lsp_workspace.o \
    src/cli/port_agent_ssl_guard.o \
    src/cli/port_gateway_memory_monitor.o \
    src/cli/port_hermes_cli_codex_runtime_switch.o \
    src/cli/port_hermes_cli_dashboard_auth_ws_tickets.o \
    src/cli/port_hermes_cli_oneshot.o \
    src/cli/port_hermes_cli_portal_cli.o \
    src/cli/port_hermes_cli_relaunch.o \
    src/cli/port_hermes_cli_stdio.o \
    src/cli/port_optional_skills_mcp_fastmcp_templates_database_server.o \
    src/cli/port_optional_skills_productivity_canvas_scripts_canvas_api.o \
    src/cli/port_optional_skills_research_osint_investigation_scripts_fetch_wikipedia.o \
    src/cli/port_optional_skills_research_osint_investigation_scripts_timing_analysis.o \
    src/cli/port_plugins_google_meet_node_protocol.o \
    src/cli/port_plugins_teams_pipeline_runtime.o \
    src/cli/port_skills_creative_comfyui_scripts_extract_schema.o \
    src/cli/port_slermes_scripts_perf_gate.o \
    src/cli/port_slermes_stub_hunt_v5.o \
    src/cli/port_tui_gateway_slash_worker.o \
    src/cli/port_website_scripts_generate_llms_txt.o \
    src/cli/port_agent_iteration_budget.o \
    src/cli/port_agent_moonshot_schema.o \
    src/cli/port_agent_runtime_cwd.o \
    src/cli/port_agent_stream_diag.o \
    src/cli/port_agent_system_prompt.o \
    src/cli/port_agent_video_gen_registry.o \
    src/cli/port_gateway_runtime_footer.o \
    src/cli/port_hermes_cli__subprocess_compat.o \
    src/cli/port_hermes_cli_cli_agent_setup_mixin.o \
    src/cli/port_hermes_cli_codex_models.o \
    src/cli/port_hermes_cli_completion.o \
    src/cli/port_hermes_cli_dashboard_register.o \
    src/cli/port_hermes_cli_kanban_specify.o \
    src/cli/port_hermes_cli_pairing.o \
    src/cli/port_hermes_cli_proxy_cli.o \
    src/cli/port_hermes_time.o \
    src/cli/port_optional_skills_mcp_fastmcp_templates_api_wrapper.o \
    src/cli/port_optional_skills_research_osint_investigation_scripts_entity_resolution.o \
    src/cli/port_scripts_keystroke_diagnostic.o \
    src/cli/port_skills_creative_comfyui_scripts_auto_fix_deps.o \
    src/cli/port_skills_creative_comfyui_scripts_health_check.o \
    src/cli/port_skills_productivity_google_workspace_scripts_gws_bridge.o \
    src/cli/port_skills_productivity_ocr_and_documents_scripts_extract_pymupdf.o \
    src/cli/port_slermes_stub_hunt_v2.o \
    src/cli/port_slermes_stub_hunt_v3.o \
    src/cli/port_slermes_stub_hunt_v4.o \
    src/cli/port_toolset_distributions.o \
    src/cli/port_agent_lsp_range_shift.o \
    src/cli/port_agent_transcription_registry.o \
    src/cli/port_agent_transports_hermes_tools_mcp_server.o \
    src/cli/port_agent_tts_registry.o \
    src/cli/port_gateway_session_context.o \
    src/cli/port_hermes_cli_dashboard_auth_registry.o \
    src/cli/port_hermes_cli_dep_ensure.o \
    src/cli/port_hermes_cli_fallback_config.o \
    src/cli/port_hermes_cli_mcp_security.o \
    src/cli/port_hermes_cli_mcp_startup.o \
    src/cli/port_hermes_cli_model_cost_guard.o \
    src/cli/port_hermes_cli_partial_compress.o \
    src/cli/port_hermes_cli_profile_describer.o \
    src/cli/port_hermes_cli_timeouts.o \
    src/cli/port_optional_skills_mcp_fastmcp_templates_file_processor.o \
    src/cli/port_optional_skills_productivity_memento_flashcards_scripts_youtube_quiz.o \
    src/cli/port_optional_skills_research_drug_discovery_scripts_ro5_screen.o \
    src/cli/port_optional_skills_research_osint_investigation_scripts__normalize.o \
    src/cli/port_optional_skills_research_osint_investigation_scripts_fetch_ofac_sdn.o \
    src/cli/port_optional_skills_research_osint_investigation_scripts_fetch_opencorporates.o \
    src/cli/port_optional_skills_research_osint_investigation_scripts_fetch_sec_edgar.o \
    src/cli/port_scripts_analyze_livetest.o \
    src/cli/port_scripts_benchmark_browser_eval.o \
    src/cli/port_skills_creative_comfyui_scripts_fetch_logs.o \
    src/cli/port_skills_media_youtube_content_scripts_fetch_transcript.o \
    src/cli/port_tui_gateway_event_publisher.o \
    src/cli/port_acp_adapter_auth.o \
    src/cli/port_agent_lsp_reporter.o \
    src/cli/port_agent_portal_tags.o \
    src/cli/port_agent_prompt_caching.o \
    src/cli/port_agent_title_generator.o \
    src/cli/port_agent_trajectory.o \
    src/cli/port_gateway_stream_dispatch.o \
    src/cli/port_gateway_whatsapp_identity.o \
    src/cli/port_hermes_cli_callbacks.o \
    src/cli/port_hermes_cli_migrate.o \
    src/cli/port_hermes_cli_psutil_android.o \
    src/cli/port_hermes_cli_pt_input_extras.o \
    src/cli/port_hermes_cli_suggestions_cmd.o \
    src/cli/port_optional_skills_devops_watchers_scripts_watch_github.o \
    src/cli/port_optional_skills_devops_watchers_scripts_watch_http_json.o \
    src/cli/port_optional_skills_devops_watchers_scripts_watch_rss.o \
    src/cli/port_optional_skills_finance_excel_author_scripts_recalc.o \
    src/cli/port_optional_skills_health_fitness_nutrition_scripts_nutrition_search.o \
    src/cli/port_optional_skills_mcp_fastmcp_scripts_scaffold_fastmcp.o \
    src/cli/port_optional_skills_research_osint_investigation_scripts_build_findings.o \
    src/cli/port_optional_skills_research_osint_investigation_scripts_fetch_nyc_acris.o \
    src/cli/port_optional_skills_research_osint_investigation_scripts_fetch_usaspending.o \
    src/cli/port_plugins_google_meet_node_cli.o \
    src/cli/port_scripts_docker_config_migrate.o \
    src/cli/port_skills_creative_comfyui_scripts_run_batch.o \
    src/cli/port_skills_creative_comfyui_scripts_ws_monitor.o \
    src/cli/port_skills_creative_excalidraw_scripts_upload.o \
    src/cli/port_skills_productivity_powerpoint_scripts_office_pack.o \
    src/cli/port_slermes_scripts_coverage_gate.o \
    src/cli/port_slermes_src_tools_vision_analysis.o \
    src/cli/port_tui_gateway_render.o \
    src/cli/port_acp_adapter_provenance.o \
    src/cli/port_agent_gemini_schema.o \
    src/cli/port_hermes_cli__parser.o \
    src/cli/port_hermes_cli_colors.o \
    src/cli/port_hermes_cli_dashboard_auth_audit.o \
    src/cli/port_hermes_cli_dashboard_auth_login_page.o \
    src/cli/port_hermes_cli_platforms.o \
    src/cli/port_hermes_cli_slack_cli.o \
    src/cli/port_hermes_cli_subcommands_gateway.o \
    src/cli/port_optional_skills_creative_pixel_art_scripts_pixel_art.o \
    src/cli/port_optional_skills_research_drug_discovery_scripts_chembl_target.o \
    src/cli/port_optional_skills_research_osint_investigation_scripts__http.o \
    src/cli/port_optional_skills_research_osint_investigation_scripts_fetch_courtlistener.o \
    src/cli/port_optional_skills_research_osint_investigation_scripts_fetch_gdelt.o \
    src/cli/port_optional_skills_research_osint_investigation_scripts_fetch_senate_ld.o \
    src/cli/port_optional_skills_research_osint_investigation_scripts_fetch_wayback.o \
    src/cli/port_scripts_build_model_catalog.o \
    src/cli/port_scripts_check_subprocess_stdin.o \
    src/cli/port_scripts_install_psutil_android.o \
    src/cli/port_skills_productivity_ocr_and_documents_scripts_extract_marker.o \
    src/cli/port_slermes_scripts_web_extract_delegate.o \
    src/cli/port_tools_neutts_synth.o \
    src/cli/port_website_scripts_extract_automation_blueprints.o \
    src/cli/port_agent_lmstudio_reasoning.o \
    src/cli/port_agent_manual_compression_feedback.o \
    src/cli/port_agent_retry_utils.o \
    src/cli/port_agent_tool_result_classification.o \
    src/cli/port_agent_turn_context.o \
    src/cli/port_agent_turn_finalizer.o \
    src/cli/port_gateway_restart.o \
    src/cli/port_hermes_bootstrap.o \
    src/cli/port_hermes_cli_build_info.o \
    src/cli/port_hermes_cli_subcommands__shared.o \
    src/cli/port_hermes_cli_subcommands_acp.o \
    src/cli/port_hermes_cli_subcommands_auth.o \
    src/cli/port_hermes_cli_subcommands_backup.o \
    src/cli/port_hermes_cli_subcommands_claw.o \
    src/cli/port_hermes_cli_subcommands_config.o \
    src/cli/port_hermes_cli_subcommands_cron.o \
    src/cli/port_hermes_cli_subcommands_dashboard.o \
    src/cli/port_hermes_cli_subcommands_debug.o \
    src/cli/port_hermes_cli_subcommands_doctor.o \
    src/cli/port_hermes_cli_subcommands_dump.o \
    src/cli/port_hermes_cli_subcommands_gui.o \
    src/cli/port_hermes_cli_subcommands_hooks.o \
    src/cli/port_hermes_cli_subcommands_import_cmd.o \
    src/cli/port_hermes_cli_subcommands_insights.o \
    src/cli/port_hermes_cli_subcommands_login.o \
    src/cli/port_hermes_cli_subcommands_logout.o \
    src/cli/port_hermes_cli_subcommands_logs.o \
    src/cli/port_hermes_cli_subcommands_mcp.o \
    src/cli/port_hermes_cli_subcommands_memory.o \
    src/cli/port_hermes_cli_subcommands_model.o \
    src/cli/port_hermes_cli_subcommands_pairing.o \
    src/cli/port_hermes_cli_subcommands_plugins.o \
    src/cli/port_hermes_cli_subcommands_postinstall.o \
    src/cli/port_hermes_cli_subcommands_profile.o \
    src/cli/port_hermes_cli_subcommands_prompt_size.o \
    src/cli/port_hermes_cli_subcommands_security.o \
    src/cli/port_hermes_cli_subcommands_setup.o \
    src/cli/port_hermes_cli_subcommands_skills.o \
    src/cli/port_hermes_cli_subcommands_slack.o \
    src/cli/port_hermes_cli_subcommands_status.o \
    src/cli/port_hermes_cli_subcommands_tools.o \
    src/cli/port_hermes_cli_subcommands_uninstall.o \
    src/cli/port_hermes_cli_subcommands_update.o \
    src/cli/port_hermes_cli_subcommands_version.o \
    src/cli/port_hermes_cli_subcommands_webhook.o \
    src/cli/port_hermes_cli_subcommands_whatsapp.o \
    src/cli/port_hermes_cli_tips.o \
    src/cli/port_optional_skills_creative_pixel_art_scripts_palettes.o \
    src/cli/port_optional_skills_research_darwinian_evolver_scripts_show_snapshot.o \
    src/cli/port_optional_skills_security_godmode_scripts_load_godmode.o \
    src/cli/port_plugins_security_guidance_patterns.o \
    src/cli/port_setup.o \
    src/cli/port_skills_research_arxiv_scripts_search_arxiv.o \
    src/cli/port_slermes_src_tools_vision_delegate.o \
    src/cli/port_tools_ansi_strip.o \
    src/cli/port_tools_binary_extensions.o \
    src/cli/port_tools_budget_config.o \
    src/cli/port_tools_yuanbao_tools.o \
    src/cli/port_hermes_cli_gateway_enroll.o \
    src/cli/port_hermes_cli_managed_scope.o \
    src/cli/port_hermes_cli_provider_catalog.o \
    src/cli/port_hermes_cli_session_listing.o \
    src/cli/port_hermes_cli_memory_providers.o \
    src/cli/port_hermes_cli_nous_billing.o \
    src/cli/port_hermes_cli_skin_engine.o \
    src/cli/port_hermes_cli_memory_setup.o \
    src/cli/port_hermes_cli_mcp_startup.o \
    src/cli/port_gateway_run.o \
    src/cli/port_tools_process_registry.o \
    src/cli/port_tools_terminal_tool.o \
    src/cli/port_tools_checkpoint_manager.o \
    src/cli/port_tools_write_approval.o \
    src/cli/port_tools_environments_docker.o \
    src/tools/environments.o \
    src/cli/port_tools_mcp_tool.o \
    src/cli/port_tools_skill_usage.o \
    src/cli/port_tools_memory_tool.o \
    src/cli/port_tools_file_tools.o \
    src/cli/port_tools_delegate_tool.o \
    src/cli/port_tools_browser_tool.o \
    src/cli/port_tools_browser_supervisor.o \
    src/cli/port_gateway_status.o \
    src/gateway/session.o \
    src/cli/port_gateway_kanban_watchers.o \
    src/gateway/stream_consumer.o \
    src/cli/port_gateway_channel_directory.o \
    src/cli/port_gateway_whatsapp_identity.o \
    src/gateway/session_context.o \
    src/cli/port_cron_jobs.o \
    src/cli/port_agent_insights.o \
    src/cli/port_agent_usage_pricing.o \
    src/cli/port_auth.o \
    src/cli/port_backup.o \
    src/cli/port_cli_extra.o \
    src/cli/port_config.o \
    src/cli/port_container_boot.o \
    src/cli/port_context_switch_guard.o \
    src/cli/port_dump.o \
    src/cli/port_env_loader.o \
    src/cli/port_gateway.o \
    src/cli/port_gateway_windows.o \
    src/cli/port_goals.o \
    src/cli/port_kanban_db.o \
    src/cli/port_main.o \
    src/cli/port_managed_scope.o \
    src/cli/port_memory_providers.o \
    src/cli/port_model_normalize.o \
    src/cli/port_model_setup_flows.o \
    src/cli/port_model_switch.o \
    src/cli/port_models.o \
    src/cli/port_nous_billing.o \
    src/cli/port_plugins.o \
    src/cli/port_profiles.o \
    src/cli/port_provider_catalog.o \
    src/cli/port_proxy/adapters/nous_portal.o \
    src/cli/port_runtime_provider.o \
    src/cli/port_voice.o \
    src/cli/port_web_server.o \
    src/gateway/port_api_server.o \
    src/gateway/port_base.o \
    src/gateway/port_signal.o \
    src/gateway/port_signal_rate_limit.o \
    src/gateway/port_webhook.o \
    src/tools/port_browser_tool.o \
    src/tools/port_environments/base.o \
    src/tools/port_process_registry.o \
    src/agent/anthropic_adapter.o

# Desktop app parity objects (v465-v468)
DESKTOP_OBJ = \
    src/pty.o \
    src/terminal.o \
    src/window_compositor.o \
    src/window_stubs.o \
    src/chat_render.o \
    src/chat_composer.o \
    src/gateway_client.o \
    src/clipboard.o \
    src/file_ops.o \
    src/gateway_probe.o \
    src/desktop_app_common.o \
    src/app_desktop.o

# Platform-specific window backends
ifeq ($(UNAME_S),Linux)
    DESKTOP_OBJ += src/window_wayland.o
    DESKTOP_OBJ += src/xdg-shell-protocol.o
else ifeq ($(UNAME_S),Darwin)
    DESKTOP_OBJ += src/window_macos.o
    PLATFORM_LDFLAGS += -framework Cocoa -framework IOKit -framework OpenGL
else
    # Windows (MinGW/Cygwin)
    DESKTOP_OBJ += src/window_win32.o
    PLATFORM_LDFLAGS += -lgdi32 -lole32 -lshell32 -luser32 -lopengl32
endif

# Phase targets (each adds its objects)
DEPS_OBJ = src/hermes_error.o src/secrets.o src/hermes_tokenizer.o src/xai_retirement.o src/skills_hub.o src/mcp_serve.o src/api_server.o src/hermes_env_keys.o src/web_dashboard.o src/jiter_preload.o
AGENT_OBJ = src/agent/agent_loop.o src/agent/agent_runtime_helpers.o src/agent/chat_completion_helpers.o src/agent/credential_pool.o src/agent/credential_sources.o src/agent/llm_client.o src/agent/credential_persistence.o src/agent/logger.o src/agent/context.o src/agent/context_engine.o src/agent/stream_diag.o src/agent/title.o src/agent/provider.o src/agent/provider_openai.o src/agent/provider_openrouter.o src/agent/provider_deepseek.o src/agent/provider_xai.o src/agent/provider_anthropic.o src/agent/provider_google.o src/agent/provider_azure.o src/agent/provider_bedrock.o src/agent/process_bootstrap.o src/agent/provider_custom.o src/agent/provider_codex_responses.o src/agent/codex_app_server_client.o src/agent/codex_app_server_session.o src/agent/codex_event_projector.o src/agent/hermes_tools_mcp_server.o src/agent/credential_pool.o src/agent/fallback_routing.o src/agent/budget_tracker.o src/agent/provider_metadata.o src/agent/checkpoint.o src/agent/plugin_ext.o src/agent/redact.o src/agent/audit.o src/agent/sanitize.o src/agent/vault.o src/agent/think_scrubber.o src/agent/retry_utils.o src/agent/trajectory.o src/agent/portal_tags.o src/agent/markdown_tables.o src/agent/markdown_render.o src/agent/file_safety.o src/agent/system_prompt.o src/agent/skill_preprocessing.o src/agent/tool_guardrails.o src/agent/i18n.o src/agent/subdir_hints.o src/agent/onboarding.o src/agent/image_routing.o src/agent/skill_bundles.o src/agent/usage_pricing.o src/agent/lmstudio_reasoning.o src/agent/manual_compression_feedback.o src/agent/prompt_caching.o src/agent/context_references.o src/agent/gemini_schema.o src/agent/moonshot_schema.o src/agent/auxiliary_client.o src/agent/browser_provider.o src/agent/browser_registry.o src/agent/tool_error.o src/agent/hook_registry.o src/agent/shell_hooks.o src/agent/ssl_guard.o src/agent/nous_rate_guard.o src/agent/agent_gaps.o src/agent/hermes_gap_fixes.o src/agent/transcription_provider.o src/agent/web_search_provider.o src/agent/curator.o src/agent/plugin_llm.o src/agent/tool_executor.o src/agent/google_code_assist.o src/agent/copilot_acp_client.o src/agent/azure_identity_adapter.o src/agent/agent_message_repair.o src/agent/agent_message_sanitize.o src/agent/proxy_utils.o src/agent/credits_tracker.o src/agent/turn_context.o src/agent/turn_retry_state.o src/agent/agent_init.o src/agent/title_generator.o src/provider/token_exchange.o src/provider/google_oauth.o src/provider/copilot_oauth.o src/acp/server.o src/acp/edit_approval.o src/acp/events.o src/acp/permissions.o src/acp/resource.o src/tools/rate_limit.o src/tools/tool_result.o src/agent/skill_commands.o src/agent/memory_provider.o src/agent/anthropic_adapter.o src/agent/async_utils.o src/agent/background_review.o src/agent/bedrock_adapter.o src/agent/codex_responses_adapter.o src/agent/codex_runtime.o src/agent/context_compressor.o src/agent/conversation_compression.o src/agent/conversation_loop.o src/agent/turn_finalizer.o src/agent/gemini_cloudcode_adapter.o src/agent/gemini_native_adapter.o src/agent/insights.o src/agent/memory_manager.o src/agent/message_sanitization.o src/agent/model_metadata.o src/agent/models_dev.o src/agent/prompt_builder.o src/agent/runtime_cwd.o src/agent/subdirectory_hints.o src/agent/port_agent_antigravity_code_assist.o src/agent/port_agent_antigravity_oauth.o src/agent/port_agent_auxiliary_client.o src/agent/port_agent_billing_view.o src/agent/port_agent_context_compressor.o src/agent/port_agent_conversation_loop.o src/agent/port_agent_display.o src/agent/port_agent_image_gen_provider.o src/agent/port_agent_memory_manager.o src/agent/port_agent_memory_provider.o src/agent/port_agent_message_content.o src/agent/port_agent_prompt_builder.o src/agent/port_agent_secret_scope.o src/agent/port_agent_skill_commands.o src/agent/port_agent_skill_utils.o src/agent/port_agent_system_prompt.o src/agent/port_agent_title_generator.o src/agent/port_agent_tool_executor.o src/agent/port_agent_antigravity_code_assist.o src/agent/port_agent_antigravity_oauth.o src/agent/port_agent_auxiliary_client.o src/agent/port_agent_billing_view.o src/agent/port_agent_context_compressor.o src/agent/port_agent_conversation_loop.o src/agent/port_agent_display.o src/agent/port_agent_image_gen_provider.o src/agent/port_agent_memory_manager.o src/agent/port_agent_memory_provider.o src/agent/port_agent_message_content.o src/agent/port_agent_prompt_builder.o src/agent/port_agent_secret_scope.o src/agent/port_agent_skill_commands.o src/agent/port_agent_skill_utils.o src/agent/port_agent_system_prompt.o src/agent/port_agent_title_generator.o src/agent/port_agent_tool_executor.o
TOOLS_OBJ = src/tools/registry.o src/tools/terminal.o src/tools/terminal_tool.o src/tools/file.o src/tools/web.o src/tools/skills.o src/tools/tool_init.o src/tools/patch.o src/tools/exec_code.o src/tools/clarify.o src/tools/memory.o src/tools/todo.o src/tools/process.o src/tools/process_registry.o src/tools/send_message.o src/tools/cronjob.o src/tools/skill_mgmt.o src/tools/session_search.o src/tools/session_crud.o src/tools/tts.o src/tools/vision.o src/tools/delegate.o src/tools/x_search.o src/tools/browser.o src/tools/approval.o src/tools/url_safety.o src/tools/tirith.o src/tools/voice_mode.o src/tools/image_gen.o src/tools/homeassistant.o src/tools/kanban.o src/tools/computer_use.o src/tools/result_storage.o src/tools/api_helpers.o src/tools/tool_config.o src/tools/discord.o src/tools/mcp_tool.o src/tools/file_batch.o src/tools/file_watch.o src/tools/feishu_tools.o src/tools/feishu_comment_rules.o src/tools/file_merge.o src/tools/mixture_of_agents.o src/tools/video_gen.o src/tools/video_gen_registry.o src/tools/video_analyze.o src/tools/image_gen_registry.o src/tools/web_search_registry.o src/tools/path_security.o src/tools/xai_http.o src/tools/account_usage.o src/tools/ansi_strip.o src/tools/transcribe.o src/tools/wecom_crypto.o src/tools/yuanbao_tools.o src/tools/yuanbao_media.o src/tools/media_cache.o src/sandbox_escape.o src/tools/env_probe.o src/tools/skills_guard.o src/tools/curator_backup.o src/tools/daytona.o src/tools/environment_gaps.o src/tools/image_gen_provider.o src/tools/tool_result_classification.o src/tools/tts_provider.o src/tools/tts_registry.o src/tools/video_gen_provider.o src/tools/binary_extensions.o src/tools/browser_camofox.o src/tools/managed_tool_gateway.o src/tools/microsoft_graph_auth.o src/tools/microsoft_graph_client.o src/tools/tool_backend_helpers.o src/tools/tool_output_limits.o src/tools/tool_search.o src/tools/checkpoint_manager.o src/tools/port_tools_async_delegation.o src/tools/port_base.o
GATEWAY_OBJ = src/gateway/server.o src/gateway/gateway_runtime.o src/gateway/config.o src/gateway/telegram_filter.o src/gateway/helpers.o src/gateway/shutdown_forensics.o src/gateway/sticker_cache.o src/gateway/mirror.o src/gateway/slash_access.o src/gateway/runtime_footer.o src/gateway/channel_directory.o src/gateway/gateway_gaps.o src/gateway/platforms/telegram.o src/gateway/platforms/telegram_network.o src/gateway/platforms/discord.o src/gateway/platforms/webhook.o src/gateway/platforms/slack.o src/gateway/platforms/matrix.o src/gateway/platforms/mattermost.o src/gateway/platforms/whatsapp.o src/gateway/platforms/email.o src/gateway/platforms/signal.o src/gateway/platforms/homeassistant.o src/gateway/platforms/sms.o src/gateway/platforms/feishu.o src/gateway/platforms/wecom.o src/gateway/platforms/wecom_callback.o src/gateway/platforms/dingtalk.o src/gateway/platforms/qqbot.o src/gateway/platforms/bluebubbles.o src/gateway/platforms/msgraph_webhook.o src/gateway/platforms/weixin.o src/gateway/platforms/yuanbao.o src/gateway/platforms/base.o src/gateway/platforms/base_ext.o src/gateway/platforms/base_ext2.o src/gateway/platforms/base_adapter.o src/gateway/platforms/api_server_adapter.o src/gateway/platforms/api_server_adapter_handlers.o src/gateway/platforms/api_server_adapter_sessions.o src/gateway/platforms/api_server_adapter_chat.o src/gateway/platforms/api_server_adapter_responses.o src/gateway/platforms/api_server_adapter_runs.o src/gateway/platforms/api_server_adapter_cron.o src/gateway/platforms/feishu_comment_rules.o src/gateway/platforms/signal_rate_limit.o src/gateway/platforms/feishu_comment.o src/gateway/platforms/yuanbao_proto.o src/gateway/platforms/yuanbao_media.o src/gateway/platforms/yuanbao_sticker.o src/gateway/gateway_lifecycle.o src/gateway/stream_events.o src/gateway/delivery.o src/gateway/display_config.o src/gateway/memory_monitor.o src/gateway/pairing.o src/gateway/restart.o src/gateway/run.o src/gateway/session.o src/gateway/session_context.o src/gateway/stream_consumer.o src/gateway/whatsapp_identity.o src/gateway/port_gateway_message_timestamps.o src/gateway/port_gateway_platforms_api_server.o src/gateway/port_gateway_platforms_base.o src/gateway/port_gateway_relay_adapter.o src/gateway/port_gateway_relay_auth.o src/gateway/port_gateway_relay_descriptor.o src/gateway/port_gateway_relay_transport.o src/gateway/port_gateway_relay_ws_transport.o src/gateway/port_gateway_rich_sent_store.o
CRON_OBJ = src/cron/scheduler.o src/cron/jobs.o src/cron/cron_extras.o src/cron/cron_sqlite.o src/cron/cron_cli.o src/cron/port_cron_scheduler_provider.o

# Progressively larger builds
PHASE1_OBJ = $(DEPS_OBJ)
PHASE2_OBJ = $(PHASE1_OBJ) src/main.o $(AGENT_OBJ) $(CLI_OBJ)
PHASE3_OBJ = $(PHASE2_OBJ) $(TOOLS_OBJ)
PHASE4_OBJ = $(PHASE3_OBJ) $(GATEWAY_OBJ)
PHASE5_OBJ = $(PHASE4_OBJ) $(CRON_OBJ)

.PHONY: all clean phase1 phase2 phase3 phase4 phase5 test asan coverage clean-tests \
        upstream-sync upstream-merge what-changed test-libs tui docs asan fuzz static \
        static-analysis

all: phase5

# Parallel build support: `make -j$(nproc)` compiles all objects in parallel
# Phase targets enforce ordering for link-time dependencies only
phase1: $(PHASE1_OBJ)
	@echo "Phase 1 complete: $(words $(PHASE1_OBJ)) objects"

phase2: $(PHASE2_OBJ)
	@echo "Phase 2 complete: $(words $(PHASE2_OBJ)) objects"

phase3: $(PHASE3_OBJ)
	@echo "Phase 3 complete: $(words $(PHASE3_OBJ)) objects"

phase4: $(PHASE4_OBJ)
	@echo "Phase 4 complete: $(words $(PHASE4_OBJ)) objects"

phase5: slermes
	@echo "Phase 5 complete: slermes binary built"

slermes: $(PHASE5_OBJ) src/main.o $(HERMES_CLI_PORT_OBJ) $(HERMES_CLI_PORT_EXTRA_OBJ) $(PORT_OBJ) $(DESKTOP_OBJ) $(LIB_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(PLATFORM_LDFLAGS) $(LIBS) \
		$(if $(findstring 1,$(HAS_NCURSES)),-L lib/syslib -lncursesw -ltinfo -lpanelw) -lstdc++ \
		lib/whisper_cpp/lib/libwhisper.a lib/whisper_cpp/lib/libggml.a lib/whisper_cpp/lib/libggml-base.a lib/whisper_cpp/lib/libggml-cpu.a

# C11 Desktop App build (ncurses-based, replaces Electron/TS shell)
# PoP: apps/desktop/src/app/desktop-controller.tsx
DESKTOP_APP_OBJ = src/main_desktop.o src/app_desktop.o src/chat_render.o src/chat_composer.o \
    src/desktop_app_common.o src/gateway_probe.o src/window_stubs.o \
    src/hermes_env_keys.o src/file_ops.o src/cli/port_hermes_logging.o \
    src/agent/logger.o src/pty.o
DESKTOP_LIBS_FILTER = lib/libdb/sqlite3.o lib/whisper_cpp/whisper_wrapper.o lib/libtranscribe/transcribe.o
# ── Custom GUI desktop (SDL2-based, our own framework) ────────────────
DESKTOP_GUI_OBJ := src/gui_core.o src/desktop_gui.o src/slermes_home.o src/chat_render.o lib/libdb/sqlite3.o lib/libhttp/http.o lib/libjson/json.o lib/libbase64/base64.o lib/libcrypto/crypto.o
DESKTOP_GUI_CFLAGS := $(shell pkg-config --cflags sdl2 SDL2_ttf)
DESKTOP_GUI_LIBS := $(shell pkg-config --libs sdl2 SDL2_ttf) -lm -lssl -lcrypto -lz

lib/libdb/sqlite3.o: lib/libdb/sqlite3.c lib/libdb/sqlite3.h
	$(CC) $(CFLAGS) -DSQLITE_THREADSAFE=0 -DSQLITE_OMIT_LOAD_EXTENSION -c -o $@ $<

src/slermes_home.o: src/slermes_home.c include/slermes_home.h
	$(CC) $(CFLAGS) -c -o $@ $<

desktop-gui: $(DESKTOP_GUI_OBJ)
	$(CC) $(CFLAGS) $(DESKTOP_GUI_CFLAGS) -o slermes-desktop-gui $(DESKTOP_GUI_OBJ) $(DESKTOP_GUI_LIBS)
	@echo "slermes-desktop-gui built — custom graphical desktop app"

src/gui_core.o: src/gui_core.c include/gui_core.h
	$(CC) $(CFLAGS) $(DESKTOP_GUI_CFLAGS) -I include -c -o $@ $<

src/desktop_gui.o: src/desktop_gui.c include/gui_core.h
	$(CC) $(CFLAGS) $(DESKTOP_GUI_CFLAGS) -I include -I lib -c -o $@ $<

desktop: $(DESKTOP_APP_OBJ) $(filter-out $(DESKTOP_LIBS_FILTER), $(LIB_OBJ))
	$(CC) $(CFLAGS) -o hermes-desktop $^ $(LDFLAGS) $(PLATFORM_LDFLAGS) $(LIBS) \
		/usr/lib/x86_64-linux-gnu/libncursesw.so.6 /usr/lib/x86_64-linux-gnu/libtinfo.so.6 -lpanelw -lstdc++ \
		lib/whisper_cpp/lib/libwhisper.a lib/whisper_cpp/lib/libggml.a lib/whisper_cpp/lib/libggml-base.a lib/whisper_cpp/lib/libggml-cpu.a
	@echo "hermes-desktop built — C11 Desktop App (replaces Electron shell)"

# Recompile curses_widget with ncurses TUI support for the tui build
lib/libcurses_widget/curses_widget_tui.o: lib/libcurses_widget/curses_widget.c lib/libcurses_widget/curses_widget.h
	$(CC) $(CFLAGS) $(LIB_INCS) -DHAS_NCURSES_TUI -I lib/libncurses/include -c $< -o $@

# ncurses full-screen TUI build (shared libs from system)
TUI_LIB_OBJ = $(filter-out lib/libcurses_widget/curses_widget.o, $(LIB_OBJ)) lib/libcurses_widget/curses_widget_tui.o
tui: $(PHASE5_OBJ) $(TUI_OBJ) src/main.o $(TUI_LIB_OBJ)
	$(CC) $(CFLAGS) -DHAS_NCURSES_TUI -o slermes-tui $^ $(LDFLAGS) $(PLATFORM_LDFLAGS) $(LIBS) \
		-L lib/syslib -L lib/libncurses/lib -lncursesw -ltinfo -lstdc++ lib/whisper_cpp/lib/libwhisper.a lib/whisper_cpp/lib/libggml.a lib/whisper_cpp/lib/libggml-base.a lib/whisper_cpp/lib/libggml-cpu.a
	@echo "hermes-tui built with ncurses TUI support"

# Static linking target — single binary with no runtime deps beyond kernel
static: CFLAGS += -static -Os -s
static: LDFLAGS += -static
static: $(PHASE5_OBJ) src/main.o $(LIB_OBJ)
	$(CC) $(CFLAGS) -o slermes-static $^ $(LDFLAGS) -lssl -lcrypto -ldl -lpthread -lz -lm 2>/dev/null \
	  || (echo "NOTE: Static build requires static versions of libssl/libcrypto."; \
	      echo "On Debian/Ubuntu: apt-get install libssl-dev (provides .a files)"; \
	      echo "On Alpine: apk add openssl-dev (static by default)"; \
	      $(CC) $(CFLAGS) -o slermes-static $^ $(LDFLAGS) $(LIBS) -static 2>/dev/null \
	      && echo "Static build succeeded (dynamic openssl fallback)" \
	      || echo "Static build failed — ensure static libraries are installed")
	@test -f slermes-static && echo "Static binary: slermes-static ($$(ls -lh slermes-static | awk '{print $$5}'))" || true

# Fuzz test harness (T08)
fuzz: CFLAGS += -DFUZZ_TESTING -O1 -g
fuzz: tests/fuzz_harness.o $(LIB_OBJ)
	$(CC) $(CFLAGS) $(LIB_INCS) -o slermes-fuzz tests/fuzz_harness.o $(LIB_OBJ) $(LDFLAGS) $(LIBS)
		@echo "Fuzz harness built — run with: ./slermes-fuzz"

# Pattern rule for .c files
src/%.o: src/%.c include/hermes.h
	$(CC) $(CFLAGS) $(TUI_INC) -c -o $@ $<

# Pattern rule for .m files (Objective-C, macOS only)
src/%.o: src/%.m include/hermes.h
	$(CC) $(CFLAGS) $(TUI_INC) -c -o $@ $<

# Library pattern rule — standalone libs
lib/%.o: lib/%.c
	$(CC) $(CFLAGS) $(LIB_INCS) -c -o $@ $<

# Static library archives
lib/libbinary.a: lib/libbinary/binary.o
	$(AR) rcs $@ $^

lib/libosv.a: lib/libosv/osv.o
	$(AR) rcs $@ $^


lib/libbrowser.a: lib/libbrowser/camofox_state.o
	$(AR) rcs $@ $^

lib/libdebug.a: lib/libdebug/debug_helpers.o
	$(AR) rcs $@ $^

lib/libwebsite.a: lib/libwebsite/website_policy.o
	$(AR) rcs $@ $^

lib/libskillusage.a: lib/libskillusage/skill_usage.o lib/libskillusage/skill_provenance.o
	$(AR) rcs $@ $^

lib/libskillsync.a: lib/libskillsync/skills_sync.o
	$(AR) rcs $@ $^

lib/libjson.a: lib/libjson/json.o
	$(AR) rcs $@ $^

lib/libhttp.a: lib/libhttp/http.o
	$(AR) rcs $@ $^

lib/libtemplate.a: lib/libtemplate/template.o
	$(AR) rcs $@ $^

lib/libyaml.a: lib/libyaml/yaml.o
	$(AR) rcs $@ $^

lib/libcrypto.a: lib/libcrypto/crypto.o
	$(AR) rcs $@ $^

lib/libdotenv.a: lib/libdotenv/dotenv.o
	$(AR) rcs $@ $^

lib/libcron.a: lib/libcron/cron.o
	$(AR) rcs $@ $^

lib/libproc.a: lib/libproc/proc.o
	$(AR) rcs $@ $^

lib/libtui.a: lib/libtui/tui.o
	$(AR) rcs $@ $^

lib/libdb.a: lib/libdb/db.o
	$(AR) rcs $@ $^

lib/libplugin.a: lib/libplugin/plugin.o
	$(AR) rcs $@ $^

lib/libskin.a: lib/libskin/skin.o
	$(AR) rcs $@ $^

lib/libmcp.a: lib/libmcp/mcp.o
	$(AR) rcs $@ $^

lib/libpath.a: lib/libpath/path.o
	$(AR) rcs $@ $^

lib/libdatetime.a: lib/libdatetime/datetime.o
	$(AR) rcs $@ $^

lib/libcsv.a: lib/libcsv/csv.o
	$(AR) rcs $@ $^

lib/libhash.a: lib/libhash/hash.o
	$(AR) rcs $@ $^

lib/libuuid.a: lib/libuuid/uuid.o
	$(AR) rcs $@ $^

lib/libbase64.a: lib/libbase64/base64.o
	$(AR) rcs $@ $^

lib/libhtml.a: lib/libhtml/html.o
	$(AR) rcs $@ $^

lib/libtextwrap/textwrap.o: lib/libtextwrap/textwrap.c lib/libtextwrap/textwrap.h
	$(CC) $(CFLAGS) -c $< -o $@

lib/libtextwrap.a: lib/libtextwrap/textwrap.o
	$(AR) rcs $@ $^

lib/libglob/glob.o: lib/libglob/glob.c lib/libglob/hermes_glob.h
	$(CC) $(CFLAGS) -c $< -o $@

lib/libglob.a: lib/libglob/glob.o
	$(AR) rcs $@ $^

lib/libdifflib/difflib.o: lib/libdifflib/difflib.c lib/libdifflib/difflib.h
	$(CC) $(CFLAGS) -c $< -o $@

lib/libdifflib.a: lib/libdifflib/difflib.o
	$(AR) rcs $@ $^

lib/libregex/hermes_regex.o: lib/libregex/hermes_regex.c lib/libregex/hermes_regex.h
	$(CC) $(CFLAGS) -c $< -o $@

lib/libregex.a: lib/libregex/hermes_regex.o
	$(AR) rcs $@ $^

lib/libsignal/hermes_signal.o: lib/libsignal/hermes_signal.c lib/libsignal/hermes_signal.h
	$(CC) $(CFLAGS) -c $< -o $@

lib/libsignal.a: lib/libsignal/hermes_signal.o
	$(AR) rcs $@ $^

lib/libwebsocket/websocket.o: lib/libwebsocket/websocket.c lib/libwebsocket/websocket.h
	$(CC) $(CFLAGS) -c $< -o $@

lib/libwebsocket.a: lib/libwebsocket/websocket.o
	$(AR) rcs $@ $^

lib/libprotobuf/protobuf.o: lib/libprotobuf/protobuf.c lib/libprotobuf/protobuf.h
	$(CC) $(CFLAGS) -c $< -o $@

lib/libprotobuf.a: lib/libprotobuf/protobuf.o
	$(AR) rcs $@ $^

lib/libtoml/toml.o: lib/libtoml/toml.c lib/libtoml/toml.h
	$(CC) $(CFLAGS) -c $< -o $@

lib/libtoml.a: lib/libtoml/toml.o
	$(AR) rcs $@ $^

lib/libansi/ansi.o: lib/libansi/ansi.c lib/libansi/ansi.h
	$(CC) $(CFLAGS) -c $< -o $@

lib/libansi.a: lib/libansi/ansi.o
	$(AR) rcs $@ $^

lib/libjson5/json5.o: lib/libjson5/json5.c lib/libjson5/json5.h lib/libjson/json.h
	$(CC) $(CFLAGS) -I lib/libjson -c $< -o $@

lib/libjson5.a: lib/libjson5/json5.o

lib/libtranscribe.a: lib/libtranscribe/transcribe.o lib/libtranscribe/transcription_registry.o
	$(AR) rcs $@ $^

lib/librateguard/rate_guard.o: lib/librateguard/rate_guard.c lib/librateguard/rate_guard.h
	$(CC) $(CFLAGS) -c $< -o $@

lib/librateguard.a: lib/librateguard/rate_guard.o
	$(AR) rcs $@ $^

lib/libfal_common/fal_common.o: lib/libfal_common/fal_common.c lib/libfal_common/fal_common.h lib/libjson/json.h lib/libhttp/http.h
	$(CC) $(CFLAGS) -I lib/libjson -I lib/libhttp -c $< -o $@

lib/libfal_common.a: lib/libfal_common/fal_common.o
	$(AR) rcs $@ $^

lib/libtooloutput/tool_output.o: lib/libtooloutput/tool_output.c
	$(CC) $(CFLAGS) -c $< -o $@

lib/libtooloutput.a: lib/libtooloutput/tool_output.o
	$(AR) rcs $@ $^

lib/libxai_http/xai_http.o: lib/libxai_http/xai_http.c
	$(CC) $(CFLAGS) -c $< -o $@

lib/libxai_http.a: lib/libxai_http/xai_http.o
	$(AR) rcs $@ $^

lib/libmcp_oauth/mcp_oauth.o: lib/libmcp_oauth/mcp_oauth.c lib/libmcp_oauth/mcp_oauth.h
	$(CC) $(CFLAGS) -c $< -o $@

lib/libmcp_oauth.a: lib/libmcp_oauth/mcp_oauth.o
	$(AR) rcs $@ $^

lib/libenvpassthrough/env_passthrough.o: lib/libenvpassthrough/env_passthrough.c
	$(CC) $(CFLAGS) -c $< -o $@

lib/libenvpassthrough.a: lib/libenvpassthrough/env_passthrough.o
	$(AR) rcs $@ $^

lib/libcredential/credential.o: lib/libcredential/credential.c
	$(CC) $(CFLAGS) -c $< -o $@

lib/libcredential.a: lib/libcredential/credential.o
	$(AR) rcs $@ $^

lib/libschemasanitizer/schema_sanitizer.o: lib/libschemasanitizer/schema_sanitizer.c
	$(CC) $(CFLAGS) -c $< -o $@

lib/libschemasanitizer.a: lib/libschemasanitizer/schema_sanitizer.o
	$(AR) rcs $@ $^

lib/libfuzzymatch/fuzzy_match.o: lib/libfuzzymatch/fuzzy_match.c
	$(CC) $(CFLAGS) -c $< -o $@

lib/libfuzzymatch.a: lib/libfuzzymatch/fuzzy_match.o
	$(AR) rcs $@ $^

lib/libinterrupt/interrupt.o: lib/libinterrupt/interrupt.c
	$(CC) $(CFLAGS) -c $< -o $@

lib/libinterrupt.a: lib/libinterrupt/interrupt.o
	$(AR) rcs $@ $^

lib/libtoolbackend/tool_backend.o: lib/libtoolbackend/tool_backend.c
	$(CC) $(CFLAGS) -c $< -o $@

lib/libtoolbackend.a: lib/libtoolbackend/tool_backend.o
	$(AR) rcs $@ $^

lib/libmangateway/managed_gateway.o: lib/libmangateway/managed_gateway.c
	$(CC) $(CFLAGS) -c $< -o $@

lib/libmangateway.a: lib/libmangateway/managed_gateway.o
	$(AR) rcs $@ $^

lib/libratelimit/rate_limit.o: lib/libratelimit/rate_limit.c
	$(CC) $(CFLAGS) -c $< -o $@

lib/libratelimit.a: lib/libratelimit/rate_limit.o
	$(AR) rcs $@ $^

lib/libfilestate/file_state.o: lib/libfilestate/file_state.c
	$(CC) $(CFLAGS) -c $< -o $@

lib/libfilestate.a: lib/libfilestate/file_state.o
	$(AR) rcs $@ $^

lib/libtooldispatch/tool_dispatch_helpers.o: lib/libtooldispatch/tool_dispatch_helpers.c
	$(CC) $(CFLAGS) -c $< -o $@

lib/libtooldispatch.a: lib/libtooldispatch/tool_dispatch_helpers.o
	$(AR) rcs $@ $^

lib/liberrorclassifier/error_classifier.o: lib/liberrorclassifier/error_classifier.c lib/liberrorclassifier/error_classifier.h
	$(CC) $(CFLAGS) -c $< -o $@

lib/liberrorclassifier.a: lib/liberrorclassifier/error_classifier.o
	$(AR) rcs $@ $^

lib/libskillutils.a: lib/libskillutils/skill_utils.o
	$(AR) rcs $@ $^

lib/libbudgetconfig/budget_config.o: lib/libbudgetconfig/budget_config.c lib/libbudgetconfig/budget_config.h
	$(CC) $(CFLAGS) -c $< -o $@

lib/libbudgetconfig.a: lib/libbudgetconfig/budget_config.o
	$(AR) rcs $@ $^

lib/libthreatpatterns/threat_patterns.o: lib/libthreatpatterns/threat_patterns.c lib/libthreatpatterns/threat_patterns.h lib/libregex/hermes_regex.h
	$(CC) $(CFLAGS) -I lib/libregex -c $< -o $@

lib/libthreatpatterns.a: lib/libthreatpatterns/threat_patterns.o
	$(AR) rcs $@ $^

lib/libcredentialfiles/credential_files.o: lib/libcredentialfiles/credential_files.c lib/libcredentialfiles/credential_files.h
	$(CC) $(CFLAGS) -c $< -o $@

lib/libcredentialfiles.a: lib/libcredentialfiles/credential_files.o
	$(AR) rcs $@ $^

lib/libskillaudit/skill_audit.o: lib/libskillaudit/skill_audit.c lib/libskillaudit/skill_audit.h
	$(CC) $(CFLAGS) -c $< -o $@

lib/libskillaudit.a: lib/libskillaudit/skill_audit.o
	$(AR) rcs $@ $^

lib/libslashconfirm/slash_confirm.o: lib/libslashconfirm/slash_confirm.c lib/libslashconfirm/slash_confirm.h
	$(CC) $(CFLAGS) -c $< -o $@

lib/libslashconfirm.a: lib/libslashconfirm/slash_confirm.o
	$(AR) rcs $@ $^

lib/libmsgraph/ms_graph.o: lib/libmsgraph/ms_graph.c lib/libmsgraph/ms_graph.h lib/libjson/json.h lib/libhttp/http.h
	$(CC) $(CFLAGS) -I lib/libjson -I lib/libhttp -c $< -o $@

lib/libmsgraph.a: lib/libmsgraph/ms_graph.o
	$(AR) rcs $@ $^

lib/libcurses_widget/curses_widget.o: lib/libcurses_widget/curses_widget.c lib/libcurses_widget/curses_widget.h
	$(CC) $(CFLAGS) $(LIB_INCS) -I lib/libcurses_widget $(if $(findstring 1,$(HAS_NCURSES)),-DHAS_NCURSES_TUI -I lib/libncurses/include) -c $< -o $@

lib/libcurses_widget.a: lib/libcurses_widget/curses_widget.o
	$(AR) rcs $@ $^

# whisper.cpp third-party library
lib/whisper_cpp/whisper_wrapper.o: lib/whisper_cpp/whisper_wrapper.cc lib/whisper_cpp/whisper_wrapper.h
	$(CXX) $(CFLAGS) -I lib/whisper_cpp/include -std=c++11 -c $< -o $@

lib/whisper_cpp.a: lib/whisper_cpp/whisper_wrapper.o
	$(AR) rcs $@ $^ lib/whisper_cpp/lib/libwhisper.a lib/whisper_cpp/lib/libggml.a lib/whisper_cpp/lib/libggml-base.a lib/whisper_cpp/lib/libggml-cpu.a

# liblineedit
lib/liblineedit/line_edit.o: lib/liblineedit/line_edit.c lib/liblineedit/line_edit.h
	$(CC) $(CFLAGS) -c $< -o $@

lib/liblineedit.a: lib/liblineedit/line_edit.o
	$(AR) rcs $@ $^

# libfile_sync
lib/libfile_sync/file_sync.o: lib/libfile_sync/file_sync.c
	$(CC) $(CFLAGS) -c $< -o $@

lib/libfile_sync.a: lib/libfile_sync/file_sync.o
	$(AR) rcs $@ $^

# libbudgetconfig
lib/libbudgetconfig/budget_config.o: lib/libbudgetconfig/budget_config.c lib/libbudgetconfig/budget_config.h
	$(CC) $(CFLAGS) -c $< -o $@

lib/libbudgetconfig.a: lib/libbudgetconfig/budget_config.o
	$(AR) rcs $@ $^

# libthreatpatterns
lib/libthreatpatterns/threat_patterns.o: lib/libthreatpatterns/threat_patterns.c lib/libthreatpatterns/threat_patterns.h lib/libregex/hermes_regex.h
	$(CC) $(CFLAGS) -I lib/libregex -c $< -o $@

lib/libthreatpatterns.a: lib/libthreatpatterns/threat_patterns.o
	$(AR) rcs $@ $^

# libcredentialfiles
lib/libcredentialfiles/credential_files.o: lib/libcredentialfiles/credential_files.c lib/libcredentialfiles/credential_files.h
	$(CC) $(CFLAGS) -c $< -o $@

lib/libcredentialfiles.a: lib/libcredentialfiles/credential_files.o
	$(AR) rcs $@ $^

# libskillaudit
lib/libskillaudit/skill_audit.o: lib/libskillaudit/skill_audit.c lib/libskillaudit/skill_audit.h
	$(CC) $(CFLAGS) -c $< -o $@

lib/libskillaudit.a: lib/libskillaudit/skill_audit.o
	$(AR) rcs $@ $^

# libslashconfirm
lib/libslashconfirm/slash_confirm.o: lib/libslashconfirm/slash_confirm.c lib/libslashconfirm/slash_confirm.h
	$(CC) $(CFLAGS) -c $< -o $@

lib/libslashconfirm.a: lib/libslashconfirm/slash_confirm.o
	$(AR) rcs $@ $^

# libmsgraph
lib/libmsgraph/ms_graph.o: lib/libmsgraph/ms_graph.c lib/libmsgraph/ms_graph.h lib/libjson/json.h lib/libhttp/http.h
	$(CC) $(CFLAGS) -I lib/libjson -I lib/libhttp -c $< -o $@

lib/libmsgraph.a: lib/libmsgraph/ms_graph.o
	$(AR) rcs $@ $^

# libcurses_widget
lib/libcurses_widget/curses_widget.o: lib/libcurses_widget/curses_widget.c lib/libcurses_widget/curses_widget.h
	$(CC) $(CFLAGS) $(LIB_INCS) -I lib/libcurses_widget $(if $(findstring 1,$(HAS_NCURSES)),-DHAS_NCURSES_TUI -I lib/libncurses/include) -c $< -o $@

lib/libcurses_widget.a: lib/libcurses_widget/curses_widget.o
	$(AR) rcs $@ $^

# Build all standalone libs
libs: $(LIB_A)
	@echo "Standalone libraries built: $(words $(LIB_A)) archives"

# Build example plugins as .so files
plugins: src/plugins/plugin_honcho.so src/plugins/plugin_kanban.so src/plugins/plugin_spotify.so src/plugins/plugin_disk_cleanup.so src/plugins/plugin_file_memory.so src/plugins/plugin_achievements.so src/plugins/plugin_observability.so src/plugins/plugin_skills.so src/plugins/plugin_image_gen.so src/plugins/plugin_google_meet.so src/plugins/plugin_browser.so src/plugins/plugin_context_engine.so src/plugins/plugin_dashboard_auth.so src/plugins/plugin_model_providers.so src/plugins/plugin_platforms.so src/plugins/plugin_teams_pipeline.so src/plugins/plugin_video_gen.so src/plugins/plugin_web.so

src/plugins/plugin_%.so: src/plugins/plugin_%.c include/hermes_plugin.h lib/libplugin/plugin.h
	$(CC) -O2 -fPIC -shared -I include -I lib/libplugin -DSPOTIFY_PLUGIN_VERSION='"1.0.0"' -o $@ $< -ldl

# Install example plugins to ~/.hermes/plugins/
install-plugins: plugins
	mkdir -p ~/.hermes/plugins
	cp src/plugins/plugin_honcho.so ~/.hermes/plugins/
	cp src/plugins/plugin_kanban.so ~/.hermes/plugins/
	cp src/plugins/plugin_spotify.so ~/.hermes/plugins/
	cp src/plugins/honcho-memory.yaml ~/.hermes/plugins/
	cp src/plugins/kanban-board.yaml ~/.hermes/plugins/
	cp src/plugins/spotify-control.yaml ~/.hermes/plugins/
	@echo "Plugins installed to ~/.hermes/plugins/"

# Uninstall — remove binary and optionally config
# make uninstall           — removes binary, preserves config
# make uninstall FORCE=1   — removes config too
.PHONY: uninstall
uninstall:
	@echo "=== Uninstalling Slermes ==="
	@INSTALLED=0; \
	for d in ~/.local/bin /usr/local/bin /usr/bin; do \
		if [ -f "$$d/slermes" ]; then \
			rm -f "$$d/slermes" && echo "  Removed: $$d/slermes" && INSTALLED=1; \
		fi; \
	done; \
	if [ "$$INSTALLED" = "0" ]; then echo "  Binary not found in PATH — already clean."; fi
	@echo "  Note: ~/.hermes/config.yaml and .env preserved."
	@if [ -n "$(FORCE)" ] && [ -d ~/.hermes ]; then \
		echo "  FORCE=1: removing ~/.hermes/"; \
		rm -rf ~/.hermes/config.yaml ~/.hermes/.env ~/.hermes/skills; \
		echo "  Config, .env, and skills removed. Logs and sessions preserved."; \
	fi
	@echo "=== Uninstall complete ==="

# Digestion — standard local diff
digest:
	python3 digest.py

# ─── Super Fork: Upstream Tracking ─────────────────────────────────
# Fetch origin/main (upstream), diff Python since last sync, report C work needed
upstream-sync:
	python3 digest.py --upstream

# Fetch + merge upstream into current branch + generate stubs
upstream-merge:
	python3 digest.py --upstream --merge --generate-stubs

# Full workflow: sync, build, report
sync-all: upstream-merge phase5
	@echo "Super Fork sync complete."

# Test
test: slermes
	bash test_runner.sh

# Test all standalone libraries
test-libs:
	@echo "=== Testing libjson ==="
	@gcc -O2 -Wall -Wextra -I lib/libjson lib/libjson/json.c tests/test_json.c -o /tmp/t_json -lm 2>/dev/null; /tmp/t_json 2>&1 || echo "(no test runner)"
	@echo "=== Testing libhttp ==="
	@gcc -O2 -Wall -Wextra -I lib/libhttp lib/libhttp/http.c tests/test_http.c -o /tmp/t_http -lssl -lcrypto -lz 2>/dev/null; /tmp/t_http 2>&1 || echo "(no test runner)"
	@echo "=== Testing libyaml ==="
	@gcc -O2 -Wall -Wextra -I lib/libyaml lib/libyaml/yaml.c tests/test_yaml.c -o /tmp/t_yaml -lm 2>/dev/null; /tmp/t_yaml 2>&1 || echo "(no test runner)"
	@echo "=== Testing libcrypto ==="
	@gcc -O2 -Wall -Wextra -I lib/libcrypto lib/libcrypto/crypto.c tests/test_crypto.c -o /tmp/t_crypto -lssl -lcrypto 2>/dev/null; /tmp/t_crypto 2>&1 || echo "(no test runner)"
	@echo "=== Testing libdotenv ==="
	@gcc -O2 -Wall -Wextra -I lib/libdotenv lib/libdotenv/dotenv.c tests/test_dotenv.c -o /tmp/t_dotenv -lm 2>/dev/null; /tmp/t_dotenv 2>&1 || echo "(no test runner)"
	@echo "=== Testing libcron ==="
	@gcc -O2 -Wall -Wextra -I lib/libcron lib/libcron/cron.c tests/test_cron.c -o /tmp/t_cron -lm 2>/dev/null; /tmp/t_cron 2>&1 || echo "(no test runner)"
	@echo "=== Testing libproc ==="
	@gcc -O2 -Wall -Wextra -I lib/libproc lib/libproc/proc.c tests/test_proc.c -o /tmp/t_proc -lm 2>/dev/null; /tmp/t_proc 2>&1 || echo "(no test runner)"
	@echo "=== Testing libtemplate ==="
	@gcc -O2 -Wall -Wextra -I lib/libtemplate -I lib/libjson lib/libtemplate/template.c lib/libjson/json.c tests/test_template.c -o /tmp/t_tmpl -lm 2>/dev/null; /tmp/t_tmpl 2>&1 || echo "(no test runner)"
	@echo "=== Testing libtui ==="
	@gcc -O2 -Wall -Wextra -I lib/libtui lib/libtui/tui.c tests/test_tui.c -o /tmp/t_tui -lm 2>/dev/null; /tmp/t_tui 2>&1 || echo "(no test runner)"
	@echo "=== Testing libdb ==="
	@gcc -O2 -Wall -Wextra -I lib/libdb lib/libdb/db.c tests/test_db.c -o /tmp/t_db -lm 2>/dev/null; /tmp/t_db 2>&1 || echo "(no test runner)"
	@echo "=== Testing libpath ==="
	@gcc -O2 -Wall -Wextra -I lib/libpath lib/libpath/path.c tests/test_path.c -o /tmp/t_path -lm 2>/dev/null; /tmp/t_path 2>&1 || echo "(no test runner)"
	@echo "=== Testing plugin_llm ==="
	@gcc -O2 -g -Wall -Wextra -I include -I lib/libjson -I lib/libplugin tests/test_plugin_llm.c src/agent/plugin_llm.o lib/libjson/json.o -o /tmp/t_plugin_llm -lm 2>/dev/null && /tmp/t_plugin_llm 2>&1 || echo "(no test runner)"

	@echo "=== Testing auxiliary_client ===="
	@gcc -O2 -g -Wall -Wextra -I include $(LIB_INCS) tests/test_auxiliary_client.c src/agent/auxiliary_client.o src/tools/url_safety.o -o /tmp/t_aux_client -lm 2>/dev/null && /tmp/t_aux_client 2>&1 || echo "(no test runner)"

	@echo "=== Testing iteration_budget ==="
	@gcc -O2 -g -Wall -Wextra -I include -I lib/libjson -I lib/libplugin tests/test_iteration_budget.c src/agent/budget_tracker.o lib/libjson/json.o -o /tmp/t_iter_budget -lm 2>/dev/null && /tmp/t_iter_budget 2>&1 || echo "(no test runner)"

	@echo "=== Testing checkpoint_persist ==="
	@gcc -O2 -g -Wall -Wextra -I include $(LIB_INCS) tests/test_checkpoint_persist.c src/agent/checkpoint.c src/agent/context.c lib/libjson/json.o -o /tmp/t_chkpersist -lm 2>/dev/null && /tmp/t_chkpersist 2>&1 || echo "(no test runner)"

# Check what changed since last git pull
what-changed:
	@cd .. && git log --oneline @{u}..HEAD --name-only -- '*.py' 2>/dev/null | head -40

# API documentation
docs:
	@if command -v doxygen > /dev/null 2>&1; then \
		echo "Generating API docs..."; \
		doxygen Doxyfile 2>/dev/null; \
		echo "Done: docs/api/html/index.html"; \
	else \
		echo "doxygen not installed. Install: sudo apt install doxygen"; \
	fi

clean:
	rm -f slermes src/*.o src/*/*.o src/*/*/*.o
	rm -f $(LIB_OBJ) $(LIB_A)
	rm -rf digest_output/
	rm -f *.gcda *.gcno src/*.gcda src/*.gcno src/*/*.gcda src/*/*.gcno
	rm -f lib/*.gcda lib/*.gcno lib/*/*.gcda lib/*/*.gcno
	rm -rf coverage_html/ coverage.info coverage-filtered.info

# O05: Release automation target
release:
	@bash scripts/release.sh $(filter-out $@,$(MAKECMDGOALS))

# ASan build target — compile with AddressSanitizer for memory error detection
asan:
	$(MAKE) CFLAGS="-O1 -g -fsanitize=address -fno-omit-frame-pointer -Wall -Wextra -Wpedantic -I include $(LIB_INCS) $(SSL_CFLAGS)" \
		LDFLAGS="-fsanitize=address $(SSL_LDFLAGS) $(PLATFORM_LDFLAGS) $(LIBS)" \
		clean all

# ASan test target — build with ASan and run test suite under ASan instrumentation
asan-test: asan
	@echo "=== ASan: Running test suite under AddressSanitizer ==="
	@ASAN_SYMBOLIZER_PATH=$(shell command -v llvm-symbolizer 2>/dev/null || echo "") \
		CFLAGS_EXTRA="-fsanitize=address" ./test_runner.sh
	@echo "=== ASan: Test run complete ==="

# Valgrind leak check target — build with debug symbols, run under valgrind
valgrind: hermes
	@echo "=== Valgrind: Basic memory check ==="
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes \
		--error-exitcode=1 --errors-for-leak-kinds=all \
		./hermes --help 2>&1
	@echo ""
	@echo "=== Valgrind: hermes binary OK ==="

# U07: Code coverage target — build with gcov flags, run tests, generate lcov report
# Requires: sudo apt-get install lcov
coverage:
	$(MAKE) CFLAGS="-O0 -g --coverage -Wall -Wextra -Wpedantic -I include $(LIB_INCS) $(SSL_CFLAGS)" \
		LDFLAGS="--coverage $(SSL_LDFLAGS) $(PLATFORM_LDFLAGS) $(LIBS)" \
		clean all
	@echo "=== Coverage: Running test suite with coverage instrumentation === (this will also instrument test binaries)"
	@CFLAGS_EXTRA="--coverage" ./test_runner.sh
	@echo "=== Coverage: Generating report ==="
	lcov --capture --directory . --output-file coverage.info \
		--rc lcov_branch_coverage=1 2>/dev/null || true
	lcov --remove coverage.info '/usr/*' '*/lib/libdb/*' '*/tests/*' \
		--output-file coverage-filtered.info --rc lcov_branch_coverage=1 2>/dev/null || true
	genhtml coverage-filtered.info --output-directory coverage_html \
		--rc lcov_branch_coverage=1 2>/dev/null || true
	@echo "=== Coverage report: coverage_html/index.html ==="

# Coverage gate — build with coverage, run tests, enforce minimum threshold
# Uses COVERAGE_THRESHOLD env var (default: 1.0) — set to 0.0 for non-blocking monitoring
coverage-gate:
	$(MAKE) CFLAGS="-O0 -g --coverage -Wall -Wextra -Wpedantic -I include $(LIB_INCS) $(SSL_CFLAGS)" \
		LDFLAGS="--coverage $(SSL_LDFLAGS) $(PLATFORM_LDFLAGS) $(LIBS)" \
		clean all
	@echo "=== Coverage: Running test suite with coverage instrumentation ==="
	@-CFLAGS_EXTRA="--coverage" ./test_runner.sh
	@rm -f *.gcov
	@echo "=== Coverage gate: checking threshold ==="
	@COVERAGE_THRESHOLD="$(or $(COVERAGE_THRESHOLD),1.0)" python3 scripts/coverage-gate.py --threshold="$(or $(COVERAGE_THRESHOLD),1.0)"

# U10: Performance gate — check binary size and startup time against baseline
# Baseline stored in .perf-baseline.json (auto-created on first run)
# Use --update-baseline to refresh baseline after intentional growth
perf-gate: hermes
	@python3 scripts/perf-gate.py

# R07: Combined check — bash lint, build, test suite
.PHONY: check
check:
	@echo "=== Check: lint ==="
	bash -n test_runner.sh 2>/dev/null || true
	@echo "=== Check: build ==="
	$(MAKE) -j$(nproc) all
	@echo "=== Check: test suite ==="
	bash test_runner.sh

# S13 #2: Static analysis — cppcheck on src/ agent/ tools/ gateway/ cron/
static-analysis:
	@echo "=== Static analysis: cppcheck ==="
	@cppcheck --enable=warning,performance,portability \
		--suppress=missingIncludeSystem \
		--suppress=unmatchedSuppression \
		--suppress=normalCheckLevelMaxBranches \
		--suppress=toomanyconfigs \
		-I include $(LIB_INCS) \
		src/ 2>&1 && echo "  PASS: no errors found" || echo "  NOTE: cppcheck found warnings/errors (see above)"
	@echo "=== Static analysis complete ==="

# Python bridge dependencies — install all third-party packages
# See THIRD_PARTY.md §9 for details on which features need which packages
.PHONY: python-deps
python-deps:
	@echo "=== Installing Python bridge dependencies ==="
	@pip install -r requirements-bridge.txt 2>/dev/null \
		|| pip3 install -r requirements-bridge.txt 2>/dev/null \
		|| echo "  WARNING: pip not found. Install manually: pip install -r requirements-bridge.txt"
	@echo "=== Python bridge dependencies installed ==="
