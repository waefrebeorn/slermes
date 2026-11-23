#!/usr/bin/env python3
"""Differential oracle for gateway/media_fetch.py (gate w6_mediafetch_d0155c).

remote_path_is_denied is the live pure function. The three denylist constants
are injected from gateway.platforms.base through a stub; the C port mirrors
the exact same tuples. No environment / filesystem / network is touched.
"""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _stub(name, **attrs):
    parts = name.split(".")
    for i in range(1, len(parts)):
        pn = ".".join(parts[:i])
        if pn not in sys.modules:
            pm = types.ModuleType(pn)
            pm.__path__ = [Q + "/" + "/".join(parts[:i])]
            sys.modules[pn] = pm
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


DENIED_PREFIXES = (
    "/etc", "/proc", "/sys", "/dev", "/root", "/boot", "/var/log", "/var/lib", "/var/run")
DENIED_HOME_SUBPATHS = (
    ".ssh", ".aws", ".gnupg", ".kube", ".docker", ".config", ".azure", ".gcloud",
    "Library/Keychains")
ROOT_CREDENTIAL_PATHS = (
    "auth.json", "auth.lock", "credentials", "config.yaml", ".anthropic_oauth.json",
    "google_token.json", "google_client_secret.json", "google_oauth_pending.json",
    "webhook_subscriptions.json", "sessions", "browser-profile", "state.db",
    "state.db-wal", "state.db-shm", "state.db-journal", "kanban.db", "kanban.db-wal",
    "kanban.db-shm", "kanban.db-journal", "webapp", "auth/google_oauth.json")

_stub("agent")
_stub("agent.file_safety", is_project_env_basename=lambda b: False)
_stub("gateway.platforms.base",
      _MEDIA_DELIVERY_DENIED_PREFIXES=DENIED_PREFIXES,
      _MEDIA_DELIVERY_DENIED_HOME_SUBPATHS=DENIED_HOME_SUBPATHS,
      _ROOT_CREDENTIAL_PATHS=ROOT_CREDENTIAL_PATHS)

LIVE_NAME = "w6_mediafetch_d0155c_live"
spec = importlib.util.spec_from_file_location(LIVE_NAME, Q + "/gateway/media_fetch.py")
live = importlib.util.module_from_spec(spec)
sys.modules[LIVE_NAME] = live
spec.loader.exec_module(live)

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") != "denied":
        continue

    py = bool(live.remote_path_is_denied(
        r.get("path", ""), r.get("remote_home") or None))

    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH denied args=%r py=%r c=%r" % (r, py, got), file=sys.stderr)

print("ORACLE w6_mediafetch_d0155c: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
