#!/usr/bin/env python3
"""sta_oracle_wave_c11_gaps.py -- differential oracle for the C11 wave re-ports.

LIVE Python truth is ast-extracted from the READ-ONLY quarry.  The C harness
emits the exact fixtures (zip paths, umasks, state-file dirs, lines) it used;
this oracle binds the live function to those same fixtures and compares.
"""
import ast
import json
import os
import shutil
import stat
import sys
import tempfile
import zipfile
from pathlib import Path
from typing import Optional

HQ = "C:/Users/eman5/hermes-agent"


def bind(module: str, func: str, ns: dict):
    path = f"{HQ}/{module}"
    with open(path, encoding="utf-8") as fh:
        text = fh.read()
    tree = ast.parse(text)
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name == func:
            exec(compile(ast.get_source_segment(text, node), path, "exec"), ns)
            return ns[func]
    print(f"ORACLE wave_c11_gaps: FATAL, {func} not found in {module}")
    raise SystemExit(1)


# ── live functions, each in its own namespace ──
ns_dnfm = {"os": os, "Optional": Optional}
live_dnfm = bind("hermes_cli/backup_restore.py", "_default_new_file_mode", ns_dnfm)

ns_vbz = {"Path": Path}
live_vbz = bind("hermes_cli/backup_restore.py", "_validate_backup_zip", ns_vbz)


def _preserve_mode(p):
    try:
        return stat.S_IMODE(os.stat(p).st_mode)
    except OSError:
        return None


def _preserve_owner(p):
    return None


def _mkstemp_beside(target, prefix, suffix):
    return tempfile.mkstemp(dir=str(Path(target).parent), prefix=prefix, suffix=suffix)


def _atomic_replace(tmp, target):
    os.replace(tmp, target)
    return str(target)


def _restore_owner(p, owner):
    pass


def _restore_mode(p, mode):
    if mode is not None:
        os.chmod(p, mode)


ns_ema = {
    "os": os, "shutil": shutil, "stat": stat, "Path": Path,
    "Optional": Optional, "zipfile": zipfile,
    "preserve_file_mode": _preserve_mode,
    "preserve_file_owner": _preserve_owner,
    "mkstemp_beside": _mkstemp_beside,
    "atomic_replace": _atomic_replace,
    "restore_file_owner": _restore_owner,
    "restore_file_mode": _restore_mode,
}
live_ema = bind("hermes_cli/backup_restore.py", "_extract_member_atomically", ns_ema)

ns_sent = {"os": os}
live_sent = bind("hermes_cli/web_server_lifecycle.py", "_write_machine_sentinel_line", ns_sent)

ns_gstop = {"_json": json, "Path": Path, "Optional": Optional}


def _cron_profile_home(profile):
    return "default", Path(os.environ.get("ORACLE_WAVE_HOME", "."))


ns_gstop["_cron_profile_home"] = _cron_profile_home
live_gstop = bind("hermes_cli/web_server_cron.py", "_gateway_intentionally_stopped", ns_gstop)

cases = mismatches = 0


def check(name, got, want):
    global cases, mismatches
    cases += 1
    if got != want:
        mismatches += 1
        print(f"  MISMATCH {name}: got={got!r} want={want!r}")


for raw in sys.stdin:
    raw = raw.strip()
    if not raw:
        continue
    c = json.loads(raw)
    fn = c["fn"]

    if fn == "dnfm":
        mask = c["mask"]
        before = os.umask(mask)
        try:
            want = live_dnfm()
        finally:
            os.umask(before)
        check(f"dnfm(mask={mask:o})", c["out"], want)

    elif fn == "vbz":
        with zipfile.ZipFile(c["path"]) as zf:
            ok, _reason = live_vbz(zf)
        check(f"vbz({c['id']})", c["out"], 1 if ok else 0)

    elif fn == "ema":
        tmpd = tempfile.mkdtemp(prefix="wave_oracle_ema_")
        target = Path(tmpd) / "restored.bin"
        want_rc, want_bytes = 0, ""
        try:
            with zipfile.ZipFile(c["path"]) as zf:
                live_ema(zf, c["member"], target)
            if target.is_file():
                want_rc = 1
                want_bytes = target.read_bytes().decode("utf-8", errors="replace")
        except Exception:
            want_rc, want_bytes = 0, ""
        check(f"ema({c['id']}).rc", c["rc"], want_rc)
        check(f"ema({c['id']}).bytes", c["bytes"], want_bytes)
        shutil.rmtree(tmpd, ignore_errors=True)

    elif fn == "sentinel":
        captured = []
        orig_write = os.write

        def fake_write(fd, data):
            if fd == 1:
                captured.append(data)
                return len(data)
            return orig_write(fd, data)

        os.write = fake_write
        try:
            live_sent(c["line"])
        finally:
            os.write = orig_write
        want = b"".join(captured).decode("utf-8", errors="replace")
        check(f"sentinel({c['line']!r})", c["out"], want)

    elif fn == "gstop":
        os.environ["ORACLE_WAVE_HOME"] = c["home"]
        want = 1 if live_gstop(None) else 0
        check(f"gstop({c['content']!r})", c["out"], want)

    else:
        mismatches += 1
        print(f"  MISMATCH unknown fn {fn!r}")

print(f"ORACLE wave_c11_gaps: {cases} cases, {mismatches} mismatches")
raise SystemExit(0 if mismatches == 0 else 1)
