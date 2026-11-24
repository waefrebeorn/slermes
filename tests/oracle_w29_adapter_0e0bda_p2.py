#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/a2a/adapter.py, gate p2.

``A2AAdapter`` is built with ``__new__`` and only the attributes these methods
read are set: the served-agent registry (what ``_load_served_agents`` would have
produced, supplied by the fixture), the bind host/port, the configured agent
name and a stand-in security context exposing ``localhost_only()``.  That is the
seam -- no server, no store, no profile scope.

Compared values: ``_base_url``, ``_route_for_path``, ``_route_for_request``
(including the tenant-mismatch message), ``_served_agent_summary``,
``_scope_for_agent`` and ``_build_card``.  The Agent Card's skills take the
static ``skills_from_toolsets`` fallback: with no ``tools.registry`` import in
this process the live adapter falls back the same way.
"""
import json
import os
import sys
from pathlib import Path
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w29_adapter_0e0bda_p2"

# A login shell can start without a Windows home; the live package resolves
# hermes_home at import time, so anchor it on the quarry tree's parent.
if not os.environ.get("USERPROFILE"):
    os.environ["USERPROFILE"] = str(QUARRY.parent)
if not os.environ.get("HOME"):
    os.environ["HOME"] = str(QUARRY.parent)

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from plugins.platforms.a2a import adapter as live  # noqa: E402

WANTED = {"base_url", "route_for_path", "route_for_request",
          "served_agent_summary", "scope_for_agent", "build_card"}

ENV_ORG, ENV_URL = "A2A_PROVIDER_ORG", "A2A_PROVIDER_URL"


def adapter_for(r):
    agents = [a for a in (r.get("agents") or []) if isinstance(a, dict)]
    a = live.A2AAdapter.__new__(live.A2AAdapter)
    a.host = r.get("host") if r.get("host") is not None else "127.0.0.1"
    a.port = r.get("port") if r.get("port") is not None else live._DEFAULT_PORT
    a.agent_name = r.get("agent_name") if r.get("agent_name") is not None else "hermes-test"
    a._agents = {(ag.get("slug") or ""): dict(ag) for ag in agents}
    a._advertised_toolsets = []
    localhost_only = bool(r.get("localhost_only"))
    a._security_context = SimpleNamespace(localhost_only=lambda: localhost_only)
    return a


def base_url(r):
    a = adapter_for(r)
    return a._base_url(r.get("public_url"))


def route_for_path(r):
    a = adapter_for(r)
    route = a._route_for_path(r.get("path"))
    return {"slug": route["agent"].get("slug", ""), "subpath": route["subpath"]}


def route_for_request(r):
    a = adapter_for(r)
    route = a._route_for_request(r.get("path"), r.get("params") or {})
    if "error" in route:
        return {"error": route["error"]}
    return {"slug": route["agent"].get("slug", ""), "subpath": route["subpath"]}


def served_agent_summary(r):
    a = adapter_for(r)
    return a._served_agent_summary(r.get("public_url"))


def scope_for_agent(r):
    a = adapter_for(r)
    slug = r.get("agent_slug")
    agent = a._agents.get(slug) if slug is not None else None
    return list(a._scope_for_agent(agent))


def build_card(r):
    a = adapter_for(r)
    for key, env in ((r.get("provider_org"), ENV_ORG), (r.get("provider_url"), ENV_URL)):
        if key is None:
            os.environ.pop(env, None)
        else:
            os.environ[env] = str(key)
    try:
        slug = r.get("agent_slug")
        agent = a._agents.get(slug) if slug is not None else None
        return a._build_card(r.get("public_url"), agent=agent)
    finally:
        os.environ.pop(ENV_ORG, None)
        os.environ.pop(ENV_URL, None)


HANDLERS = {
    "base_url": base_url,
    "route_for_path": route_for_path,
    "route_for_request": route_for_request,
    "served_agent_summary": served_agent_summary,
    "scope_for_agent": scope_for_agent,
    "build_card": build_card,
}


def main():
    cases = mism = 0
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
            py = HANDLERS[r["func"]](r)
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
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
