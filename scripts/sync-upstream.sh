#!/usr/bin/env bash
# sync-upstream.sh — refresh the Python quarry from NousResearch/hermes-agent.
#
# Layout (Windows dev box): the quarry is a DIRECT clone of
# NousResearch/hermes-agent at ~/hermes-agent (origin = NousResearch).
# slermes (this repo) is an independent C11 repo — it never pushes code back
# upstream. Parity scanners/oracles/watercups read the quarry LIVE, so a
# quarry pull immediately re-targets every gate.
#
# Usage:
#   bash scripts/sync-upstream.sh           # fetch + report the gap (safe)
#   bash scripts/sync-upstream.sh --now     # also fast-forward the quarry
#
# SAFETY: never pull while a watercup fleet or oracle suite is running —
# the gates execute the quarry's Python files; changing them mid-run
# corrupts verdicts. --now refuses when runner processes are detected.
set -euo pipefail

NOW=0
[ "${1:-}" = "--now" ] && NOW=1

# MSYS2 login shells do not inherit Windows env vars, and $HOME is the
# MSYS home (/home/<user>), not the Windows profile. Probe real locations.
WINHOME=""
for cand in "${USERPROFILE:-}" "/c/Users/eman5" "$HOME"; do
  if [ -n "$cand" ] && [ -d "$cand/AppData/Local" ]; then WINHOME="$cand"; break; fi
done
[ -n "$WINHOME" ] || WINHOME="/c/Users/eman5"
LAPP="$WINHOME/AppData/Local"

QUARRY="${SLERMES_QUARRY:-}"
if [ -z "$QUARRY" ]; then
  for cand in "$WINHOME/hermes-agent" "$HOME/hermes-agent"; do
    if [ -d "$cand/.git" ]; then QUARRY="$cand"; break; fi
  done
fi

# Portable git (WindowsApps blocks the bundled one); fall back to PATH git.
GIT="git"
PGIT="$LAPP/hermes/tools/git-2.53.0-win32-x64/mingw64/bin/git.exe"
if [ -x "$PGIT" ]; then
  GIT="$PGIT"
  export GIT_EXEC_PATH="$LAPP/hermes/tools/git-2.53.0-win32-x64/mingw64/libexec/git-core"
fi
unset GIT_CONFIG_GLOBAL GIT_CONFIG_SYSTEM || true
export GIT_TERMINAL_PROMPT=0
if [ -f "$LAPP/hermes/tools/askpass.exe" ]; then
  export GIT_ASKPASS="$LAPP/hermes/tools/askpass.exe"
fi

if [ ! -d "$QUARRY/.git" ]; then
  echo "ERROR: quarry not found at $QUARRY (set SLERMES_QUARRY to override)" >&2
  exit 2
fi
cd "$QUARRY"

echo "-> Fetching upstream (NousResearch/hermes-agent)..."
"$GIT" -c credential.helper= fetch origin 2>&1 | tail -2 || true

BRANCH="$("$GIT" branch --show-current)"
COUNTS="$("$GIT" rev-list --left-right --count "HEAD...origin/$BRANCH")"
AHEAD="$(echo "$COUNTS" | cut -f1)"
BEHIND="$(echo "$COUNTS" | cut -f2)"
echo "  quarry $BRANCH: $AHEAD ahead / $BEHIND behind origin/$BRANCH"

if [ "$NOW" = "0" ]; then
  echo "  (report only — pass --now to fast-forward)"
  exit 0
fi

# Fleet/suite guard: refuse to move the Python under running gates.
if powershell -NoProfile -Command     "Get-CimInstance Win32_Process | Where-Object { \$_.CommandLine -match 'wc_run|run_all_oracles|run_one_oracle' } | Select-Object -First 1 -ExpandProperty ProcessId"     2>/dev/null | grep -q '[0-9]'; then
  echo "ERROR: watercup/oracle runners are active — pulling now would corrupt verdicts." >&2
  echo "       Wait for the fleet to finish, then re-run with --now." >&2
  exit 3
fi

if [ -n "$("$GIT" status --porcelain | grep -v '^??' | head -1)" ]; then
  echo "ERROR: quarry working tree is dirty — commit/stash first." >&2
  exit 4
fi
if [ "$AHEAD" != "0" ]; then
  echo "ERROR: quarry has local commits ($AHEAD) — refusing non-ff pull." >&2
  exit 5
fi

echo "-> Fast-forwarding $BRANCH to origin/$BRANCH ($BEHIND commits)..."
"$GIT" merge --ff-only "origin/$BRANCH"
echo "OK: quarry updated. Next: re-run 'make parity' and the oracle suite in slermes"
echo "  (every gate now targets the new upstream Python)."
