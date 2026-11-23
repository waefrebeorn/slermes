#!/usr/bin/env python3
"""Differential oracle for gate w10_sessionenv_1ffefb (gateway/session_envelope.py)."""
import importlib.util, json, os, sys, types
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _pkg(name):
    m = sys.modules.get(name)
    if m is None:
        m = types.ModuleType(name)
        m.__path__ = [os.path.join(Q, *name.split("."))]
        sys.modules[name] = m
    return m


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


# Heavy module-level imports of the live module; stub them so it loads.
_pkg("gateway")
_pkg("gateway.platforms")
_mod("gateway.platforms.event", MessageEvent=object, MessageType=object)
_mod("gateway.session", SessionSource=object)
_mod("gateway.session_ingress_media",
     capture_native_media=lambda v: v, restore_native_media=lambda v: v)
_pkg("persistence")
_pkg("persistence.core")


class AuthorityRefusal(Exception):
    def __init__(self, reason, field=None, detail=None):
        super().__init__(reason)
        self.reason, self.field, self.detail = reason, field, detail


_mod("persistence.core.errors", AuthorityRefusal=AuthorityRefusal)

NAME = "w10_sessionenv_1ffefb_live"
sp = importlib.util.spec_from_file_location(NAME, os.path.join(Q, "gateway", "session_envelope.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)


def compute(r):
    f = r.get("func")
    if f == "media_clause":
        ev = types.SimpleNamespace(
            media_urls=r.get("media_urls"),
            media_types=r.get("media_types"),
            media_text_inlined=r.get("media_text_inlined"),
            media_filenames=r.get("media_filenames"))
        return live._media_clause(ev)
    if f == "native_metadata":
        ev = types.SimpleNamespace(
            metadata=r.get("metadata"),
            source=types.SimpleNamespace(platform=types.SimpleNamespace(value=r.get("platform"))))
        return live._native_metadata(ev)
    return None


def norm(v):
    if isinstance(v, (list, tuple)):
        return [norm(x) for x in v]
    if isinstance(v, dict):
        return {k: norm(x) for k, x in v.items()}
    return v


n = m = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if not isinstance(r, dict) or "out" not in r:
        continue
    n += 1
    py, got = norm(compute(r)), r.get("out")
    if py != got:
        m += 1
        print("MISMATCH %s py=%r c=%r" % (r.get("func"), py, got), file=sys.stderr)
print("ORACLE w10_sessionenv_1ffefb: %d cases, %d mismatches" % (n, m))
sys.exit(1 if m else 0)
