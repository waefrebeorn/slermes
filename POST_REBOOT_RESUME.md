# POST-REBOOT RESUME (2026-11-05)

Written just before the machine was rebooted to activate the Smart App Control
flip. After restart, run:

1. Smoke: `./slermes.exe --version`  (previously blocked; should now print)
2. Full gates:
   `cd C:\Users\eman5\slermes && /c/msys64/usr/bin/bash -lc "export PATH=/mingw64/bin:/usr/bin:$PATH; cd /c/Users/eman5/slermes && python tests/verify_build_health.py"`
   `bash tests/run_mission8_tests.sh`
3. Finish the two oracle runs marked PENDING in their commits (they were
   blocked because SAC refused every fresh link at every -O):

   - `tests/t_port_image_token_cost.c` + `tests/sta_oracle_image_token_cost.py`
     commit `2c895e4` — port VERIFIED 23/0 already (a single -O0 link slipped
     through post-flip); re-run to confirm.
   - `tests/t_port_session_activity.c` + `tests/sta_oracle_session_activity.py`
     commit `3c06209` — oracle written but run BLOCKED, still unverified.
     Run with: `MINIMAL_OBJS='src/cli/port_session_activity.o
     lib/libjson/json.o' bash tests/run_one_oracle.sh session_activity`

## Admin state already in place
- SAC policy registry `VerifiedAndReputablePolicyState = 0` (needs the reboot
  to load; `CodeIntegrityPolicyEnforcementStatus` was still 2 before it).
  One-way flip — re-enabling requires a Windows reinstall.
- Defender exclusions added (survive reboot) for `C:\Users\eman5\slermes`,
  `.testbin`, and `C:\Users\eman5\AppData\Local\hermes` — these should stop
  the flaky rc-126 refusals of freshly linked test binaries so the oracle loop
  stops needing the `-O0` lottery.

## Repo state at handoff
HEAD `3c06209` (`session_activity` ported, 7 stubs removed).
- placeholders (parity_reality): implemented 3289, placeholder-only 630,
  missing 31939. Build health (last pre-reboot): 744/744, LINK OK.
- Tree clean except untracked `.testbin/` artifacts and `trajectory_samples.jsonl`.

## Method reminder for the oracle loop
`tests/run_one_oracle.sh <name>` compiles `tests/t_port_<name>.c` with
`MINIMAL_OBJS` when set. If Smart App Control still refuses a fresh link
(rc 126), relink at `-O0` under a new `.exe` name and feed its stdout into
`tests/sta_oracle_<name>.py` — but with the exclusions + SAC off this should
no longer be needed.
