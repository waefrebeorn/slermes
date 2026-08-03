# ── Install & Uninstall Targets ───────────────────────────────────
# Included by top-level Makefile. Expected vars: PREFIX, BINDIR, DOCDIR, DATADIR
# make uninstall                    — removes binary, preserves config
# make uninstall FORCE=1            — removes config too
# make install-gui                 — installs desktop GUI too
# make install-web                  — installs web server too

.PHONY: install install-gui install-web install-all uninstall

PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
DOCDIR ?= $(PREFIX)/share/doc/slermes
DATADIR ?= $(PREFIX)/share/slermes

# Slermes home directory (own infra — NOT ~/.hermes)
SLERMES_HOME_DIR ?= $(HOME)/.slermes

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
	@echo "  Creating $(SLERMES_HOME_DIR) (sessions, skills, cron, profiles, cache, plugins, logs, pets)"
	@mkdir -p $(SLERMES_HOME_DIR)/sessions $(SLERMES_HOME_DIR)/skills \
		$(SLERMES_HOME_DIR)/cron $(SLERMES_HOME_DIR)/profiles \
		$(SLERMES_HOME_DIR)/cache $(SLERMES_HOME_DIR)/plugins \
		$(SLERMES_HOME_DIR)/logs $(SLERMES_HOME_DIR)/pets
	@echo "=== Install complete ==="
	@echo "Run: slermes init  (then slermes --help)"

install-gui: desktop-gui install
	install -m 755 slermes-desktop-gui $(DESTDIR)$(BINDIR)/slermes-desktop-gui
	@echo "  Desktop GUI: $(DESTDIR)$(BINDIR)/slermes-desktop-gui"

install-web: web_server install
	install -m 755 web_server $(DESTDIR)$(BINDIR)/slermes-web-server
	@echo "  Web server: $(DESTDIR)$(BINDIR)/slermes-web-server"

install-all: install-gui install-web
	@echo "=== Full install complete ==="

uninstall:
	@echo "=== Uninstalling Slermes ==="
	@INSTALLED=0; \
	for d in ~/.local/bin /usr/local/bin /usr/bin; do \
		if [ -f "$$d/slermes" ]; then \
			rm -f "$$d/slermes" && echo "  Removed: $$d/slermes" && INSTALLED=1; \
		fi; \
		if [ -f "$$d/slermes-desktop-gui" ]; then \
			rm -f "$$d/slermes-desktop-gui" && echo "  Removed: $$d/slermes-desktop-gui"; \
		fi; \
		if [ -f "$$d/slermes-web-server" ]; then \
			rm -f "$$d/slermes-web-server" && echo "  Removed: $$d/slermes-web-server"; \
		fi; \
	done; \
	if [ "$$INSTALLED" = "0" ]; then echo "  Binary not found in PATH — already clean."; fi
	@echo "  Note: $(SLERMES_HOME_DIR)/config.yaml and .env preserved."
	@if [ -n "$(FORCE)" ] && [ -d $(SLERMES_HOME_DIR) ]; then \
		echo "  FORCE=1: removing $(SLERMES_HOME_DIR)"; \
		rm -rf $(SLERMES_HOME_DIR)/config.yaml $(SLERMES_HOME_DIR)/.env $(SLERMES_HOME_DIR)/skills; \
		echo "  Config, .env, and skills removed. Logs and sessions preserved."; \
	fi
	@echo "=== Uninstall complete ==="
