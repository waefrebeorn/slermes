# ── Plugin Build Targets ──────────────────────────────────────────
# Build example plugins as .so files and install to ~/.hermes/plugins/
# Included by top-level Makefile

.PHONY: plugins install-plugins

plugins: src/plugins/plugin_honcho.so src/plugins/plugin_kanban.so src/plugins/plugin_spotify.so src/plugins/plugin_disk_cleanup.so src/plugins/plugin_file_memory.so src/plugins/plugin_achievements.so src/plugins/plugin_observability.so src/plugins/plugin_skills.so src/plugins/plugin_image_gen.so src/plugins/plugin_google_meet.so src/plugins/plugin_browser.so src/plugins/plugin_context_engine.so src/plugins/plugin_dashboard_auth.so src/plugins/plugin_model_providers.so src/plugins/plugin_platforms.so src/plugins/plugin_teams_pipeline.so src/plugins/plugin_video_gen.so src/plugins/plugin_web.so

src/plugins/plugin_%.so: src/plugins/plugin_%.c include/hermes_plugin.h lib/libplugin/plugin.h
	$(CC) -O2 -fPIC -shared -I include -I lib/libplugin -DSPOTIFY_PLUGIN_VERSION='"1.0.0"' -o $@ $< -ldl

install-plugins: plugins
	mkdir -p ~/.hermes/plugins
	cp src/plugins/plugin_honcho.so ~/.hermes/plugins/
	cp src/plugins/plugin_kanban.so ~/.hermes/plugins/
	cp src/plugins/plugin_spotify.so ~/.hermes/plugins/
	cp src/plugins/honcho-memory.yaml ~/.hermes/plugins/
	cp src/plugins/kanban-board.yaml ~/.hermes/plugins/
	cp src/plugins/spotify-control.yaml ~/.hermes/plugins/
	@echo "Plugins installed to ~/.hermes/plugins/"
