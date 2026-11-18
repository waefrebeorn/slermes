<!-- PARITY:AUTO -->
| PORTED  | 8,377 / 34,262 (24.4%) |
| REAL_GAP| 25,840 (75.4%) — no N/A |
|  ├ DA-4 stubs | 13 |
|  └ Genuine gaps | 25,827 |
| PARTIAL | 45 (0.1%) |
| BOOTLEG | 2 |
| **Watercup** | unavailable — oracle produced no verdicts |
**Phase (v673):** PORT phase

**Upstream sync:** 0 ahead / 0 behind (last merge unknown)

_Generated 2026-11-18T22:16:00Z from live scanner + watercup oracle — do not edit by hand._
<!-- /PARITY:AUTO -->

# Third-Party SLERMing — Plugins, Providers, and the Dependency Front

The mission is the whole ecosystem: not just the agent, but every plugin,
provider and platform adapter — and the third-party libraries underneath
them — reimplemented from scratch in C11. Nothing here is "not doable";
every dependency is either SLERMed, being SLERMed, or queued to be.

**Status discipline:** this file lists what C units EXIST. Existence is not
parity. Behavioral parity is proven per-port by the differential oracle and
the watercup gate, and the live counts are owned by `make parity`
(see the sentinels in README/BANNER and `docs/parity-summary.md`). The
per-area module coverage rollup is `python scripts/parity_coverage.py --dirs`
— at the latest run the `plugins` area sat at 44/304 modules (14%),
which is the honest headline: the plugin front is early, not done.

## Platform adapters

The gateway platform adapters live in `src/gateway/platforms/` (60 `.c`
files at this writing — telegram, discord, slack, signal, whatsapp, matrix,
feishu, wecom, weixin, yuanbao, qqbot, dingtalk, email, sms, webhook,
homeassistant, mattermost, bluebubbles, msgraph_webhook, api_server, …).
Upstream rehomed the Python adapters from `gateway/platforms/<name>.py` to
`plugins/platforms/<name>/adapter.py`; PoP annotations were re-pointed to
the new layout (`scripts/pop_repoint.py`). Adapter parity is tracked by the
scanner per module — `gateway` area coverage was 145/340 (43%) at the
latest run.

## Model providers

`src/agent/provider_*.c` (18 units: openai, anthropic, google, deepseek,
openrouter, azure, bedrock, xai, codex_responses, custom, …) plus the
adapter layers (`anthropic_adapter.c`, `gemini_*`, `lmstudio`, ollama,
together, fireworks, groq, perplexity, moonshot, copilot paths in the
provider registry). The provider-auth table is oracle-gated
(`provider_auth` — regenerated from LIVE Python `PROVIDER_REGISTRY` when
the last FAP was closed).

## Plugins

Native C plugin units in `src/plugins/` (`plugin_*.c`, ~30 units:
honcho, holographic/file memory, spotify, image_gen, video_gen,
security_guidance, dashboard_auth, context_engine, disk_cleanup, kanban,
observability, skills, achievements, browser, google_meet, …), built as
loadable `.so` plugins alongside the static tree. The Python plugin
modules' function-level parity is the `plugins` area of the scanner —
14% module coverage means most plugin functions are still REAL_GAP work.
The old version of this file claimed every plugin "✅ PORTED" via
`port_plugins_*.c` files that do not exist in the tree; that claim was
false and is retracted. Existence of a `plugin_<name>.c` unit proves a
native implementation exists, nothing more.

## Bundled libraries — the SLERM dependency front

`lib/` holds the project's own C libraries (libjson, libhttp, libyaml,
libwebsocket, libregex, libhash, libcrypto, libdb, liblineedit, libtui,
libomap, libglob, libdifflib, libansi, … — these are already ours) AND the
vendored third-party code that is the SLERM target list:

| Dependency | Today | SLERM target |
|---|---|---|
| whisper.cpp (+ ggml) | vendored prebuilt libs under `lib/whisper_cpp/lib/`, headers in `lib/whisper_cpp/include/` | **`lib/c11_whisper/` — ACTIVE**: pure-C11 model/math/encoder/compat units, linked into the binary when the whisper libs are present (`C11_WHISPER_OBJ` in `build/targets.mk`; the current build is "12M with whisper"). Finish the port, retire the vendored libs. |
| SQLite 3.45.1 | vendored amalgamation `lib/libdb/sqlite3.c` (8.7 MB) behind our own `db.c` layer | C11 engine SLERM — the db layer is ours; the engine underneath is work. |
| ncurses | vendored `lib/libncurses/` (+ `libcurses_widget`, `libtui`) | Retired by the Lua UI substrate (`src/lv/`, `src/lua/slv_ui.c`) — the ncurses TUI is a dead end on Windows. |
| ctranslate2 | `lib/ctranslate2_src/` slot (empty) | C11 translation backend — the slot exists, the implementation is the work. |
| OpenSSL, zlib, pcre2, tre, SDL2 | prebuilt under `C:/winlibs/extra-libs`, linked via `-L` | Long-horizon SLERM targets (crypto, inflate, regex, raster). The regex engine already has our own `lib/libregex` wrapper; full replacement is queued, never refused. |

The rule is identical to Python features: reference in, from-scratch C11
out, oracle/watercup proof, no escape hatches. A dependency we have not
started is REAL_GAP work on this front, not an exception.

## Optional skills

The Python optional-skill modules (blockchain clients, creative tools,
finance, health, MCP templates, migration, ML ops, productivity, research,
security) are scanned as part of the quarry; their function-level status is
in `docs/real-gap-list.md` like everything else. No blanket "all ported"
claim is made — the scanner is the only scoreboard.

## Verify, don't trust

```bash
python scripts/parity_coverage.py --dirs        # per-area module coverage
python scripts/parity_coverage.py --by-module   # every module, defs + importers
make parity                                     # live sentinels everywhere
bash tests/run_one_oracle.sh <port>             # behavioral proof per port
```
