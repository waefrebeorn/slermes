# ── Developer & Analysis Targets ──────────────────────────────────
# docs, static-analysis, upstream-sync/merge, digest, python-deps
# Included by top-level Makefile

.PHONY: docs static-analysis upstream-sync upstream-merge sync-all digest python-deps release deps what-changed

# API documentation via Doxygen
docs:
	@if command -v doxygen > /dev/null 2>&1; then \
		echo "Generating API docs..."; \
		doxygen Doxyfile 2>/dev/null; \
		echo "Done: docs/api/html/index.html"; \
	else \
		echo "doxygen not installed. Install: sudo apt install doxygen"; \
	fi

# Static analysis — cppcheck on src/ agent/ tools/ gateway/ cron/
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

# Super Fork — Upstream Tracking
# Fetch origin/main (upstream), diff Python since last sync, report C work needed
upstream-sync:
	@if [ -f digest.py ]; then python3 digest.py --upstream; else echo "NOTE: digest.py not found — upstream tracking unavailable"; fi

# Fetch + merge upstream into current branch + generate stubs
upstream-merge:
	@if [ -f digest.py ]; then python3 digest.py --upstream --merge --generate-stubs; else echo "NOTE: digest.py not found — upstream merge unavailable. Install from upstream repo."; fi

# Full workflow: sync, build, report
sync-all: upstream-merge phase5
	@echo "Super Fork sync complete."

# Digestion — standard local diff
digest:
	@if [ -f digest.py ]; then python3 digest.py; else echo "NOTE: digest.py not found — diff digest unavailable"; fi

# Convenience: install dependencies (used by CI workflows)
deps:
	@echo "=== Installing Slermes build dependencies ==="
	@if command -v apt-get >/dev/null 2>&1; then \\
		sudo apt-get update -qq && sudo apt-get install -y -qq build-essential libssl-dev pkg-config file 2>/dev/null || true; \\
	elif command -v brew >/dev/null 2>&1; then \\
		brew install pkg-config openssl file 2>/dev/null || true; \\
	fi
	@echo "=== Dependency check complete ==="

# Python bridge dependencies
python-deps:
	@echo "=== Installing Python bridge dependencies ==="
	@pip install -r requirements-bridge.txt 2>/dev/null \
		|| pip3 install -r requirements-bridge.txt 2>/dev/null \
		|| echo "  WARNING: pip not found. Install manually: pip install -r requirements-bridge.txt"
	@echo "=== Python bridge dependencies installed ==="

# Release automation
release:
	@bash scripts/release.sh $(filter-out $@,$(MAKECMDGOALS))
