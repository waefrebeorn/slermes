#!/usr/bin/env python3
"""Differential oracle for agent/model_metadata_llamacpp.py (w17_modelmetad_923e37).

The live module is replayed against a scripted HTTP client (response specs in
fixture order). ``httpx.HTTPError`` and the two helper functions imported from
``agent.model_metadata`` are injected with their exact live semantics, so no
network or real model_metadata import is needed.
"""
import importlib.util
import json
import os
import sys
import types

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


_pkg("agent")

# Inject httpx.HTTPError so `except httpx.HTTPError` in the live module binds.
httpx = types.ModuleType("httpx")
class HTTPError(Exception):
    pass
httpx.HTTPError = HTTPError
sys.modules["httpx"] = httpx

# Inject the two agent.model_metadata helpers with their VERBATIM live bodies.
mm = types.ModuleType("agent.model_metadata")
mm._positive_int = lambda v: int(v) if isinstance(v, (int, float)) and v else None
mm._model_id_matches = lambda cid, lm: (cid == lm or
    ("/" in cid and cid.rsplit("/", 1)[1] == lm))
sys.modules["agent.model_metadata"] = mm

NAME = "w17_modelmetad_923e37_live"
sp = importlib.util.spec_from_file_location(NAME, os.path.join(Q, "agent", "model_metadata_llamacpp.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)


class Resp:
    def __init__(self, spec):
        self.status_code = spec.get("status", 200)
        self._body = spec.get("body")
        self._jr = bool(spec.get("json_raises", False))

    def json(self):
        if self._jr:
            raise ValueError("bad json")
        return self._body


class Client:
    def __init__(self, specs):
        self.specs = list(specs)
        self.i = 0

    def get(self, url):
        spec = self.specs[self.i]
        self.i += 1
        if spec.get("error", False):
            raise HTTPError("network down")
        return Resp(spec)


def compute(r):
    client = Client(r.get("responses", []))
    model = r.get("model", "")
    if r.get("func") == "llamacpp":
        return live.llamacpp_context(client, "http://server", model)
    if r.get("func") == "router":
        return live.router_launch_ctx(client, "http://server", model)
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    py = compute(r)
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (r.get("func"), r.get("case"), py, got),
              file=sys.stderr)
print("ORACLE w17_modelmetad_923e37: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
