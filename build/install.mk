# ── Install & Uninstall Targets ───────────────────────────────────
# Included by top-level Makefile. Expected vars: PREFIX, BINDIR, DOCDIR, DATADIR
# make uninstall                    — removes binary, preserves config
# make uninstall FORCE=1            — removes config too

.PHONY: install uninstall

PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
DOCDIR ?= $(PREFIX)/share/doc/slermes
DATADIR ?= $(PREFIX)/share/slermes

install: slermes
	@echo "=== Installing Slermes ==="
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 slermes $(DESTDIR)$(BINDIR)/slermes
	install -d $(DESTDIR)$(DOCDIR)
	install -m 644 README.md $(DESTDIR)$(DOCDIR)/README.md
	install -d $(DESTDIR)$(DATADIR)/docs
	@cp -r docs/* $(DESTDIR)$(DATADIR)/docs/ 2>/dev/null || true
	@echo "  Binary: $(DESTDIR)$(BINDIR)/slermes"
	@echo "  Docs:   $(DESTDIR)$(DATADIR)/docs/"
	@echo "=== Install complete ==="
	@echo "Run: slermes --help"

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
