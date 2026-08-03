# ── Compilation Rules & Static Library Archives ───────────────────
# Pattern rules, per-file compilation rules, and .a archive bundling
# Included by top-level Makefile. Expected vars: CC, CXX, AR, CFLAGS, LIB_INCS

# SQLite3 compile with threading-friendly flags (FTS5 enabled for session search)
lib/libdb/sqlite3.o: lib/libdb/sqlite3.c lib/libdb/sqlite3.h
	$(CC) $(CFLAGS) -DSQLITE_THREADSAFE=0 -DSQLITE_OMIT_LOAD_EXTENSION -DSQLITE_ENABLE_FTS5 -c -o $@ $<

src/slermes_home.o: src/slermes_home.c include/slermes_home.h
	$(CC) $(CFLAGS) -c -o $@ $<

src/skills/skills_parser.o: src/skills/skills_parser.c include/skills_parser.h
	$(CC) $(CFLAGS) -c -o $@ $<

# Desktop GUI custom rules (need sdl2 flags)
src/gui_core.o: src/gui_core.c include/gui_core.h
	$(CC) $(CFLAGS) $(DESKTOP_GUI_CFLAGS) -I include -c -o $@ $<

src/desktop_gui.o: src/desktop_gui.c include/gui_core.h
	$(CC) $(CFLAGS) $(DESKTOP_GUI_CFLAGS) -I include -I lib -c -o $@ $<

# Pattern rule for .c files — NOTE: hermes.h omitted as explicit dependency
# to work around GitHub Actions Make 4.3 pattern-rule resolution issue
# -I lib added so "libjson/..." and similar prefixed includes resolve
# (consistent with the lib/%.o rule which uses $(LIB_INCS)).
src/%.o: src/%.c
	$(CC) $(CFLAGS) $(TUI_INC) -I include -I lib -c -o $@ $<

# Pattern rule for .m files (Objective-C, macOS only)
src/%.o: src/%.m
	$(CC) $(CFLAGS) $(TUI_INC) -c -o $@ $<

# Library pattern rule — standalone libs
# NOTE: -I lib is required so lib/*.c files that include hermes.h can resolve
# prefixed vendored headers like "libdb/db.h" (same as the src/%.o rule).
lib/%.o: lib/%.c
	$(CC) $(CFLAGS) $(LIB_INCS) -I lib -c -o $@ $<

# Special rules for libs that need extra includes
lib/libtextwrap/textwrap.o: lib/libtextwrap/textwrap.c lib/libtextwrap/textwrap.h
	$(CC) $(CFLAGS) -c $< -o $@

lib/libglob/glob.o: lib/libglob/glob.c lib/libglob/hermes_glob.h
	$(CC) $(CFLAGS) -c $< -o $@

# WuBuOffice (vendored OOXML toolkit): needs POSIX feature macros for
# strdup/strstr under -std=c11 (mirrors its own CMake add_compile_definitions).
lib/libwubuoffice/%.o: lib/libwubuoffice/%.c
	$(CC) $(CFLAGS) -D_POSIX_C_SOURCE=200809L -I lib/libwubuoffice/src/wubuzip -I lib/libwubuoffice/src/wubuoxml -c $< -o $@

# PNG decoder (from-scratch, libpngdec): rides on wubuzip's DEFLATE inflate
# + CRC32 — needs that include root for "wubuzip/inflate.h".
lib/libpngdec/pngdec.o: lib/libpngdec/pngdec.c lib/libpngdec/pngdec.h
	$(CC) $(CFLAGS) -I lib/libwubuoffice/src -c $< -o $@

# The read_extract port consumes the same wubuoxml headers via
# "wubuoxml/reader.h" / "wubuoxml/docx_text.h", which live under
# lib/libwubuoffice/src. Add that include root for this one TU.
src/tools/port_tools_read_extract.o: src/tools/port_tools_read_extract.c
	$(CC) $(CFLAGS) $(TUI_INC) -I include -I lib -I lib/libwubuoffice/src -c -o $@ $<

lib/libdifflib/difflib.o: lib/libdifflib/difflib.c lib/libdifflib/difflib.h
	$(CC) $(CFLAGS) -c $< -o $@

lib/libregex/hermes_regex.o: lib/libregex/hermes_regex.c lib/libregex/hermes_regex.h
	$(CC) $(CFLAGS) -c $< -o $@

lib/libsignal/hermes_signal.o: lib/libsignal/hermes_signal.c lib/libsignal/hermes_signal.h
	$(CC) $(CFLAGS) -c $< -o $@

lib/libwebsocket/websocket.o: lib/libwebsocket/websocket.c lib/libwebsocket/websocket.h
	$(CC) $(CFLAGS) -c $< -o $@

lib/libprotobuf/protobuf.o: lib/libprotobuf/protobuf.c lib/libprotobuf/protobuf.h
	$(CC) $(CFLAGS) -c $< -o $@

lib/libtoml/toml.o: lib/libtoml/toml.c lib/libtoml/toml.h
	$(CC) $(CFLAGS) -c $< -o $@

lib/libansi/ansi.o: lib/libansi/ansi.c lib/libansi/ansi.h
	$(CC) $(CFLAGS) -c $< -o $@

lib/libansi/ansi_strip.o: lib/libansi/ansi_strip.c lib/libansi/ansi.h
	$(CC) $(CFLAGS) -c $< -o $@

lib/libjson5/json5.o: lib/libjson5/json5.c lib/libjson5/json5.h lib/libjson/json.h
	$(CC) $(CFLAGS) -I lib/libjson -c $< -o $@

lib/librateguard/rate_guard.o: lib/librateguard/rate_guard.c lib/librateguard/rate_guard.h
	$(CC) $(CFLAGS) -c $< -o $@

lib/libfal_common/fal_common.o: lib/libfal_common/fal_common.c lib/libfal_common/fal_common.h lib/libjson/json.h lib/libhttp/http.h
	$(CC) $(CFLAGS) -I lib/libjson -I lib/libhttp -c $< -o $@

lib/libtooloutput/tool_output.o: lib/libtooloutput/tool_output.c
	$(CC) $(CFLAGS) -c $< -o $@

lib/libxai_http/xai_http.o: lib/libxai_http/xai_http.c
	$(CC) $(CFLAGS) -c $< -o $@

lib/libmcp_oauth/mcp_oauth.o: lib/libmcp_oauth/mcp_oauth.c lib/libmcp_oauth/mcp_oauth.h
	$(CC) $(CFLAGS) -c $< -o $@

lib/libenvpassthrough/env_passthrough.o: lib/libenvpassthrough/env_passthrough.c
	$(CC) $(CFLAGS) -c $< -o $@

lib/libcredential/credential.o: lib/libcredential/credential.c
	$(CC) $(CFLAGS) -c $< -o $@

lib/libschemasanitizer/schema_sanitizer.o: lib/libschemasanitizer/schema_sanitizer.c
	$(CC) $(CFLAGS) -c $< -o $@

lib/libfuzzymatch/fuzzy_match.o: lib/libfuzzymatch/fuzzy_match.c
	$(CC) $(CFLAGS) -c $< -o $@

lib/libproviderauth/provider_auth.o: lib/libproviderauth/provider_auth.c lib/libproviderauth/provider_auth.h
	$(CC) $(CFLAGS) -I include -c $< -o $@

lib/libinterrupt/interrupt.o: lib/libinterrupt/interrupt.c
	$(CC) $(CFLAGS) -c $< -o $@

lib/libtoolbackend/tool_backend.o: lib/libtoolbackend/tool_backend.c
	$(CC) $(CFLAGS) -c $< -o $@

lib/libmangateway/managed_gateway.o: lib/libmangateway/managed_gateway.c
	$(CC) $(CFLAGS) -c $< -o $@

lib/libratelimit/rate_limit.o: lib/libratelimit/rate_limit.c
	$(CC) $(CFLAGS) -c $< -o $@

lib/libfilestate/file_state.o: lib/libfilestate/file_state.c
	$(CC) $(CFLAGS) -c $< -o $@

lib/libtooldispatch/tool_dispatch_helpers.o: lib/libtooldispatch/tool_dispatch_helpers.c
	$(CC) $(CFLAGS) -c $< -o $@

lib/liberrorclassifier/error_classifier.o: lib/liberrorclassifier/error_classifier.c lib/liberrorclassifier/error_classifier.h
	$(CC) $(CFLAGS) -c $< -o $@

lib/libbudgetconfig/budget_config.o: lib/libbudgetconfig/budget_config.c lib/libbudgetconfig/budget_config.h
	$(CC) $(CFLAGS) -c $< -o $@

lib/libthreatpatterns/threat_patterns.o: lib/libthreatpatterns/threat_patterns.c lib/libthreatpatterns/threat_patterns.h lib/libregex/hermes_regex.h
	$(CC) $(CFLAGS) -I lib/libregex -c $< -o $@

lib/libcredentialfiles/credential_files.o: lib/libcredentialfiles/credential_files.c lib/libcredentialfiles/credential_files.h
	$(CC) $(CFLAGS) -c $< -o $@

lib/libskillaudit/skill_audit.o: lib/libskillaudit/skill_audit.c lib/libskillaudit/skill_audit.h
	$(CC) $(CFLAGS) -c $< -o $@

lib/libslashconfirm/slash_confirm.o: lib/libslashconfirm/slash_confirm.c lib/libslashconfirm/slash_confirm.h
	$(CC) $(CFLAGS) -c $< -o $@

lib/libmsgraph/ms_graph.o: lib/libmsgraph/ms_graph.c lib/libmsgraph/ms_graph.h lib/libjson/json.h lib/libhttp/http.h
	$(CC) $(CFLAGS) -I lib/libjson -I lib/libhttp -c $< -o $@

lib/libcurses_widget/curses_widget.o: lib/libcurses_widget/curses_widget.c lib/libcurses_widget/curses_widget.h
	$(CC) $(CFLAGS) $(LIB_INCS) -I lib/libcurses_widget $(if $(findstring 1,$(HAS_NCURSES)),-DHAS_NCURSES_TUI -I lib/libncurses/include) -c $< -o $@

# Recompile curses_widget with ncurses TUI support for the tui build
lib/libcurses_widget/curses_widget_tui.o: lib/libcurses_widget/curses_widget.c lib/libcurses_widget/curses_widget.h
	$(CC) $(CFLAGS) $(LIB_INCS) -DHAS_NCURSES_TUI -I lib/libncurses/include -c $< -o $@

# whisper.cpp third-party library (C++ wrapper)
lib/whisper_cpp/whisper_wrapper.o: lib/whisper_cpp/whisper_wrapper.cc lib/whisper_cpp/whisper_wrapper.h
	$(CXX) $(CFLAGS) -I lib/whisper_cpp/include -std=c++11 -c $< -o $@

# whisper stubs (used when prebuilt .a libs are not available)
lib/whisper_cpp/whisper_stubs.o: lib/whisper_cpp/whisper_stubs.c lib/whisper_cpp/whisper_wrapper.h
	$(CC) $(CFLAGS) -I lib/whisper_cpp/include -c $< -o $@

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

# Static library archives — bundled .a rules
lib/libbinary.a: lib/libbinary/binary.o
	$(AR) rcs $@ $^

lib/libbrowser.a: lib/libbrowser/camofox_state.o
	$(AR) rcs $@ $^

lib/libdebug.a: lib/libdebug/debug_helpers.o
	$(AR) rcs $@ $^

lib/libosv.a: lib/libosv/osv.o
	$(AR) rcs $@ $^

lib/libwebsite.a: lib/libwebsite/website_policy.o
	$(AR) rcs $@ $^

lib/libenvpassthrough.a: lib/libenvpassthrough/env_passthrough.o
	$(AR) rcs $@ $^

lib/libxai_http.a: lib/libxai_http/xai_http.o
	$(AR) rcs $@ $^

lib/libasync.a: lib/libasync/fiber.o lib/libasync/async_http.o
	$(AR) rcs $@ $^

lib/libcredential.a: lib/libcredential/credential.o
	$(AR) rcs $@ $^

lib/libschemasanitizer.a: lib/libschemasanitizer/schema_sanitizer.o
	$(AR) rcs $@ $^

lib/libfuzzymatch.a: lib/libfuzzymatch/fuzzy_match.o
	$(AR) rcs $@ $^

lib/libjson.a: lib/libjson/json.o lib/libjson/json_yaml_bridge.o
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

lib/libtextwrap.a: lib/libtextwrap/textwrap.o
	$(AR) rcs $@ $^

lib/libglob.a: lib/libglob/glob.o
	$(AR) rcs $@ $^

lib/libdifflib.a: lib/libdifflib/difflib.o
	$(AR) rcs $@ $^

lib/libregex.a: lib/libregex/hermes_regex.o
	$(AR) rcs $@ $^

lib/libsignal.a: lib/libsignal/hermes_signal.o
	$(AR) rcs $@ $^

lib/libwebsocket.a: lib/libwebsocket/websocket.o
	$(AR) rcs $@ $^

lib/libprotobuf.a: lib/libprotobuf/protobuf.o
	$(AR) rcs $@ $^

lib/libtoml.a: lib/libtoml/toml.o
	$(AR) rcs $@ $^

lib/libansi.a: lib/libansi/ansi.o
	$(AR) rcs $@ $^

lib/libjson5.a: lib/libjson5/json5.o
	$(AR) rcs $@ $^

lib/libskillusage.a: lib/libskillusage/skill_usage.o lib/libskillusage/skill_provenance.o
	$(AR) rcs $@ $^

lib/libskillsync.a: lib/libskillsync/skills_sync.o
	$(AR) rcs $@ $^

lib/libtranscribe.a: lib/libtranscribe/transcribe.o lib/libtranscribe/transcription_registry.o
	$(AR) rcs $@ $^

lib/libmcp_oauth.a: lib/libmcp_oauth/mcp_oauth.o
	$(AR) rcs $@ $^

lib/libfal_common.a: lib/libfal_common/fal_common.o
	$(AR) rcs $@ $^

lib/libtooloutput.a: lib/libtooloutput/tool_output.o
	$(AR) rcs $@ $^

lib/libratelimit.a: lib/libratelimit/rate_limit.o
	$(AR) rcs $@ $^

lib/libmangateway.a: lib/libmangateway/managed_gateway.o
	$(AR) rcs $@ $^

lib/libtoolbackend.a: lib/libtoolbackend/tool_backend.o
	$(AR) rcs $@ $^

lib/libinterrupt.a: lib/libinterrupt/interrupt.o
	$(AR) rcs $@ $^

lib/libfilestate.a: lib/libfilestate/file_state.o
	$(AR) rcs $@ $^

lib/libtooldispatch.a: lib/libtooldispatch/tool_dispatch_helpers.o
	$(AR) rcs $@ $^

lib/librateguard.a: lib/librateguard/rate_guard.o
	$(AR) rcs $@ $^

lib/libskillutils.a: lib/libskillutils/skill_utils.o
	$(AR) rcs $@ $^

lib/liberrorclassifier.a: lib/liberrorclassifier/error_classifier.o
	$(AR) rcs $@ $^

lib/libbudgetconfig.a: lib/libbudgetconfig/budget_config.o
	$(AR) rcs $@ $^

lib/libcredentialfiles.a: lib/libcredentialfiles/credential_files.o
	$(AR) rcs $@ $^

lib/libskillaudit.a: lib/libskillaudit/skill_audit.o
	$(AR) rcs $@ $^

lib/libslashconfirm.a: lib/libslashconfirm/slash_confirm.o
	$(AR) rcs $@ $^

lib/libmsgraph.a: lib/libmsgraph/ms_graph.o
	$(AR) rcs $@ $^

lib/libcurses_widget.a: lib/libcurses_widget/curses_widget.o
	$(AR) rcs $@ $^

# Build all standalone libs
libs: $(LIB_A)
	@echo "Standalone libraries built: $(words $(LIB_A)) archives"

# ── libhive shared library (dynamic linking) ─────────────────────────────
# Pure C11 skipfield+freelist collection as a loadable .so, plus its
# oracle-style correctness test. Build:  make hive.so   /   make hive-test
lib/libhive/hive.so: lib/libhive/hive.c lib/libhive/hive.h
	$(CC) -std=c11 -O2 -fPIC -shared -Wall -Wextra -I lib/libhive -o $@ lib/libhive/hive.c
	@echo "libhive shared library built: $@"

hive.so: lib/libhive/hive.so

hive-test: lib/libhive/hive.c lib/libhive/hive_test.c lib/libhive/hive.h
	$(CC) -std=c11 -O2 -Wall -Wextra -I lib/libhive lib/libhive/hive.c lib/libhive/hive_test.c -o /tmp/t_hive
	/tmp/t_hive

.PHONY: hive.so hive-test
