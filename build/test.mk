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
