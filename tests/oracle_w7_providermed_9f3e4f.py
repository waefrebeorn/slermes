#!/usr/bin/env python3
"""Differential oracle for agent/provider_media.py (w7_providermed_9f3e4f gate)."""
import importlib.util, json, pathlib, sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
PYROOT = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, PYROOT)
SRC = PYROOT + "/agent/provider_media.py"
spec = importlib.util.spec_from_file_location("live_provider_media_w7", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)

# Freeze the nondeterministic inputs to cache_path on the live module.
FROZEN = {"home": "HOMEROOT", "ts": "20260102_030405", "short": "ab12cd34"}

class Now:
    def strftime(self, fmt):
        return FROZEN["ts"]

class FakeDatetime:
    class datetime:
        @staticmethod
        def now():
            return Now()

class FakeUuid:
    class _U:
        hex = "ab12cd34"
    @staticmethod
    def uuid4():
        u = FakeUuid._U()
        u.hex = FROZEN["short"]
        return u

live.datetime = FakeDatetime
live.uuid = FakeUuid
import hermes_constants
hermes_constants.get_hermes_home = lambda: pathlib.Path(FROZEN["home"])
pathlib.Path.mkdir = lambda self, *a, **k: None  # cache_dir must not touch disk in the oracle

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    func = r.get("func")
    try:
        if func == "subdir":
            py = live.GENERATED_SUBDIR
        elif func == "max_age":
            py = live.MEDIA_CACHE_MAX_AGE_HOURS
        elif func == "max_redirects":
            py = live._MAX_SAVE_URL_REDIRECTS
        elif func == "redirect":
            py = r.get("code") in live._REDIRECT_STATUS_CODES
        elif func == "cache_path":
            FROZEN["home"] = r.get("home", "HOMEROOT")
            FROZEN["ts"] = r.get("ts", "20260102_030405")
            FROZEN["short"] = r.get("short", "ab12cd34")
            py = str(live.cache_path(r.get("kind", ""), r.get("prefix", ""), r.get("extension", "")))
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w7_providermed_9f3e4f: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
