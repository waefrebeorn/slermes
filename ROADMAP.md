# 🗺️ Slermes Roadmap — What's Next

<!-- PARITY:AUTO -->
**Version:** 0.19.0-slermes (v670, PORT phase)  
**Last updated:** 2026-08-08

> **v670 PORT phase:** live scanner 2026-08-08: 13,034 / 14,045 (92.8%) PORTED · 1,008 REAL_GAP · 3 PARTIAL. The C11 binary is the deliverable.
>
> **Upstream sync checkpoint:** 1,298 ahead / 754 behind upstream/main (last merge 2026-08-08 (upstream fetched)). The behind-count is the staleness timer; re-port the delta with the stash→pull→fix→pop workflow after each sync.
<!-- /PARITY:AUTO -->

> "Mission 1-8 complete" doesn't mean done — it means the foundation is laid.  
> The real work of making Slermes *the* C11 AI agent starts here.
>
` blocks).

---

## ✅ Done (Missions 1–8)

| Mission | Claim | Reality |
|---------|-------|---------|
| **5** | Docs serving (`/api/docs*`) | ✅ 6 endpoints, 749 upstream .md files served |
| **6** | Skills parser + `/api/skills` | ✅ 121 skills parsed from 77 SKILL.md files |
| **7** | Distribution (AppImage, Homebrew, NSIS, Docker, Nix, make install) | ✅ Packaging scripts exist, **multi-OS release CI just added** |
| **8** | Tests (API, CLI, state_db, UI) | ✅ 63+ test cases, 200+ unit tests |

---

## 🎯 Near-Term (v0.16–0.18)

### Release Engineering (v0.16)
| Priority | Item | Why |
|----------|------|-----|
| P0 | **GitHub Releases with multi-platform binaries** | Users should download a binary, not build from source |
| P0 | **macOS x86_64 build** (Intel Macs) | Current CI only builds ARM64 (native Apple Silicon) |
| P1 | **Windows native CI** (MSYS2/MinGW) in GitHub Actions | Cross-compile from Linux is fragile; native CI gives real coverage |
| P1 | **AppImage auto-build in CI** | Currently manual; should be part of every release |
| P2 | **Homebrew tap formula deploy** | Auto-update the formula on release |
| P2 | **Nix flake** (flake-based, not plain nix-build) | Modern Nix convention |

### CI/CD Hardening (v0.17)
| Priority | Item | Why |
|----------|------|-----|
| P0 | **Pass CI green on all platforms** | Currently macOS and cross-compile builds haven't been tested end-to-end |
| P1 | **Integration test suite in CI** | Currently only smoke tests (`--version`, `--help`) |
| P1 | **AddressSanitizer CI job** | Catch memory errors on every push |
| P1 | **Valgrind/memcheck CI job** | Deep memory analysis |
| P2 | **Coverage report + threshold gate in CI** | Prevent coverage regression |
| P2 | **Static analysis (cppcheck, clang-tidy)** | Catch bugs before runtime |

### Vault & Legacy Management (v0.18)
| Priority | Item | Why |
|----------|------|-----|
| P0 | **`vault/` directory for stale/inert code** | Move dead code out of active tree |
| P1 | **Mark stale `port_*.c` files with deprecation headers** | Clarity on what's truly active vs. heritage |
| P2 | **Upstream Python changelog sync** | Track what upstream changed that affects our C port |

---

## 🚀 Mid-Term (v0.19–0.24)

### Performance & Optimization
| Item | Notes |
|------|-------|
| **Thread pool for gateway platforms** | Currently serial dispatch; parallelize for multi-platform users |
| **Connection reuse / keepalive in libhttp** | Every API call opens a new TCP connection today |
| **LTO (Link-Time Optimization) builds** | Enable `-flto` in Makefile for smaller/faster binary |
| **Docker multi-arch images** | Push `linux/amd64`, `linux/arm64`, `linux/arm/v7` manifests |
| **Precompiled headers for build speed** | Current full rebuild ~30s; could be <10s |

### Testing Coverage
| Item | Notes |
|------|-------|
| **Fuzz harness in CI** | `tests/fuzz_*.c` exists but not run in CI |
| **Gateway platform E2E tests** | Mock server + real protocol tests per platform |
| **GUI harness tests** | SDL2 desktop GUI has no automated tests |
| **Performance regression benchmarks** | Track build time, binary size, startup time |

### Port Completeness
| Item | Notes |
|------|-------|
| **Audit all port_*.c vs upstream Python** | Find functions ported as stubs vs fully implemented |
| **Run upstream Python test suite against C port** | Cross-language verification |
| **Generate stub coverage report** | What % of Python signatures are covered |

---

## 🌟 Long-Term (v0.25+)

### Platform Expansion
| Platform | Status | Priority |
|----------|--------|----------|
| **FreeBSD** | ❌ Not built | Low |
| **Termux (Android)** | Partially detected in Makefile | Medium |
| **WebAssembly (WASM)** | ❌ Not explored | Low |
| **RISC-V Linux** | ❌ Not explored | Low |

### Feature Parity with Upstream
| Feature | Slermes | Upstream Hermes |
|---------|---------|-----------------|
| Electron desktop (470 TS files → ~30/111 GUI features) | 🟡 27% | ✅ |
| Multi-tenant gateway | ✅ | ✅ |
| Plugin system (19 .so files) | ✅ | ✅ |
| Cron scheduler | ✅ | ✅ |
| ACP protocol | ✅ | ✅ |
| MCP server | ✅ | ✅ |
| Petdex / floating pets | ✅ | ✅ |
| Voice (TTS/STT) | ✅ | ✅ |
| **Agent delegation** (sub-agents) | ✅ | ✅ |
| **Memory system** (long-term + working) | ✅ | ✅ |
| **Skills system** (SKILL.md parser + executor) | ✅ | ✅ |
| **Web dashboard** (React SPA) | ✅ | ✅ |
| **File browser + side-by-side preview** | ✅ | ✅ |
| **MCP client** (external tool servers) | ✅ | ✅ |
| **Computer Use** (browser + desktop automation) | ✅ port exists | ✅ |

### Known Gaps vs Upstream
| Gap | Impact |
|-----|--------|
| **Python code executor** (exec_code tool) | Currently delegates to `python3` on PATH; no sandboxed runtime |
| **Electron desktop parity** (file drag-drop, tray, notifications, system dialogs) | SDL2 desktop has 30/111 features; missing 81 |
| **Plugin discoverability** (store/browse/install via CLI) | `hermes tools` equivalent in C? |
| **`port_*.c` audit accuracy** | Many functions are stubs with PoP annotations only |

---

## 🏛️ Museum / Vault

Legacy content lives on the `vault` branch:

```bash
git checkout vault
```

Contains:
- Legacy `port_*.c` files that were replaced by native C implementations
- Old build scripts no longer used
- Historical notes and design decisions
- Upstream Python reference snapshots

**Commitment:** The vault is append-only and never rebased. Every tagged release links to the vault snapshot at that time.

---

## 📊 Key Metrics to Track

| Metric | Current | Target |
|--------|---------|--------|
| Binary size (Linux x86_64) | ~46 MB | <30 MB (with LTO + stripping) |
| Build time (4 cores) | ~30s | <15s |
| CI pass rate | Unknown (new multi-OS CI) | 100% on all platforms |
| Platform coverage | Linux only in CI | 5 platforms (Linux ×3, macOS, Windows) |
| Test count | 263+ | 500+ |
| CLI tools ported | ~45 | All upstream tools |
| Gateway platforms | 29 | 29 (parity) |

---

## How to Contribute

1. Pick something from the **Near-Term** list above
2. Open a PR — CI will build it on all platforms
3. Tag releases go through the Release workflow

```bash
# Create a release
git tag v0.16.0
git push origin v0.16.0
# GitHub Actions builds + publishes everything
```
