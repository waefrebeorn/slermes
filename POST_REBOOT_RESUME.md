# Resume — current dev-box state and how to pick the work back up

Rewritten 2026-11-18. This replaces the 2026-11-05 post-reboot handoff (SAC
flip era), which is obsolete: the machine state it described no longer holds.

## Dev box (Windows 11)

- Repo: `C:\Users\eman5\slermes`, port branch **`integrate`** (push to
  `origin integrate`; `origin/main` is upstream history + docs).
- Python quarry: `C:\Users\eman5\hermes-agent` (upstream =
  NousResearch/hermes-agent). The scanner resolves ground truth through the
  package links beside the repo.
- Toolchain: MSYS2 bash + winlibs GCC (`C:/winlibs`), ccache warm. Every
  build/gate goes through one MSYS2 child that sources the env script:

```bash
/c/msys64/usr/bin/bash -lc 'source /c/Users/eman5/slermes/build/winlibs-env.sh; \
  cd /c/Users/eman5/slermes && make -j8 slermes \
  USERPROFILE="C:\Users\eman5" LOCALAPPDATA="C:\Users\eman5\AppData\Local"'
```

## Environment facts that shape every run

- **Smart App Control** refuses freshly linked images containing
  `lib/libhash/hash.o` (SHA-256), `lib/libregex/hermes_regex.o`, or
  `src/agent/auxiliary_client_runtime.o` (rc 127). Harnesses that need those
  link a local no-op stub for the *dependency* (never the code under test)
  and name the real object in DEPS. Prebuilt harnesses still run.
- `./slermes.exe --version` exits 127 with no output on this box even at
  clean HEAD — pre-existing CLI/environment behavior, not a build regression.
  The gates (compile + link + oracles) remain valid evidence.
- This box's shell strips `VAR=value cmd` env prefixes; pass per-invocation
  config as script arguments or `export` inside the child shell.
- Defender exclusions cover the repo, `.testbin`, and the Hermes local dir;
  freshly linked test binaries can still hit transient rc-126 refusals —
  retry with backoff before calling a suite blocked.

## Pick-up procedure (discover, don't synthesize)

```bash
cd C:/Users/eman5/slermes
git branch --show-current && git log --oneline -3 && git status --short
make parity                 # live scanner + watercup + walkway (fail-closed)
python tests/verify_build_health.py --no-link   # inside the MSYS2 env child
bash tests/run_all_oracles.sh                   # ORACLE_JOBS=1 on MSYS2
bash tests/run_slv_tests.sh
```

Then read `docs/WORK_DASHBOARD.md` §2 and take the top item. The loop:
scanner detail → grep before writing → implement/alias → oracle + watercup
proof → build → commit. One module at a time; never stop between gaps.

## Standing gates (all must be green before a commit is "done")

1. `make -j8 slermes` — reaches "Phase 5 complete".
2. `python tests/verify_build_health.py --no-link` — N/N clean, port-object
   gate PASS, cupboard check OK.
3. `bash tests/run_all_oracles.sh` — 0 mismatches; every "not run" names a
   real platform gap, never a silent skip.
4. `bash tests/run_slv_tests.sh` — slerm-Lua interpreter suites.
5. `make parity` — sentinels regenerated from live sources.

A gate that did not run is not a pass.
