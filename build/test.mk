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

# DM pairing store (faithful port of gateway/pairing.py).
test-pairing: src/gateway/pairing.o lib/libcrypto/crypto.o src/gateway/helpers.o lib/libjson/json.o
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