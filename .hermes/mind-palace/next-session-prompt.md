# Next Session Prompt — Copy-Paste Ready ()

---

```
Goal: Close function-level parity gaps (REAL_GAP) across ALL Python modules —
and continue feature/API parity for desktop, web, skills, docs, scripts, tests,
configs. Rewriting from scratch in C is the point; there is NO N/A.

Form-not-function gap: any C function whose body is (a) a hardcoded return
default with an excuse comment ("actual checks done at Python layer", "in C
core", "for brevity"), (b) printf+return-0 echo, or (c) a PoP annotation with
an empty/stub body — is a REAL_GAP. Vault + close, do not hand-wave.

Last session closed: terminal env registry (terminal_tool.py 61/64, was fake),
systemd_notify watchdog (13/13, was printf-echo stubs). Build is clean,
all new symbols ship as T in the binary (nm-verified).

Live counts:
| REAL_GAP| 742 (5.3%) — no N/A     |
| PARTIAL | 17 (0.1%)               |
| BOOTLEG | 3 (recursive_false_gap_hunter.py) |

**Phase ():** PORT phase — the C11 binary is the deliverable.
**Upstream sync:** 1, upstream/main (last merge ).

## Next Session Targets

1. **Stub hunt frontier.** Re-run the form-not-function line scan
   (`grep -rE 'always returns|return strdup\("|return \{\}|no-op in C|actual checks done|/home/wubu/hermes'`).
   The pattern still finds fake returns — push the count DOWN, aim for 0 across
   src/ + include/ (lib/ excluded as vendored).

2. **Daemon parity (WuBuOS Desktop).** Action the REAL_GAPs from
   `mind-palace/wubuos-daemon-parity-da.md`:
   - Ship `hermes-gateway.service` + `hermes-desktop.service` unit files
     (Type=notify, WatchdogSec=30, Restart=on-failure, RestartSec backoff,
     EnvironmentFile=/etc/hermes.conf).
   - XDG autostart: `~/.config/autostart/wubuos-desktop.desktop` launched from
     the desktop GUI init path.
   - PID file `/run/hermes-gateway.pid` in gateway_runtime.c.
   - Restart backoff state machine (mirror Ubuntu's RestartSec escalation).

3. **Mission 3 (Tools) lead target.** `tools/process_registry.py` — 32 REAL_GAPs,
   the single heaviest module. Port the missing process-table queries/lifecycle
   (already partially in process_registry.c; the gap is the env-override +
   isolation-key plumbing from the terminal registry you just built).

4. **Bootleg census.** Run `tests/recursive_false_gap_hunter.py` — confirm the s are not regressing and no new echo-stubs appeared.

## Ritual
Every commit: real impl (no hermes_log+return NULL), PoP annotation matching the
Python def exactly, `make slermes` clean, `nm` confirms symbol as T, re-run
`make parity-walkway` to re-stamp all docs, vault in `achievements.md`.

_Generated 2026-08-09T06:01:00Z — live scanner `tests/slermes_parity_battleground.py`._
```

<!-- PARITY:AUTO -->
| PORTED  | 6,466 / 18,219 (35.5%) |
| REAL_GAP| 11,715 (64.3%) — no N/A |
|  ├ DA-4 annotation-only stubs | 11 (PoP comment, no C body) |
|  └ Genuine gaps (no C at all) | 11,704 |
| PARTIAL | 38 (0.2%) |
| BOOTLEG | 0 (recursive_false_gap_hunter.py) |
| **Watercup** | 313 PASS / 22,604 total (1.4%) — 22,291 fail |

**Phase (v672):** PORT phase — the C11 binary is the deliverable: faithful, oracle-verified, usable standalone across operating systems. Closing REAL_GAPs is the path; the AGI-OS integration consumes the binary, not the Python tree.

**Upstream sync checkpoint:** 2,072 ahead / 2,560 behind upstream/main (last merge 2026-09-14 (upstream fetched)). The behind-count is the staleness timer — see the stash→pull→fix→pop workflow below.

_Generated 2026-09-15T04:34:33Z from live scanner `tests/slermes_parity_battleground.py` + watercup oracle (22,604 trace-cup gates) — do not edit by hand; run `make parity-walkway`._
<!-- /PARITY:AUTO -->
