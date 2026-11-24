#!/usr/bin/env python3
"""Differential oracle for tools/bot_desktop/resources.py (gate w26_resources_caddbd).

Every file read in the live module is a seam; each seam is replaced by a fixed
input taken from the request, so the *live* code still computes the answer:

  * ``_read_int`` / ``_meminfo`` / ``_stat_value``  -- fed a real temp file
    whose text the fixture supplies;
  * ``_read_int`` / ``_stat_value`` -- also table-substituted so
    ``_cgroup_limit_and_usage`` runs unmodified over the fixture's numbers;
  * ``_meminfo`` / ``_cgroup_limit_and_usage`` -- substituted so ``memory_info``
    runs unmodified (the fixture's ``meminfo`` is already in bytes, the shape
    ``_meminfo`` returns);
  * ``hermes_cli.config.load_config_readonly`` -- replaced by the fixture's
    config object so ``min_free_mb`` runs unmodified;
  * ``memory_info`` / ``min_free_mb`` -- substituted so ``memory_blocker``
    runs unmodified.
"""
import json
import shutil
import sys
import tempfile
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w26_resources_caddbd"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from tools.bot_desktop import resources as live  # noqa: E402
import hermes_cli.config as hcfg  # noqa: E402

WANTED = {"read_int", "meminfo", "stat_value", "cgroup", "memory_info",
          "min_free_mb", "tight_headroom_mb", "memory_blocker", "constants"}

_MISSING = object()
_TMP = Path(tempfile.mkdtemp(prefix="w26res-"))
_ORIG_MEMORY_INFO = live.memory_info
_ORIG_MIN_FREE = live.min_free_mb
_ORIG_CGROUP = live._cgroup_limit_and_usage


def _tmpfile(name, text):
    p = _TMP / name
    p.write_text(text, encoding="utf-8")
    return p


def read_int(req):
    return live._read_int(_tmpfile("read_int.txt", req.get("text") or ""))


def meminfo(req):
    saved = live._MEMINFO
    live._MEMINFO = _tmpfile("meminfo.txt", req.get("text") or "")
    try:
        return live._meminfo()
    finally:
        live._MEMINFO = saved


def stat_value(req):
    return live._stat_value(_tmpfile("memory.stat", req.get("text") or ""),
                            req.get("key") or "")


def _cgroup_values(req):
    c = req.get("cgroup")
    c = c if isinstance(c, dict) else {}
    version = c.get("version", 2)
    limit, usage, cache = c.get("limit"), c.get("usage"), c.get("cache")
    if version == 1:
        ints = {"memory.limit_in_bytes": limit, "memory.usage_in_bytes": usage}
        stat = {"total_inactive_file": cache}
    else:
        ints = {"memory.max": limit, "memory.current": usage}
        stat = {"inactive_file": cache}
    return ints, stat


def cgroup(req):
    ints, stat = _cgroup_values(req)

    def fake_read_int(path):
        return ints.get(Path(path).name)

    def fake_stat_value(path, key):
        return stat.get(key)

    saved = (live._read_int, live._stat_value)
    live._read_int, live._stat_value = fake_read_int, fake_stat_value
    try:
        return list(_ORIG_CGROUP())
    finally:
        live._read_int, live._stat_value = saved


def _memory_info_live(req):
    raw = req.get("meminfo")

    def fake_meminfo():
        return dict(raw) if isinstance(raw, dict) else {}

    saved = (live._meminfo, live._cgroup_limit_and_usage)
    live._meminfo = fake_meminfo
    live._cgroup_limit_and_usage = lambda: tuple(cgroup(req))
    try:
        return _ORIG_MEMORY_INFO()
    finally:
        live._meminfo, live._cgroup_limit_and_usage = saved


def memory_info(req):
    info = _memory_info_live(req)
    return {"available_mb": info.available_mb, "limit_mb": info.limit_mb}


def _config_loader(req):
    """The fixture's ``cfg`` is the ``bot_desktop`` section, not the whole file."""
    cfg = req.get("cfg")
    section = cfg if isinstance(cfg, dict) else {}
    return lambda: {"bot_desktop": section}


def _min_free_live(req):
    saved = hcfg.load_config_readonly
    hcfg.load_config_readonly = _config_loader(req)
    try:
        return _ORIG_MIN_FREE()
    finally:
        hcfg.load_config_readonly = saved


def min_free_mb(req):
    return _min_free_live(req)


def tight_headroom_mb(req):
    floor = req.get("floor")
    saved_cfg = hcfg.load_config_readonly
    hcfg.load_config_readonly = _config_loader(req)
    try:
        return live.tight_headroom_mb(floor)
    finally:
        hcfg.load_config_readonly = saved_cfg


def memory_blocker(req):
    raw = req.get("info", _MISSING)
    if raw is _MISSING or raw is None:
        info = None
    else:
        info = live.MemoryInfo(available_mb=(raw or {}).get("available_mb"),
                               limit_mb=(raw or {}).get("limit_mb"))
    saved = (live.min_free_mb, live.memory_info)
    live.min_free_mb = lambda: _min_free_live(req)
    live.memory_info = lambda: _memory_info_live(req)
    try:
        return live.memory_blocker(info, req.get("need"))
    finally:
        live.min_free_mb, live.memory_info = saved


def constants():
    return {"default_min_free_mb": live.DEFAULT_MIN_FREE_MB,
            "mib": float(live._MIB),
            "v1_unlimited_sentinel": float(1 << 60)}


DISPATCH = {
    "read_int": read_int,
    "meminfo": meminfo,
    "stat_value": stat_value,
    "cgroup": cgroup,
    "memory_info": memory_info,
    "min_free_mb": min_free_mb,
    "tight_headroom_mb": tight_headroom_mb,
    "memory_blocker": memory_blocker,
    "constants": lambda req: constants(),
}


def main():
    cases = mism = 0
    try:
        for line in sys.stdin:
            line = line.strip()
            if not line:
                continue
            try:
                r = json.loads(line)
            except ValueError:
                continue
            if not isinstance(r, dict) or r.get("func") not in WANTED:
                continue
            cases += 1
            try:
                py = DISPATCH[r["func"]](r)
            except Exception as exc:  # noqa: BLE001
                mism += 1
                print("MISMATCH %s case=%r live-error=%r"
                      % (r.get("func"), r.get("case"), exc), file=sys.stderr)
                continue
            got = r.get("out")
            if py != got:
                mism += 1
                print("MISMATCH %s case=%r py=%r c=%r"
                      % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    finally:
        shutil.rmtree(_TMP, ignore_errors=True)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
