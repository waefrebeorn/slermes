#!/usr/bin/env python3
"""Differential oracle for upstream Hermes gateway/run_inbound_media (noc_w11_runinbound_aaf63b.h)."""
import importlib.util
import json
import os
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, QUARRY)


def _load(modpath):
    parts = modpath.split("/")
    for i in range(1, len(parts)):
        pname = ".".join(parts[:i])
        if pname in sys.modules:
            continue
        pdir = os.path.join(QUARRY, *parts[:i])
        init = os.path.join(pdir, "__init__.py")
        sp = importlib.util.spec_from_file_location(pname, init, submodule_search_locations=[pdir])
        pm = importlib.util.module_from_spec(sp)
        sys.modules[pname] = pm
        try:
            sp.loader.exec_module(pm)
        except Exception:
            pass
    name = ".".join(parts)
    sp = importlib.util.spec_from_file_location(name + "_live", os.path.join(QUARRY, modpath + ".py"))
    live = importlib.util.module_from_spec(sp)
    sys.modules[name + "_live"] = live
    sp.loader.exec_module(live)
    return live


def _stub_heavy():
    if "gateway" not in sys.modules:
        g = types.ModuleType("gateway")
        g.__path__ = [os.path.join(QUARRY, "gateway")]
        sys.modules["gateway"] = g
    stubs = (
        ("gateway.platforms", {}, [os.path.join(QUARRY, "gateway", "platforms")]),
        ("gateway.platforms.event", {"MessageEvent": type("MessageEvent", (), {})}, []),
        ("gateway.session", {"SessionSource": type("SessionSource", (), {}),
                             "build_session_context": lambda *a, **k: None}, []),
    )
    for full, attrs, paths in stubs:
        if full not in sys.modules:
            m = types.ModuleType(full)
            m.__dict__.update(attrs)
            if paths:
                m.__path__ = paths
            sys.modules[full] = m


_stub_heavy()
live = _load("gateway/run_inbound_media")


class PPath(str):
    """Pure POSIX-path stand-in (relative_to / join / is_file / mkdir)."""
    def __truediv__(self, other):
        return PPath(str(self).rstrip("/") + "/" + str(other).lstrip("/"))

    def relative_to(self, other):
        s = str(self)
        o = str(other).rstrip("/")
        if s == o:
            return PPath("")
        if s.startswith(o + "/"):
            return PPath(s[len(o) + 1:])
        raise ValueError("not a subpath")

    @property
    def parent(self):
        s = str(self)
        return PPath(s.rsplit("/", 1)[0] if "/" in s else "")

    def mkdir(self, *a, **k):
        pass

    def is_file(self):
        return True

    def resolve(self):
        return self


class MEv:
    def __init__(self, d):
        self.media_urls = list(d.get("media_urls") or [])
        self.text = d.get("text")
        self._gateway_pending_stt_text = d.get("_gateway_pending_stt_text")


def compute(r):
    holder = {"active": r["active"], "launch": r["launch"], "amap": dict(r.get("amap") or {})}
    hc = types.ModuleType("hermes_constants")
    hc.get_hermes_home = lambda: holder["active"]
    hc.get_routing_process_hermes_home = lambda: holder["launch"]
    hc.hermes_home_key = lambda p: str(p)
    tools = types.ModuleType("tools")
    tools.__path__ = []
    cf = types.ModuleType("tools.credential_files")
    cf.to_agent_visible_cache_path = lambda p: holder["amap"].get(p, p)
    keys = ("hermes_constants", "tools", "tools.credential_files")
    old = {k: sys.modules.get(k) for k in keys}
    old_Path, old_shutil = live.Path, live.shutil
    try:
        sys.modules["hermes_constants"] = hc
        sys.modules["tools"] = tools
        sys.modules["tools.credential_files"] = cf
        live.Path = PPath
        live.shutil = types.SimpleNamespace(move=lambda a, b: None)
        ev = MEv(r["event"])
        live.rehome_inbound_media(ev)
    finally:
        live.Path, live.shutil = old_Path, old_shutil
        for k, v in old.items():
            if v is None:
                sys.modules.pop(k, None)
            else:
                sys.modules[k] = v
    return {"media_urls": ev.media_urls, "text": ev.text,
            "_gateway_pending_stt_text": ev._gateway_pending_stt_text}

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    try:
        py = compute(r)
    except Exception as exc:
        print(f"ORACLE w11_runinbound_aaf63b: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH w11_runinbound_aaf63b {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w11_runinbound_aaf63b: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
