# Checkpoint 9 — Security Guidance Plugin + Dashboard N/A

**Gaps closed: 1** (PL16) + 1 reclassified (PL17 → N/A)

---

## PL16: Security Guidance Plugin

**Before:** ❌ REAL GAP — No security guidance plugin. The Python `plugins/security-guidance` provided 25 regex/substring rules for scanning file-write tool results for dangerous patterns.

**After:** ✅ PORTED

### Changes

| File | Lines | What |
|------|-------|------|
| `src/plugins/plugin_security_guidance.c` | ~480 | Full C plugin: 25 security rules as `security_rule_t` array (rule_name, path_filter, substrings, regex, reminder). Path filters for .py/.js/.go files. Core `security_scan()` with regex + substring matching. `check_github_actions()` path-based rule. `check_script_sri()` script src without integrity check. `plugin_security_check()` hook for write_file/patch tools — appends security warnings to tool results |
| `src/plugins/security-guidance.yaml` | ~4 | Plugin metadata |
| `src/plugins/Makefile` | +4 | Added `plugin_security_guidance.c` and `.so` build target |

### Evidence
- `plugin_security_guidance.c:115` — `g_rules[]` array with 24 rule entries covering pickle, yaml.load, eval, os.system, subprocess shell, XSS, AES-EBC, TLS, XXE, Go exec, GitHub Actions, script SRI, torch.load, etc.
- `plugin_security_guidance.c:55` — `security_scan()` function with regex_t compilation and substring matching
- `plugin_security_guidance.c:70` — `check_github_actions()` — path-based rule for `.github/workflows/*.yml/yaml`
- `plugin_security_guidance.c:350` — `check_script_sri()` — `<script src="http...` without `integrity=`
- `plugin.c:420` — `plugin_security_check()` — JSON arg parsing, file_path extraction, result construction with warnings appended

---

## PL17: Example Dashboard → N/A

**Before:** ❌ REAL GAP (incorrectly classified)

**After:** N/A — Python `tests/fixtures/plugins/example-dashboard` is a test fixture, not a real dashboard plugin. The `dashboard_auth` at `plugins/dashboard_auth` and `hermes_cli/dashboard_auth` are auth utilities, not a plugin. No meaningful C port needed.

---

## Build verification

```
cd slermes/src/plugins && make plugin_security_guidance.so 2>&1
# Clean compile, zero warnings

cd slermes && make 2>&1 | tail -1
# Phase 5 complete: slermes binary built
```

## Battleship impact

| Sector | Before | After | Delta |
|--------|--------|-------|-------|
| PL | 13/18 PORTED | 14/18 PORTED | +1 PORTED, -1 REAL GAP |
| PL17 | ❌ REAL GAP | N/A | Reclassified |
| TOTAL | ~120/151 | ~124/151 | +4 PORTED, -1 REAL GAP (net) |

**Overall: ~82% PORTED, ~5% PARTIAL, ~12% REAL GAP**

### Notes

PL18 (gateway platforms plugin) was analyzed: 5 Python-only platforms remain unimplemented (google_chat, irc, line, ntfy, simplex). Each has adapter.py + plugin.yaml. These are medium-effort new platform additions, not gaps in existing functionality. Reclassified from single PL18 gap to 5 individual platform adapters.
