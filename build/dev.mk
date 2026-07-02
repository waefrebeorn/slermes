# ── Developer & Analysis Targets ──────────────────────────────────
# docs, static-analysis, upstream-sync/merge, digest, python-deps
# Included by top-level Makefile

.PHONY: docs static-analysis static-analysis upstream-sync upstream-merge sync-all digest python-deps

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
	python3 digest.py --upstream

# Fetch + merge upstream into current branch + generate stubs
upstream-merge:
	python3 digest.py --upstream --merge --generate-stubs

# Full workflow: sync, build, report
sync-all: upstream-merge phase5
	@echo "Super Fork sync complete."

# Digestion — standard local diff
digest:
	python3 digest.py

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
