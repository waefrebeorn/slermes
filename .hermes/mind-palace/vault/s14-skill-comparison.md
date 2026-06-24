# S14 #6: Skill System Parity — Methodology Comparison

**Methodology:** Function-level diff of Python `agent/skill_commands.py` (523 lines, 7 public functions) + `tools/skills_tool.py` + `hermes_cli/skills_cli.py` vs C `src/agent/skill_commands.c` (426 lines, 7 public functions) + `src/tools/skills.c` (2,517 lines) + `src/skills_hub.c` (245 lines) + `src/tools/skill_mgmt.c` (1,003 lines) + `src/agent/skill_bundles.c` (235 lines).

Total: C ~4,730 lines (6 files) vs Python ~1,500+ lines (7+ files).

## Summary

**Verdict: PORTED (~80%)** — Core skill system (scanning, slug resolution, invocation message building, bundles, frontmatter parsing, template substitution) is fully ported. C has substantially MORE code in skills.c (2.5K vs Python's tools/skills_tool.py) covering sync, provenance, curator, cache, search, and dependency management that Python handles elsewhere.

## Skill Command Scanning

| Feature | Python | C | Status |
|---------|--------|---|--------|
| Scan ~/.hermes/skills/ for SKILL.md | `scan_skill_commands()` | `skill_cmd_scan()` | ✅ PORTED |
| Parse YAML frontmatter (name, description) | `_parse_frontmatter()` | `extract_fm()` inline parser | ✅ PORTED |
| Name → slug normalization | lowercase, spaces→hyphens, strip invalid | `name_to_slug()` — same algorithm | ✅ PORTED |
| Skip `.git/`, `.github/`, `.archive/` dirs | Part-startsWith filter | No skip filter | ❌ PARTIAL |
| Platform filtering | `skill_matches_platform()` for gateway | Not implemented | ❌ REAL GAP |
| Disabled-skills filtering | `_get_disabled_skill_names()` from config | `skill_cmd_is_disabled()` | ✅ PORTED |
| External skills dirs | `get_external_skills_dirs()` from config | Not implemented | ❌ REAL GAP |
| Description fallback from body | First non-header line from body | Fallback to first body line in `read_skill_file()` — truncates at 120 chars or sentence boundary | ✅ PORTED (SK04) |
| Dedup by name across dirs | `seen_names` set across scan dirs | Not applicable (single dir) | ⚠️ Single-dir limitation |

## Skill Command Resolution

| Feature | Python | C | Status |
|---------|--------|---|--------|
| Get all commands | `get_skill_commands()` | `skill_cmd_get_all()` | ✅ PORTED |
| Get one command | Dict lookup by slug | `skill_cmd_get(slug)` | ✅ PORTED |
| Resolve user input to canonical slug | `resolve_skill_command_key()` — underscore↔hyphen | `skill_cmd_resolve()` — same logic | ✅ PORTED |
| Lazy scan on first access | Yes (`get_skill_commands()` checks empty) | No (caller must scan first) | ❌ PARTIAL |
| Platform-scoped cache invalidation | Rescans when platform changes | Not implemented | ❌ REAL GAP |
| Cached with TTL | No explicit TTL (reload triggers rescan) | `g_last_scan` time tracked | ⚠️ PARTIAL |

## Skill Invocation Message Building

| Feature | Python | C | Status |
|---------|--------|---|--------|
| Load skill payload | `_load_skill_payload()` — calls skill_view | `skill_cmd_build_message()` — reads SKILL.md directly | ✅ PORTED |
| Strip YAML frontmatter | Via `_parse_frontmatter()` | `context_strip_frontmatter()` | ✅ PORTED |
| Activation note | `[IMPORTANT: The user has invoked the "X" skill...]` | Same note format | ✅ PORTED |
| User instruction append | Appended at end | Appended at end | ✅ PORTED |
| Skill directory hint | `[Skill directory: /path]` + resolution advice | Same hint format | ✅ PORTED |
| Supporting files listing | Recursive rglob of refs/templates/scripts/assets | Same 4 subdirs, one level deep | ✅ PORTED |
| Template variable substitution | `_substitute_template_vars()` | In `skill_preprocessing.c` | ✅ PORTED |
| Inline shell expansion | `_expand_inline_shell()` | In `skill_preprocessing.c` | ✅ PORTED |
| Skill config injection | `_inject_skill_config()` — resolves config.yam lkeys | Not implemented | ❌ REAL GAP |
| Setup notes (skipped, gateway, generic) | 3 conditional note blocks | Not implemented | ❌ REAL GAP |
| Skill usage tracking (curator) | `bump_use()` via `tools/skill_usage.py` | Via `skills.c` usage tracking | ✅ PORTED |
| `skill_view()` URL/file_path hint | Included in supporting files block | Same hint format | ✅ PORTED |

## Reload & Diff Tracking

| Feature | Python | C | Status |
|---------|--------|---|--------|
| Re-scan skills | `reload_skills()` calls `scan_skill_commands()` | `skill_cmd_rescan()` | ✅ PORTED |
| Return added names | Yes — `added: [{name, description}]` | Count only (`*added` int) | ❌ PARTIAL |
| Return removed names | Yes — `removed: [{name, description}]` | Count only (`*removed` int) | ❌ PARTIAL |
| Return unchanged names | Yes — `unchanged: [names]` | Not returned | ❌ PARTIAL |
| Return total/count | Yes — `total`, `commands` | Not returned | ❌ PARTIAL |

## Skill Bundles

| Feature | Python | C | Status |
|---------|--------|---|--------|
| Scan bundles YAML dir | `get_skill_bundles()` | `skill_bundle_scan_all()` | ✅ PORTED |
| Parse YAML bundle format | YAML → name/description/alias | YAML → name/description/slug | ✅ PORTED |
| Bundle resolution | Resolves alias → target command | Not implemented (direct slug only) | ❌ REAL GAP |
| Bundle invocation | `build_bundle_invocation_message()` | Not in skill_bundles.c | ❌ REAL GAP |

## Curator & Lifecycle

| Feature | Python | C | Status |
|---------|--------|---|--------|
| Usage tracking | `tools/skill_usage.py` — `bump_use()`, `get_stats()` | In `skills.c` | ✅ PORTED |
| Stale detection | Curator: 90-day stale threshold | `SKILL_CURATOR_DEFAULT_STALE_DAYS 90` | ✅ PORTED |
| Archive/Purge | Curator lifecycle management | In `skill_mgmt.c` | ✅ PORTED |
| Hub catalog | Not implemented in Python | `skills_hub.c` — browse.sh catalog with cache | ✅ C-unique |

## Verdict

**PORTED (~85%)** — The core skill system is fully ported and actually more comprehensive in C (2,517-line skills.c covers many features Python delegates to multiple modules + the skills hub for remote catalog browsing). Description fallback from body resolved (SK04). Key missing features are:

1. **Platform filtering** — Python filters skills incompatible with the current gateway platform (`skill_matches_platform()`)
2. **External skills directories** — Python supports `skills.external_dirs` config key
3. **Skill config injection** — Python resolves skill-declared config keys from config.yaml and injects them into the invocation message
4. **Setup/skip notes** — Python injects conditional setup-failed/gateway-setup-hint blocks
5. **Reload returns names** — C returns only counts, Python returns full name lists
6. **Bundle alias resolution** — C bundles don't support defining aliases for other commands

**Evidence:** Python `agent/skill_commands.py:263-523` (full module), `tools/skills_tool.py`. C `src/agent/skill_commands.c:188-426` (public API), `src/tools/skills.c`, `src/tools/skill_mgmt.c`, `src/skills_hub.c`, `src/agent/skill_bundles.c`.
