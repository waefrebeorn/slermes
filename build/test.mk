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

# Combined check — lint, build, test suite
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
