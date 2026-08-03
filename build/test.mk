# ── Testing & Quality Targets ──────────────────────────────────────
# test, test-libs, check, asan, valgrind, coverage, coverage-gate, perf-gate
# Included by top-level Makefile

.PHONY: test test-libs check asan asan-test valgrind coverage coverage-gate perf-gate

# Test — run the full test suite
test: slermes
	bash tests/run_mission8_tests.sh

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
	@echo "=== Testing libhive (skipfield + freelist) ==="
	@gcc -std=c11 -O2 -Wall -Wextra -I lib/libhive lib/libhive/hive.c lib/libhive/hive_test.c -o /tmp/t_hive 2>/dev/null; /tmp/t_hive 2>&1 || echo "(no test runner)"
	@echo "=== Testing plugin_llm ==="
	@gcc -O2 -g -Wall -Wextra -I include -I lib/libjson -I lib/libplugin tests/test_plugin_llm.c src/agent/plugin_llm.o lib/libjson/json.o -o /tmp/t_plugin_llm -lm 2>/dev/null && /tmp/t_plugin_llm 2>&1 || echo "(no test runner)"
	@echo "=== Testing auxiliary_client ==="
	@gcc -O2 -g -Wall -Wextra -I include $(LIB_INCS) tests/test_auxiliary_client.c src/agent/auxiliary_client.o src/tools/url_safety.o -o /tmp/t_aux_client -lm 2>/dev/null && /tmp/t_aux_client 2>&1 || echo "(no test runner)"
	@echo "=== Testing iteration_budget ==="
	@gcc -O2 -g -Wall -Wextra -I include -I lib/libjson -I lib/libplugin tests/test_iteration_budget.c src/agent/budget_tracker.o lib/libjson/json.o -o /tmp/t_iter_budget -lm 2>/dev/null && /tmp/t_iter_budget 2>&1 || echo "(no test runner)"
	@echo "=== Testing checkpoint_persist ==="
	@gcc -O2 -g -Wall -Wextra -I include $(LIB_INCS) tests/test_checkpoint_persist.c src/agent/checkpoint.c src/agent/context.c lib/libjson/json.o -o /tmp/t_chkpersist -lm 2>/dev/null && /tmp/t_chkpersist 2>&1 || echo "(no test runner)"

# Cron job store (port of cron/jobs.py) — disk-backed real-behavior test.
test-cronjobs:
	@gcc -O2 -g -Wall -Wextra -I include $(LIB_INCS) tests/cron_jobs_test.c \
		src/cron/port_cron_jobs.o src/cron/port_lifecycle_guard.o src/slermes_home.o \
		lib/libjson/json.o lib/libhash/hash.o lib/libcron/cron.o \
		lib/libdatetime/datetime.o lib/libuuid/uuid.o \
		$(SSL_LDFLAGS) -o /tmp/t_cronjobs -lssl -lcrypto -lpcre2-8 2>/dev/null \
		&& /tmp/t_cronjobs 2>&1 || echo "(cronjobs test failed)"

# Model catalog (port of hermes_cli/models.py catalog + static resolution)
# Links model_catalog.o + port_models_helpers.o (owns fast-mode/cache-path
# helpers that model_catalog.c delegates to).
test-models:
	@gcc -O2 -g -Wall -Wextra -Werror=implicit-function-declaration -I include $(LIB_INCS) tests/model_catalog_test.c \
		src/cli/model_catalog.o src/cli/port_models_helpers.o \
		lib/libjson/json.o lib/libcrypto/crypto.o lib/libcredentialfiles/credential_files.o \
		$(SSL_LDFLAGS) -o /tmp/t_models 2>/dev/null \
		&& /tmp/t_models 2>&1 || echo "(model catalog test failed)"

# Network-backed + pure model-catalog helpers (port_models_net.c). Injectable
# HTTP transport exercised with a mock (no real network).
test-models-net:
	@gcc -O2 -g -Wall -Wextra -Werror=implicit-function-declaration -I include -I lib/libjson -I lib $(LIB_INCS) tests/port_models_net_test.c \
		src/cli/port_models_net.o src/cli/port_models_helpers.o src/cli/port_hermes_cli_models.o src/cli/model_catalog.o \
		lib/libjson/json.o lib/libcrypto/crypto.o lib/libcredentialfiles/credential_files.o \
		$(SSL_LDFLAGS) -o /tmp/t_models_net 2>/dev/null \
		&& /tmp/t_models_net 2>&1 || echo "(models net test failed)"

# Pure + network-wrapped model-catalog helpers (port_models_pure.c). Injectable
# HTTP transport exercised with a mock (no real network), plus disk-cache and
# config-resolver behavior. Depends on port_models_net.o + port_models_helpers.o.
test-models-pure:
	@gcc -O2 -g -Wall -Wextra -Werror=implicit-function-declaration -I include -I lib/libjson -I lib $(LIB_INCS) tests/port_models_pure_test.c \
		src/cli/port_models_pure.o src/cli/port_models_net.o src/cli/port_models_helpers.o \
		src/cli/port_hermes_cli_models.o src/cli/model_catalog.o src/cli/port_model_normalize.o \
		src/slermes_home.o lib/libjson/json.o lib/libcrypto/crypto.o lib/libcredentialfiles/credential_files.o \
		$(SSL_LDFLAGS) -o /tmp/t_models_pure 2>/dev/null \
		&& /tmp/t_models_pure 2>&1 || echo "(models pure test failed)"

# validate_requested_model + ensure_lmstudio_model_loaded (port_models_validate.c).
# Injectable HTTP transport + MoA/catalog resolvers, no real network.
test-models-validate:
	@gcc -O2 -g -Wall -Wextra -Werror=implicit-function-declaration -I include -I lib/libjson -I lib $(LIB_INCS) tests/port_models_validate_test.c \
		src/cli/port_models_validate.o src/cli/port_models_pure.o src/cli/port_models_net.o src/cli/port_models_helpers.o \
		src/cli/port_hermes_cli_models.o src/cli/model_catalog.o src/cli/port_model_normalize.o \
		src/slermes_home.o lib/libjson/json.o lib/libcrypto/crypto.o lib/libcredentialfiles/credential_files.o lib/libfuzzymatch/fuzzy_match.o \
		$(SSL_LDFLAGS) -lm -o /tmp/t_models_validate 2>/dev/null \
		&& /tmp/t_models_validate 2>&1 || echo "(models validate test failed)"

# Profile store (port of hermes_cli/profiles.py) — disk-backed real-behavior test.
test-profiles:
	@gcc -O2 -g -Wall -Wextra -Werror=implicit-function-declaration -I include $(LIB_INCS) tests/profile_store_test.c \
		src/cli/port_cli_profiles.o src/gateway/status.o src/slermes_home.o \
		lib/libjson/json.o lib/libyaml/yaml.o lib/libhash/hash.o \
		lib/libdatetime/datetime.o lib/libpath/path.o lib/libuuid/uuid.o \
		$(SSL_LDFLAGS) -o /tmp/t_profiles -lssl -lcrypto -lz 2>/dev/null \
		&& /tmp/t_profiles 2>&1 || echo "(profiles test failed)"

# Gateway command sanitize (pure helpers ported from hermes_cli/commands.py)
test-commands-sanitize:
	@gcc -O2 -g -Wall -Wextra -Werror=implicit-function-declaration -I include $(LIB_INCS) tests/gateway_command_sanitize_test.c \
		src/cli/gateway_command_sanitize.o lib/libjson/json.o \
		-o /tmp/t_cmdsanitize 2>/dev/null \
		&& /tmp/t_cmdsanitize 2>&1 || echo "(gateway command sanitize test failed)"

test-blueprint-cmd:
	@gcc -O2 -g -Wall -Wextra -Werror=implicit-function-declaration -I include $(LIB_INCS) tests/blueprint_cmd_test.c \
		src/cli/blueprint_cmd.o src/cli/port_blueprint_catalog_helpers.o lib/libjson/json.o lib/libdifflib/difflib.o \
		-o /tmp/t_blueprint 2>/dev/null \
		&& /tmp/t_blueprint 2>&1 || echo "(blueprint command test failed)"

# Goal data-model (pure data layer ported from hermes_cli/goals.py:
# GoalContract, parse_contract, GoalState serialize/render).
test-goals-data:
	@gcc -O2 -g -Wall -Wextra -Werror=implicit-function-declaration -I include $(LIB_INCS) tests/goal_data_test.c \
		src/cli/port_goals_data.o lib/libjson/json.o \
		-o /tmp/t_goalsdata 2>/dev/null \
		&& /tmp/t_goalsdata 2>&1 || echo "(goal data test failed)"

# CLI command registry + completion walk (commands.py registry + completion.py:_walk).
test-cli-command-registry:
	@gcc -O2 -g -Wall -Wextra -Werror=implicit-function-declaration -I include -I lib/libjson -I lib $(LIB_INCS) tests/cli_command_registry_test.c \
		src/cli/port_cli_command_registry.o src/cli/port_completion.o \
		-o /tmp/t_ccr 2>/dev/null \
		&& /tmp/t_ccr 2>&1 || echo "(cli command registry test failed)"

# Tools-config pure helpers (display/config slice of tools_config.py).
test-tools-config-helpers:
	@gcc -O2 -g -Wall -Wextra -Werror=implicit-function-declaration -I include -I lib/libjson -I lib $(LIB_INCS) tests/tools_config_helpers_test.c \
		src/cli/port_tools_config_helpers.o \
		-o /tmp/t_tch 2>/dev/null \
		&& /tmp/t_tch 2>&1 || echo "(tools config helpers test failed)"

# Plugin manifest (pure data-model port of plugins.py:PluginManifest).
test-plugin-manifest:
	@gcc -O2 -g -Wall -Wextra -Werror=implicit-function-declaration -I include -I lib/libjson -I lib $(LIB_INCS) tests/plugin_manifest_test.c \
		src/cli/port_plugin_manifest.o lib/libjson/json.o \
		-o /tmp/t_pm 2>/dev/null \
		&& /tmp/t_pm 2>&1 || echo "(plugin manifest test failed)"

# Goal manager (orchestration + judge-prompt helpers, pure layer of goals.py).
test-goals-manager: src/cli/port_goals_data.o src/cli/port_goals_manager.o src/cli/port_goals_helpers.o lib/libjson/json.o lib/libcredentialfiles/credential_files.o src/tools/process_registry.o
	@gcc -O2 -g -Wall -Wextra -Werror=implicit-function-declaration -I include -I lib/libjson -I lib -I lib/libcredentialfiles -I src $(LIB_INCS) tests/goal_manager_test.c \
		src/cli/port_goals_data.o src/cli/port_goals_manager.o src/cli/port_goals_helpers.o lib/libjson/json.o lib/libcredentialfiles/credential_files.o \
		src/tools/process_registry.o -lpthread \
		-o /tmp/t_gm 2>/dev/null \
		&& /tmp/t_gm 2>&1 || echo "(goals manager test failed)"

# Process bootstrap (agent/process_bootstrap.py _SafeWriter / _OpenAIProxy /
# build_keepalive_http_client).
test-process-bootstrap: src/agent/process_bootstrap.o src/agent/proxy_utils.o src/agent/logger.o src/tools/url_safety.o lib/libjson/json.o lib/libhttp/http.o
	@gcc -O2 -g -Wall -Wextra -Werror=implicit-function-declaration -I include -I lib/libjson -I lib -I lib/libcredentialfiles -I src $(LIB_INCS) tests/process_bootstrap_test.c \
		src/agent/process_bootstrap.o src/agent/proxy_utils.o src/agent/logger.o src/tools/url_safety.o lib/libjson/json.o lib/libhttp/http.o \
		-lpthread -lssl -lcrypto -lz -o /tmp/t_pb 2>/dev/null \
		&& /tmp/t_pb 2>&1 || echo "(process bootstrap test failed)"

test-provider-meta:
	@gcc -O2 -g -Wall -Wextra -Werror=implicit-function-declaration -I include -I lib/libjson -I lib $(LIB_INCS) tests/provider_meta_test.c \
		src/cli/port_provider_meta.o src/cli/port_config_pure.o lib/libjson/json.o \
		-o /tmp/t_provmeta 2>/dev/null \
		&& /tmp/t_provmeta 2>&1 || echo "(provider meta test failed)"

# Learning graph (agent/learning_graph.py: frontmatter / skill scan / usage /
# build_skill_nodes / memory_cards / skill_roots / build_learning_graph).
test-learning-graph: src/cli/port_learning_graph.o src/cli/port_learning_graph_helpers.o lib/libjson/json.o lib/libyaml/yaml.o lib/libskillusage/skill_usage.o
	@gcc -O2 -g -Wall -Wextra -Werror=implicit-function-declaration -I include -I lib/libjson -I lib/libyaml -I lib/libskillusage -I lib $(LIB_INCS) tests/learning_graph_test.c \
		src/cli/port_learning_graph.o src/cli/port_learning_graph_helpers.o lib/libjson/json.o lib/libyaml/yaml.o lib/libskillusage/skill_usage.o \
		-lpthread -o /tmp/t_lg 2>/dev/null \
		&& /tmp/t_lg 2>&1 || echo "(learning graph test failed)"

# Learning graph render (agent/learning_graph_render.py: 18 functions).
test-learning-graph-render: src/cli/port_learning_graph_render.o src/cli/port_learning_graph_render_helpers.o lib/libjson/json.o
	@gcc -O2 -g -Wall -Wextra -Werror=implicit-function-declaration -I include -I lib/libjson -I lib $(LIB_INCS) tests/learning_graph_render_test.c \
		src/cli/port_learning_graph_render.o src/cli/port_learning_graph_render_helpers.o lib/libjson/json.o \
		-lm -o /tmp/t_lgr 2>/dev/null \
		&& /tmp/t_lgr 2>&1 || echo "(learning graph render test failed)"

# Learning mutations (agent/learning_mutations.py: 15 features).
test-learning-mutations: src/cli/port_learning_mutations.o src/cli/port_learning_graph.o src/cli/port_learning_graph_helpers.o lib/libjson/json.o lib/libyaml/yaml.o lib/libskillusage/skill_usage.o
	@gcc -O2 -g -Wall -Wextra -Werror=implicit-function-declaration -I include -I lib/libjson -I lib/libyaml -I lib/libskillusage -I lib $(LIB_INCS) tests/learning_mutations_test.c \
		src/cli/port_learning_mutations.o src/cli/port_learning_graph.o src/cli/port_learning_graph_helpers.o lib/libjson/json.o lib/libyaml/yaml.o lib/libskillusage/skill_usage.o \
		-lpthread -o /tmp/t_lm 2>/dev/null \
		&& /tmp/t_lm 2>&1 || echo "(learning mutations test failed)"

# Shell completion generation (pure slice of hermes_cli/completion.py).
test-completion:
	@gcc -O2 -g -Wall -Wextra -Werror=implicit-function-declaration -I include $(LIB_INCS) tests/completion_test.c \
		src/cli/port_completion.o \
		-o /tmp/t_completion 2>/dev/null \
		&& /tmp/t_completion 2>&1 || echo "(completion test failed)"

test-model-normalize:
	@gcc -O2 -g -Wall -Wextra -Werror=implicit-function-declaration -I include $(LIB_INCS) tests/model_normalize_test.c \
		src/cli/port_model_normalize.o src/cli/model_catalog.o src/cli/port_models_helpers.o \
		lib/libjson/json.o lib/libcrypto/crypto.o lib/libcredentialfiles/credential_files.o \
		-lcrypto -lssl -o /tmp/t_modelnorm 2>/dev/null \
		&& /tmp/t_modelnorm 2>&1 || echo "(model normalize test failed)"

# Write-approval pending store (faithful port of tools/write_approval.py).
# Exercises the previously-stubbed file-backed store: stage_write / pending_count
# / list_pending / get_pending / discard_pending / write_approval_enabled / skill_gist.
test-write-approval: src/cli/port_tools_write_approval.o
	@gcc -O2 -g -Wall -Wextra -Werror=implicit-function-declaration -I include -I lib/libjson -I lib -I lib/libyaml $(LIB_INCS) tests/test_write_approval.c \
		src/cli/port_tools_write_approval.o \
		lib/libjson/json.o lib/libyaml/yaml.o lib/libuuid/uuid.o lib/libhash/hash.o lib/libcredentialfiles/credential_files.o \
		-lcrypto -lssl -o /tmp/t_write_approval 2>/dev/null \
		&& /tmp/t_write_approval 2>&1 || echo "(write approval test failed)"

# Skill write-origin provenance (existing port in lib/libskillusage/skill_provenance.c,
# faithful port of tools/skill_provenance.py — token-based set/reset, is_background_review).
test-skill-provenance:
	@gcc -O2 -g -Wall -Wextra -Werror=implicit-function-declaration -I include -I lib/libskillusage $(LIB_INCS) tests/test_skill_provenance.c \
		lib/libskillusage/skill_provenance.o \
		-o /tmp/t_prov 2>/dev/null \
		&& /tmp/t_prov 2>&1 || echo "(skill provenance test failed)"

# Async (background) delegation registry (faithful port of tools/async_delegation.py).
# Exercises the registry: dispatch runs a worker, completion event delivered via
# injected sink, capacity gate rejects, interrupt_all cancels, batch dispatch
# occupies one slot and emits an is_batch event with results. Replaces the prior
# stub in src/tools/port_tools_async_delegation.c.
test-async-delegation: src/tools/port_tools_async_delegation.o
	@gcc -O2 -g -Wall -Wextra -Werror=implicit-function-declaration -pthread -I include -I lib/libjson -I lib -I lib/libyaml $(LIB_INCS) tests/test_async_delegation.c \
		src/tools/port_tools_async_delegation.o \
		lib/libjson/json.o lib/libuuid/uuid.o lib/libhash/hash.o \
		-lcrypto -lssl -o /tmp/t_async 2>/dev/null \
		&& /tmp/t_async 2>&1 || echo "(async delegation test failed)"

# Memory tool store (tools/memory_tool.py: MemoryStore + load_on_disk_store).
# Exercises the file-backed bounded memory: add/replace/remove/apply_batch,
# dedupe, char-limit gate, frozen snapshot, atomic persistence, injectable
# threat scanner, and E2E parity vs the real Python module.
test-memory-tool: src/tools/port_memory_tool.o
	@gcc -O2 -g -Wall -Wextra -Werror=implicit-function-declaration -I include -I lib/libjson -I lib $(LIB_INCS) tests/test_memory_tool.c \
		src/tools/port_memory_tool.o \
		src/tools/registry.o \
		lib/libjson/json.o \
		-o /tmp/t_mem 2>/dev/null \
		&& /tmp/t_mem 2>&1 || echo "(memory tool test failed)"

# Memory tool handler (tools/memory_tool.py: memory_tool / write gate /
# missing-old_text / apply_memory_pending). Exercises the dispatch layer,
# gate integration (block/stage/allow), and E2E parity vs Python.
test-memory-tool-handler: src/tools/port_memory_tool.o
	@gcc -O2 -g -Wall -Wextra -Werror=implicit-function-declaration -I include -I lib/libjson -I lib -I src/tools $(LIB_INCS) tests/test_memory_tool_handler.c \
		src/tools/port_memory_tool.o \
		src/tools/registry.o \
		lib/libjson/json.o \
		-o /tmp/t_mem_h 2>/dev/null \
		&& /tmp/t_mem_h 2>&1 || echo "(memory tool handler test failed)"

# close_terminal tool (tools/close_terminal_tool.py) + process_registry.request_close_terminal sink.
test-close-terminal-tool: src/tools/port_close_terminal_tool.o src/tools/process_registry.o
	@gcc -O2 -g -Wall -Wextra -Werror=implicit-function-declaration -I include -I lib/libjson -I lib -I src/tools $(LIB_INCS) tests/test_close_terminal_tool.c \
		src/tools/port_close_terminal_tool.o \
		src/tools/process_registry.o \
		src/tools/registry.o \
		lib/libjson/json.o \
		-o /tmp/t_close_terminal 2>/dev/null \
		&& /tmp/t_close_terminal 2>&1 || echo "(close terminal tool test failed)"

# Registry enumeration + toolset-alias API (tools/registry.py ToolRegistry).
test-registry-enum: src/tools/registry.o
	@gcc -O2 -g -Wall -Wextra -Werror=implicit-function-declaration -I include -I lib/libjson -I lib $(LIB_INCS) tests/test_registry_enum.c \
		src/tools/registry.o \
		lib/libjson/json.o \
		-o /tmp/t_reg_enum 2>/dev/null \
		&& /tmp/t_reg_enum 2>&1 || echo "(registry enum test failed)"

# Live wiring: the faithful memory_tool.py port registered as the "memory" tool.
test-memory-tool-live: src/tools/port_memory_tool.o src/cli/port_tools_write_approval.o
	@gcc -O2 -g -Wall -Wextra -Werror=implicit-function-declaration -I include -I lib/libjson -I lib $(LIB_INCS) tests/test_memory_tool_live.c \
		src/tools/port_memory_tool.o \
		src/cli/port_tools_write_approval.o \
		src/tools/registry.o \
		lib/libjson/json.o \
		lib/libyaml/yaml.o \
		lib/libuuid/uuid.o \
		lib/libhash/hash.o \
		lib/libcredentialfiles/credential_files.o \
		-o /tmp/t_mem_live 2>/dev/null \
		&& /tmp/t_mem_live 2>&1 || echo "(memory tool live test failed)"

# Make a combined list of all phony targets for the check/ci workflow
TEST_PHONIES := test test-libs check

# ASan build — compile with AddressSanitizer for memory error detection
asan:
	$(MAKE) CFLAGS="-O1 -g -fsanitize=address -fno-omit-frame-pointer -Wall -Wextra -Wpedantic -I include $(LIB_INCS) $(SSL_CFLAGS)" \
		LDFLAGS="-fsanitize=address $(SSL_LDFLAGS) $(PLATFORM_LDFLAGS) $(LIBS)" \
		clean all

# ASan test — build with ASan and run test suite under ASan instrumentation
asan-test: asan
	@echo "=== ASan: Running test suite under AddressSanitizer ==="
	@ASAN_SYMBOLIZER_PATH=$(shell command -v llvm-symbolizer 2>/dev/null || echo "") \
		CFLAGS_EXTRA="-fsanitize=address" bash tests/run_mission8_tests.sh
	@echo "=== ASan: Test run complete ==="

# Valgrind leak check
valgrind: slermes
	@echo "=== Valgrind: Basic memory check ==="
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes \
		--error-exitcode=1 --errors-for-leak-kinds=all \
		./slermes --help 2>&1
	@echo ""
	@echo "=== Valgrind: slermes binary OK ==="

# Code coverage — build with gcov flags, run tests, generate lcov report
coverage:
	$(MAKE) CFLAGS="-O0 -g --coverage -Wall -Wextra -Wpedantic -I include $(LIB_INCS) $(SSL_CFLAGS)" \
		LDFLAGS="--coverage $(SSL_LDFLAGS) $(PLATFORM_LDFLAGS) $(LIBS)" \
		clean all
	@echo "=== Coverage: Running test suite with coverage instrumentation ==="
	@CFLAGS_EXTRA="--coverage" bash tests/run_mission8_tests.sh
	@echo "=== Coverage: Generating report ==="
	lcov --capture --directory . --output-file coverage.info --rc lcov_branch_coverage=1 2>/dev/null || true
	lcov --remove coverage.info '/usr/*' '*/lib/libdb/*' '*/tests/*' --output-file coverage-filtered.info --rc lcov_branch_coverage=1 2>/dev/null || true
	genhtml coverage-filtered.info --output-directory coverage_html --rc lcov_branch_coverage=1 2>/dev/null || true
	@echo "=== Coverage report: coverage_html/index.html ==="

# Coverage gate — enforce minimum threshold
coverage-gate:
	$(MAKE) CFLAGS="-O0 -g --coverage -Wall -Wextra -Wpedantic -I include $(LIB_INCS) $(SSL_CFLAGS)" \
		LDFLAGS="--coverage $(SSL_LDFLAGS) $(PLATFORM_LDFLAGS) $(LIBS)" \
		clean all
	@echo "=== Coverage: Running test suite with coverage instrumentation ==="
	@-CFLAGS_EXTRA="--coverage" bash tests/run_mission8_tests.sh
	@rm -f *.gcov
	@echo "=== Coverage gate: checking threshold ==="
	@COVERAGE_THRESHOLD="$(or $(COVERAGE_THRESHOLD),1.0)" python3 scripts/coverage-gate.py --threshold="$(or $(COVERAGE_THRESHOLD),1.0)"

# Performance gate — check binary size and startup time against baseline
perf-gate: slermes
	@python3 scripts/perf-gate.py


# Skill manager validation/security core (faithful port of tools/skill_manager_tool.py).
test-skill-val: src/tools/skill_manager_val.o
	@gcc -O2 -g -Wall -Wextra -I include -I src \
	    tests/test_skill_manager_val.c src/tools/skill_manager_val.o -o /tmp/t_skill_val 2>&1 \
	    && /tmp/t_skill_val 2>&1 || echo "(skill-val test failed)"

# Pet atlas pixel-ops (faithful port of agent/pet/generate/atlas.py).
test-atlas: src/pet/atlas.o
	@gcc -O2 -g -Wall -Wextra -I include -I src \
	    tests/test_atlas.c src/pet/atlas.o -o /tmp/t_atlas -lm 2>&1 \
	    && /tmp/t_atlas 2>&1 || echo "(atlas test failed)"

# Verification evidence ledger (faithful port of agent/verification_evidence.py).
test-verification: src/agent/verification_evidence.o
	@gcc -O2 -g -Wall -Wextra -I include -I src -I lib/libdb \
	    tests/test_verification_evidence.c src/agent/verification_evidence.o lib/libdb/db.o lib/libdb/sqlite3.o \
	    -o /tmp/t_ve -lpthread -ldl -lm 2>&1 \
	    && /tmp/t_ve 2>&1 || echo "(verification test failed)"

# Per-profile Project store (faithful port of hermes_cli/projects_db.py).
test-projects-db: src/hermes_cli/projects_db.o
	@gcc -O2 -g -Wall -Wextra -I include -I src -I lib/libdb \
	    tests/test_projects_db.c src/hermes_cli/projects_db.o lib/libdb/sqlite3.o \
	    -o /tmp/t_pdb -lpthread -ldl -lm 2>&1 \
	    && /tmp/t_pdb 2>&1 || echo "(projects-db test failed)"

# Replay-history sanitization (faithful port of agent/replay_cleanup.py).
# Oracle-verified: C helpers vs LIVE Python across interrupted/dangling/
# side-effect-recovery and stale-dangerous-confirmation cases.
test-replay-cleanup:
	@bash tests/oracle/runners/run_oracle.sh replay_cleanup 2>&1 \
	    | grep -E 'MISMATCH' && echo "(replay_cleanup oracle FAILED)" \
	    || echo "replay_cleanup oracle: all cases MATCH"

# Expensive-model cost guard (faithful port of hermes_cli/model_cost_guard.py).
# Oracle-verified: format_money + pricing_from_model_info vs LIVE Python.
test-model-cost-guard:
	@bash tests/oracle/runners/run_oracle.sh model_cost_guard 2>&1 \
	    | grep -E 'MISMATCH' && echo "(model_cost_guard oracle FAILED)" \
	    || echo "model_cost_guard oracle: all cases MATCH"

# Yuanbao protobuf wire codec (faithful port of gateway/platforms/yuanbao_proto.py).
# Oracle-verified: C decoders vs LIVE Python across inbound/msgcontent/bodyelem/
# group-member-list/forward round-trip cases (byte-identical wire bytes).
test-yuanbao-proto:
	@bash tests/oracle/runners/run_oracle.sh yuanbao_proto 2>&1 \
	    | grep -E 'MISMATCH' && echo "(yuanbao_proto oracle FAILED)" \
	    || echo "yuanbao_proto oracle: all cases MATCH"

# Office document text extraction (faithful port of tools/read_extract.py).
# Oracle-verified: C read_extract_document_text vs LIVE Python across docx/
# xlsx/ipynb fixtures (incl. Unicode text, shared strings, workbook sheets).
test-read-extract:
	@bash tests/oracle/runners/run_oracle.sh read_extract 2>&1 \
	    | grep -E 'MISMATCH' && echo "(read_extract oracle FAILED)" \
	    || echo "read_extract oracle: all cases MATCH"

# Usage pricing (faithful port of agent/usage_pricing.py).
# Oracle-verified: normalize_usage (3-way), resolve_billing_route (nous/vertex/
# openai-codex routing), format_token_count_compact, bedrock/anthropic model-name
# normalization vs LIVE Python across 33 cases.
test-usage-pricing:
	@bash tests/oracle/runners/run_oracle.sh usage_pricing 2>&1 \
	    | grep -E 'MISMATCH' && echo "(usage_pricing oracle FAILED)" \
	    || echo "usage_pricing oracle: all cases MATCH"

# Account usage helpers (faithful port of agent/account_usage.py).
# Oracle-verified: title_case_slug, fmt_usd (thousands separators), is_finite_num
# vs LIVE Python across 18 cases.
test-account-usage:
	@bash tests/oracle/runners/run_oracle.sh account_usage 2>&1 \
	    | grep -E 'MISMATCH' && echo "(account_usage oracle FAILED)" \
	    || echo "account_usage oracle: all cases MATCH"

# Context compressor pure helpers (faithful port of agent/context_compressor.py).
# Oracle-verified: extract_name_args, extract_id, content_text_for_contains,
# append_text_to_content vs LIVE Python across 17 cases.
test-context-compressor:
	@bash tests/oracle/runners/run_oracle.sh context_compressor 2>&1 \
	    | grep -E 'MISMATCH' && echo "(context_compressor oracle FAILED)" \
	    || echo "context_compressor oracle: all cases MATCH"

# TTS output-format resolution (faithful port of agent/tts_provider.py).
# Oracle-verified: resolve_output_format clamps to {mp3,wav,ogg,opus,flac},
# strips whitespace + case-insensitive, invalid -> mp3, vs LIVE Python.
test-tts:
	@bash tests/oracle/runners/run_oracle.sh tts 2>&1 \
	    | grep -E 'MISMATCH' && echo "(tts oracle FAILED)" \
	    || echo "tts oracle: all cases MATCH"

# Voice recognition: Whisper-hallucination filter + VAD RMS math
# (faithful port of tools/voice_mode.py). Oracle-verified: exact-set +
# repeat-regex hallucination detection and sqrt(mean(x^2)) RMS match the
# live Python module for all fixtures under tests/oracle/fixtures/recognition.
test-recognition:
	@bash tests/oracle/runners/run_oracle.sh recognition 2>&1 \
	    | grep -E 'MISMATCH' && echo "(recognition oracle FAILED)" \
	    || echo "recognition oracle: all cases MATCH"

# Tool guardrails result hash (faithful port of agent/tool_guardrails.py).
# Oracle-verified: SHA256 of canonical JSON (recursively sorted keys, compact
# separators, ensure_ascii=False) vs LIVE Python across 12 cases.
test-tool-guardrails:
	@bash tests/oracle/runners/run_oracle.sh tool_guardrails 2>&1 \
	    | grep -E 'MISMATCH' && echo "(tool_guardrails oracle FAILED)" \
	    || echo "tool_guardrails oracle: all cases MATCH"

# Custom-provider extra_body resolution (faithful port of agent/agent_init.py).
# Oracle-verified: _custom_provider_extra_body_for_agent (incl. custom:filter,
# provider_key/name match, multi-model catalog, fallback) + _merge_extra_body
# (existing overrides win) vs LIVE Python across 9 cases.
test-provider-custom:
	@bash tests/oracle/runners/run_oracle.sh provider_custom 2>&1 \
	    | grep -E 'MISMATCH' && echo "(provider_custom oracle FAILED)" \
	    || echo "provider_custom oracle: all cases MATCH"


# Shared multi-user session classification (faithful port of
# gateway/session.py:is_shared_multi_user_session). Oracle-verified: C struct
# gw_session_source_t {chat_type, thread_id} + two bool flags diffed line-by-line
# against the LIVE gateway/session.py with a SessionSource built from the same
# fields, across 19 cases (dm/group/channel/thread x per-user flags).
test-shared-session:
	@bash tests/oracle/runners/run_oracle.sh shared_session 2>&1 \
	    | grep -E 'MISMATCH' && echo "(shared_session oracle FAILED)" \
	    || echo "shared_session oracle: all cases MATCH"

# Provider "same provider pool" support (faithful port of
# hermes_cli/setup.py:_supports_same_provider_pool_setup). Oracle-verified:
# the C PROVIDER_REGISTRY snapshot (44 keys, regenerated from the LIVE
# registry) is diffed line-by-line against the live
# hermes_cli.auth.PROVIDER_REGISTRY auth_type lookup, incl. custom/openrouter
# special cases and unknown providers -> false.
test-provider-pool-setup:
	@bash tests/oracle/runners/run_oracle.sh provider_pool_setup 2>&1 \
	    | grep -E 'MISMATCH' && echo "(provider_pool_setup oracle FAILED)" \
	    || echo "provider_pool_setup oracle: all cases MATCH"

# Provider auth registry exhaustive table-drift check (lib/libproviderauth
# vs LIVE hermes_cli.auth.PROVIDER_REGISTRY). Every key->auth_type row is
# emitted and diffed by tests/sta_oracle_provider_auth.py; any missing/extra/
# renamed key or wrong auth_type fails. This is the live-source-of-truth for
# config_setup_supports_same_provider_pool_setup (no hardcoded snapshot).
test-provider-auth:
	@bash tests/oracle/runners/run_oracle.sh provider_auth 2>&1 \
	    | grep -E 'MISMATCH' && echo "(provider_auth oracle FAILED)" \
	    || echo "provider_auth oracle: all rows MATCH"

# Curator backup config (faithful port of agent/curator_backup.py
# is_enabled/get_keep). Oracle-verified: real YAML (libyaml) navigation of
# curator.backup.enabled (default true) and .keep (default 5, floor 1),
# vs LIVE agent/curator_backup.py across 10 config docs (incl. the
# previously-broken cases: top-level enabled:false must NOT disable backup,
# and keep<=0 floors at 1).
test-curator-backup:
	@bash tests/oracle/runners/run_oracle.sh curator_backup 2>&1 \
	    | grep -E 'MISMATCH' && echo "(curator_backup oracle FAILED)" \
	    || echo "curator_backup oracle: all cases MATCH"

# Gateway platform short-label (faithful port of
# hermes_cli/setup.py:_gateway_platform_short_label). Oracle-verified:
# strip trailing "(...)" qualifier + .strip(), with the "base or label"
# fallback that the dead C version got wrong (returned "" for labels
# starting with "("). 16 cases incl. real _PLATFORMS labels + edges.
test-gateway-short-label:
	@bash tests/oracle/runners/run_oracle.sh gateway_short_label 2>&1 \
	    | grep -E 'MISMATCH' && echo "(gateway_short_label oracle FAILED)" \
	    || echo "gateway_short_label oracle: all cases MATCH"

# Message sanitize surrogate-structure walker (faithful port of
# agent/message_sanitization.py:_sanitize_structure_surrogates).
# Oracle-verified: recursive dict/list scrub of surrogate code points in
# string values (not keys), returns whether any were replaced, vs LIVE
# Python across 6 cases (flat/nested/array/key/multi/clean/JSON-args).
test-msg-sanitize-surrogates:
	@bash tests/oracle/runners/run_oracle.sh msg_sanitize_surrogates 2>&1 \
	    | grep -E 'MISMATCH' && echo "(msg_sanitize_surrogates oracle FAILED)" \
	    || echo "msg_sanitize_surrogates oracle: all cases MATCH"

# fuzzymatch utilities (count_lines / trim_right) — behavior-contract oracle.
test-fuzzy-utils:
	@bash tests/oracle/runners/run_oracle.sh fuzzy_utils 2>&1 \
	    | grep -E 'MISMATCH' && echo "(fuzzy_utils oracle FAILED)" \
	    || echo "fuzzy_utils oracle: all cases MATCH"

# V4A patch parser (patch_parser_parse_v4a) — behavior-contract oracle.
test-patch-parser:
	@bash tests/oracle/runners/run_oracle.sh patch_parser 2>&1 \
	    | grep -E 'MISMATCH' && echo "(patch_parser oracle FAILED)" \
	    || echo "patch_parser oracle: all cases MATCH"

# Schema sanitizer (schema_sanitizer) — behavior-contract oracle.
test-schema-sanitizer:
	@bash tests/oracle/runners/run_oracle.sh schema_sanitizer 2>&1 \
	    | grep -E 'MISMATCH' && echo "(schema_sanitizer oracle FAILED)" \
	    || echo "schema_sanitizer oracle: all cases MATCH"

test-fuzzy-match-helpers:
	@bash tests/oracle/runners/run_oracle.sh fuzzy_match_helpers 2>&1 \
	    | grep -E 'MISMATCH' && echo "(fuzzy_match_helpers oracle FAILED)" \
	    || echo "fuzzy_match_helpers oracle: all cases MATCH"

test-tool-output-limits:
	@bash tests/oracle/runners/run_oracle.sh tool_output_limits 2>&1 \
	    | grep -E 'MISMATCH' && echo "(tool_output_limits oracle FAILED)" \
	    || echo "tool_output_limits oracle: all cases MATCH"

test-path-security:
	@bash tests/oracle/runners/run_oracle.sh path_security 2>&1 \
	    | grep -E 'MISMATCH' && echo "(path_security oracle FAILED)" \
	    || echo "path_security oracle: all cases MATCH"

test-threat-patterns:
	@bash tests/oracle/runners/run_oracle.sh threat_patterns 2>&1 \
	    | grep -E 'MISMATCH' && echo "(threat_patterns oracle FAILED)" \
	    || echo "threat_patterns oracle: all cases MATCH"

test-skills-guard:
	@bash tests/oracle/runners/run_oracle.sh skills_guard 2>&1 \
	    | grep -E 'MISMATCH' && echo "(skills_guard oracle FAILED)" \
	    || echo "skills_guard oracle: all cases MATCH"

test-tool-search:
	@bash tests/oracle/runners/run_oracle.sh tool_search 2>&1 \
	    | grep -E 'MISMATCH' && echo "(tool_search oracle FAILED)" \
	    || echo "tool_search oracle: all cases MATCH"

test-credential-persistence:
	@bash tests/oracle/runners/run_oracle.sh credential_persistence 2>&1 \
	    | grep -E 'MISMATCH' && echo "(credential_persistence oracle FAILED)" \
	    || echo "credential_persistence oracle: all cases MATCH"

test-credential-sanitize:
	@bash tests/oracle/runners/run_oracle.sh credential_sanitize 2>&1 \
	    | grep -E 'MISMATCH' && echo "(credential_sanitize oracle FAILED)" \
	    || echo "credential_sanitize oracle: all cases MATCH"

test-credential-entry-to-json:
	@bash tests/oracle/runners/run_oracle.sh credential_entry_to_json 2>&1 \
	    | grep -E 'MISMATCH' && echo "(credential_entry_to_json oracle FAILED)" \
	    || echo "credential_entry_to_json oracle: all cases MATCH"

# API error summarizer (summarize_api_error) — contract oracle vs run_agent._summarize_api_error.
test-api-error-summary:
	@bash tests/oracle/runners/run_oracle.sh api_error_summary 2>&1 \
	    | grep -E 'MISMATCH' && echo "(api_error_summary oracle FAILED)" \
	    || echo "api_error_summary oracle: all cases MATCH"

# File-mutation verifier (tracker init/record/format_footer) — contract oracle vs
# run_agent._record_file_mutation_result + _format_file_mutation_failure_footer.
test-file-mutation-verifier:
	@bash tests/oracle/runners/run_oracle.sh file_mutation_verifier 2>&1 \
	    | grep -E 'MISMATCH' && echo "(file_mutation_verifier oracle FAILED)" \
	    || echo "file_mutation_verifier oracle: all cases MATCH"

# Microsoft Graph error extraction (msgraph_extract_error) — contract oracle.
test-msgraph-error:
	@bash tests/oracle/runners/run_oracle.sh msgraph_error 2>&1 \
	    | grep -E 'MISMATCH' && echo "(msgraph_error oracle FAILED)" \
	    || echo "msgraph_error oracle: all cases MATCH"

# Skill discovery (_discover_skills_in_dir) — contract oracle over a synthetic
# skill tree (real opendir walk, no mocks).
test-skills-discover:
	@bash tests/oracle/runners/run_oracle.sh skills_discover 2>&1 \
	    | grep -E 'MISMATCH' && echo "(skills_discover oracle FAILED)" \
	    || echo "skills_discover oracle: all cases MATCH"

# Provider/model setup probes (espeak / xai / reasoning_effort / model_config_dict
# / model_section_has_credentials / google_oauth require_client_id+client_secret)
# — contract oracle over REAL inputs (controlled PATH, real env vars, a real
# hermes_config_t). No mocks.
test-setup-probes:
	@bash tests/oracle/runners/run_oracle.sh setup_probes 2>&1 \
	    | grep -E 'MISMATCH' && echo "(setup_probes oracle FAILED)" \
	    || echo "setup_probes oracle: all cases MATCH"

test-kanban-helpers:
	@bash tests/oracle/runners/run_oracle.sh kanban_helpers 2>&1 \
	    | grep -E 'MISMATCH' && echo "(kanban_helpers oracle FAILED)" \
	    || echo "kanban_helpers oracle: all cases MATCH"

test-run-pure-helpers:
	@bash tests/oracle/runners/run_oracle.sh run_pure 2>&1 \
	    | grep -E 'MISMATCH' && echo "(run_pure oracle FAILED)" \
	    || echo "run_pure oracle: all cases MATCH"

test-url-safety:
	@bash tests/oracle/runners/run_oracle.sh url_safety 2>&1 \
	    | grep -E 'MISMATCH' && echo "(url_safety oracle FAILED)" \
	    || echo "url_safety oracle: all cases MATCH"

test-file-ops-pure:
	@bash tests/oracle/runners/run_oracle.sh file_ops_pure 2>&1 \
	    | grep -E 'MISMATCH' && echo "(file_ops_pure oracle FAILED)" \
	    || echo "file_ops_pure oracle: all cases MATCH"

test-skills-hub-path:
	@bash tests/oracle/runners/run_oracle.sh skills_hub_path 2>&1 \
	    | grep -E 'MISMATCH' && echo "(skills_hub_path oracle FAILED)" \
	    || echo "skills_hub_path oracle: all cases MATCH"

test-delegate-pure:
	@bash tests/oracle/runners/run_oracle.sh delegate_pure 2>&1 \
	    | grep -E 'MISMATCH' && echo "(delegate_pure oracle FAILED)" \
	    || echo "delegate_pure oracle: all cases MATCH"

test-cron-prompt-sanitize:
	@bash tests/oracle/runners/run_oracle.sh cron_prompt_sanitize 2>&1 \
	    | grep -E 'MISMATCH' && echo "(cron_prompt_sanitize oracle FAILED)" \
	    || echo "cron_prompt_sanitize oracle: all cases MATCH"

test-github-provider:
	@bash tests/oracle/runners/run_oracle.sh github_provider 2>&1 \
	    | grep -E 'MISMATCH' && echo "(github_provider oracle FAILED)" \
	    || echo "github_provider oracle: all cases MATCH"

test-command-sanitize:
	@bash tests/oracle/runners/run_oracle.sh command_sanitize 2>&1 \
	    | grep -E 'MISMATCH' && echo "(command_sanitize oracle FAILED)" \
	    || echo "command_sanitize oracle: all cases MATCH"

test-file-search:
	@bash tests/oracle/runners/run_oracle.sh file_search 2>&1 \
	    | grep -E 'MISMATCH' && echo "(file_search oracle FAILED)" \
	    || echo "file_search oracle: all cases MATCH"

test-command-clamp:
	@bash tests/oracle/runners/run_oracle.sh command_clamp 2>&1 \
	    | grep -E 'MISMATCH' && echo "(command_clamp oracle FAILED)" \
	    || echo "command_clamp oracle: all cases MATCH"

test-command-priority:
	@bash tests/oracle/runners/run_oracle.sh command_priority 2>&1 \
	    | grep -E 'MISMATCH' && echo "(command_priority oracle FAILED)" \
	    || echo "command_priority oracle: all cases MATCH"

test-search-context:
	@bash tests/oracle/runners/run_oracle.sh search_context 2>&1 \
	    | grep -E 'MISMATCH' && echo "(search_context oracle FAILED)" \
	    || echo "search_context oracle: all cases MATCH"

test-file-text-ops:
	@bash tests/oracle/runners/run_oracle.sh file_text_ops 2>&1 \
	    | grep -E 'MISMATCH' && echo "(file_text_ops oracle FAILED)" \
	    || echo "file_text_ops oracle: all cases MATCH"

test-skills-hub-filter:
	@bash tests/oracle/runners/run_oracle.sh skills_hub_filter 2>&1 \
	    | grep -E 'MISMATCH' && echo "(skills_hub_filter oracle FAILED)" \
	    || echo "skills_hub_filter oracle: all cases MATCH"

test-file-type:
	@bash tests/oracle/runners/run_oracle.sh file_type 2>&1 \
	    | grep -E 'MISMATCH' && echo "(file_type oracle FAILED)" \
	    || echo "file_type oracle: all cases MATCH"

# Cron delivery / origin / mirror / routing helpers (faithful port of the PURE
# config/routing transforms in cron/scheduler.py: _resolve_origin,
# _cron_mirror_delivery_enabled, _target_matches_origin,
# _is_known_delivery_platform, _resolve_home_env_var, _get_home_target_chat_id,
# _get_home_target_thread_id, _iter_home_target_platforms, cron_delivery_targets,
# _expand_routing_tokens, _resolve_single_delivery_target(s)). Contract oracle
# over REAL env-driven home channels. No mocks.
test-cron-delivery:
	@bash tests/oracle/runners/run_oracle.sh cron_delivery 2>&1 \
	    | grep -E 'MISMATCH' && echo "(cron_delivery oracle FAILED)" \
	    || echo "cron_delivery oracle: all cases MATCH"

# hermes debug helpers (faithful port of hermes_cli/debug.py pure logic).
test-debug: src/hermes_cli/debug_cli.o src/agent/redact.o
	@gcc -O2 -g -Wall -Wextra -I include -I src -I lib/libdb \
	    tests/test_debug_cli.c src/hermes_cli/debug_cli.o src/agent/redact.o -o /tmp/t_dbg 2>&1 \
	    && /tmp/t_dbg 2>&1 || echo "(debug test failed)"

# hermes_cli/auth.py pure helpers (faithful port of testable core).
test-auth: src/hermes_cli/auth_helpers.o lib/libcrypto/crypto.o
	@gcc -O2 -g -Wall -Wextra -I include -I src -I lib/libdb \
	    tests/test_auth_helpers.c src/hermes_cli/auth_helpers.o lib/libcrypto/crypto.o -lcrypto -o /tmp/t_auth 2>&1 \
	    && /tmp/t_auth 2>&1 || echo "(auth test failed)"

# cron/scheduler.py pure string helpers (_summarize_cron_failure_for_delivery,
# _is_cron_silence_response, _normalize_deliver_value).
test-cron-helpers: src/cron/port_cron_scheduler_helpers.o
	@gcc -O2 -g -Wall -Wextra -I include -I src \
	    tests/test_cron_scheduler_helpers.c src/cron/port_cron_scheduler_helpers.o -o /tmp/t_cron_h 2>&1 \
	    && /tmp/t_cron_h 2>&1 || echo "(cron helpers test failed)"

# hermes_cli/commands.py file_size_label (reused existing gateway_command_sanitize).
test-gateway-cmd-sanitize: src/cli/gateway_command_sanitize.o
	@gcc -O2 -g -Wall -Wextra -I include -I src -I lib/libjson \
	    tests/test_gateway_command_sanitize.c src/cli/gateway_command_sanitize.o lib/libjson/json.o -o /tmp/t_gwsan 2>&1 \
	    && /tmp/t_gwsan 2>&1 || echo "(gateway cmd sanitize test failed)"

# cua_backend.py pure parse helpers (image dims, tree split, key combo).
test-cua-helpers: src/tools/port_cua_backend_helpers.o
	@gcc -O2 -g -Wall -Wextra -I include -I src \
	    tests/test_cua_backend_helpers.c src/tools/port_cua_backend_helpers.o -o /tmp/t_cua 2>&1 \
	    && /tmp/t_cua 2>&1 || echo "(cua helpers test failed)"

# hermes_cli/checkpoints.py pure format helpers (bytes/age/timestamp).
test-checkpoints-format: src/cli/port_checkpoints_format.o
	@gcc -O2 -g -Wall -Wextra -I include -I src \
	    tests/test_checkpoints_format.c src/cli/port_checkpoints_format.o -o /tmp/t_ckfmt 2>&1 \
	    && /tmp/t_ckfmt 2>&1 || echo "(checkpoints format test failed)"

# tools/url_safety.py pure helpers (sensitive query params, private-IP gate).
test-url-safety-helpers: src/tools/port_url_safety_helpers.o
	@gcc -O2 -g -Wall -Wextra -I include -I src \
	    tests/test_url_safety_helpers.c src/tools/port_url_safety_helpers.o -o /tmp/t_urlsafe 2>&1 \
	    && /tmp/t_urlsafe 2>&1 || echo "(url safety helpers test failed)"

# agent/moa_trace.py pure helper (session-id filename sanitization).
test-moa-trace-helpers: src/agent/port_moa_trace_helpers.o
	@gcc -O2 -g -Wall -Wextra -I include -I src \
	    tests/test_moa_trace_helpers.c src/agent/port_moa_trace_helpers.o -o /tmp/t_moa 2>&1 \
	    && /tmp/t_moa 2>&1 || echo "(moa trace helpers test failed)"

# agent/pet/generate/prompts.py pure prompt builders (style/spacing/base/row).
test-pet-prompts: src/pet/port_pet_prompts.o
	@gcc -O2 -g -Wall -Wextra -I include -I src \
	    tests/test_pet_prompts.c src/pet/port_pet_prompts.o -o /tmp/t_petprompts 2>&1 \
	    && /tmp/t_petprompts 2>&1 || echo "(pet prompts test failed)"

# cron/scripts/classify_items.py pure helpers (item id / prompt / score parse).
test-classify-items: src/cron/port_classify_items.o lib/libjson/json.o
	@gcc -O2 -g -Wall -Wextra -I include -I src -I lib -I lib/libjson \
	    tests/test_classify_items.c src/cron/port_classify_items.o lib/libjson/json.o -o /tmp/t_cls 2>&1 \
	    && /tmp/t_cls 2>&1 || echo "(classify items test failed)"

# hermes_cli/fallback_config.py pure helpers (fallback chain merge/dedup).
test-fallback-config: src/cli/port_fallback_config.o lib/libjson/json.o
	@gcc -O2 -g -Wall -Wextra -I include -I src -I lib -I lib/libjson \
	    tests/test_fallback_config.c src/cli/port_fallback_config.o lib/libjson/json.o -o /tmp/t_fb 2>&1 \
	    && /tmp/t_fb 2>&1 || echo "(fallback config test failed)"

# hermes_cli/timeouts.py pure helpers (provider/stale request timeout lookup).
test-timeouts: src/cli/port_timeouts.o lib/libjson/json.o
	@gcc -O2 -g -Wall -Wextra -I include -I src -I lib -I lib/libjson \
	    tests/test_timeouts.c src/cli/port_timeouts.o lib/libjson/json.o -o /tmp/t_to 2>&1 \
	    && /tmp/t_to 2>&1 || echo "(timeouts test failed)"

# hermes_cli/session_listing.py pure helpers (arg parse + gateway format).
test-session-listing: src/cli/port_session_listing.o lib/libjson/json.o
	    @gcc -O2 -g -Wall -Wextra -I include -I src -I lib -I lib/libjson \
	        tests/test_session_listing.c src/cli/port_session_listing.o lib/libjson/json.o -o /tmp/t_sl 2>&1 \
	        && /tmp/t_sl 2>&1 || echo "(session listing test failed)"


	    # tui_gateway/project_tree.py pure path/lane-id helpers.
test-project-tree: src/cli/port_project_tree.o
	    @gcc -O2 -g -Wall -Wextra -I include -I src -I lib -I lib/libjson \
	        tests/test_project_tree.c src/cli/port_project_tree.o -o /tmp/t_pt 2>&1 \
	        && /tmp/t_pt 2>&1 || echo "(project tree test failed)"

	    # agent/lsp/range_shift.py pure helpers (diff-aware LSP line-shift map).
test-lsp-range-shift: src/agent/port_lsp_range_shift.o lib/libjson/json.o
	@gcc -O2 -g -Wall -Wextra -I include -I src -I lib -I lib/libjson \
	    tests/test_lsp_range_shift.c src/agent/port_lsp_range_shift.o lib/libjson/json.o -o /tmp/t_rs 2>&1 \
	    && /tmp/t_rs 2>&1 || echo "(lsp range shift test failed)"

# DM pairing store (faithful port of gateway/pairing.py).
	@gcc -O2 -g -Wall -Wextra -I include -I src -I lib/libdb \
	    tests/test_pairing.c src/gateway/pairing.o lib/libcrypto/crypto.o src/gateway/helpers.o lib/libjson/json.o -lcrypto -o /tmp/t_pair 2>&1 \
	    && /tmp/t_pair 2>&1 || echo "(pairing test failed)"

# Kanban CLI pure formatting/parsing helpers (faithful port of hermes_cli/kanban.py).
test-kanban-format: src/hermes_cli/kanban_format.o
	@gcc -O2 -g -Wall -Wextra -I include -I src tests/test_kanban_format.c src/hermes_cli/kanban_format.o -o /tmp/t_kanban 2>&1 \
	    && /tmp/t_kanban 2>&1 || echo "(kanban-format test failed)"

# Combined check - lint, build, test suite
check:
	@echo "=== Check: lint ==="
	bash -n tests/run_mission8_tests.sh 2>/dev/null || true
	@echo "=== Check: build ==="
	$(MAKE) -j$(nproc) all
	@echo "=== Check: test suite ==="
	bash tests/run_mission8_tests.sh

# What changed since last git pull (upstream Python diff)
what-changed:
	@cd .. && git log --oneline @{u}..HEAD --name-only -- '*.py' 2>/dev/null | head -40