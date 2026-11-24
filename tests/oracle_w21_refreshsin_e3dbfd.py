#!/usr/bin/env python3
"""Differential oracle for hermes_cli/dashboard_auth/refresh_singleflight.py
(gate w21_refreshsin_e3dbfd).

The live module is pure stdlib, so the oracle executes its real source: the
helpers `_digest`, `forget`, `_prune`, `_refresh_provider` are lifted out of the
AST (sync and async defs, plus the module-level Assign constants) and run
against a synthetic cache/flight table. Only the live-object seams are faked:
the provider (network), the Session dataclass and the monotonic clock (both
`time.monotonic` call sites read one fixed value, so a single call never spans
a real tick). `refresh_session_coalesced` stays upstream -- it needs a live
provider stack.
"""
import ast
import hashlib
import json
import sys
import threading
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "hermes_cli" / "dashboard_auth" / "refresh_singleflight.py"
GATE = "w21_refreshsin_e3dbfd"


class RefreshExpiredError(Exception):
    pass


class _Session:
    def __init__(self, refresh_token):
        self.refresh_token = refresh_token


class _Flight:
    def __init__(self, generation=0):
        self.lock = threading.Lock()
        self.users = 0
        self.generation = generation


class _Provider:
    def __init__(self, reuse=False):
        self.refresh_reuse_detection = reuse


class _Clock:
    """time.monotonic() seam: one fixed tick per case."""

    now = 0.0

    @classmethod
    def monotonic(cls):
        return cls.now


G = {
    "__name__": "refresh_singleflight_live",
    "hashlib": hashlib,
    "threading": threading,
    "time": _Clock,
    "RefreshExpiredError": RefreshExpiredError,
    "_Flight": _Flight,
    "_guard": threading.Lock(),
    "_SUCCESS_TTL": None,
    "_FAILURE_TTL": None,
    "_MAX_ENTRIES": None,
}

_WANTED_FUNCS = {"_digest", "forget", "_prune", "_refresh_provider"}


def _exec(path, names, assigns, into):
    tree = ast.parse(path.read_text(encoding="utf-8"))
    for node in ast.walk(tree):
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in names:
            node.decorator_list = []
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(path), "exec"),
                 into)
        elif isinstance(node, (ast.Assign, ast.AnnAssign)):
            targets = node.targets if isinstance(node, ast.Assign) else [node.target]
            if any(isinstance(t, ast.Name) and t.id in assigns for t in targets):
                exec(compile(ast.Module(body=[node], type_ignores=[]), str(path), "exec"),
                     into)


def load():
    _exec(SRC, _WANTED_FUNCS, {"_SUCCESS_TTL", "_FAILURE_TTL", "_MAX_ENTRIES"}, G)
    missing = [n for n in sorted(_WANTED_FUNCS | {"_SUCCESS_TTL"}) if n not in G]
    if missing:
        raise SystemExit("oracle: live source drifted, missing %s" % ", ".join(missing))


def sess(v):
    if v is None:
        return None
    return _Session(v.get("refresh_token") or "")


def canon(v):
    if v is None:
        return None
    return {"refresh_token": v.refresh_token}


def do_forget(r):
    providers = {}
    labels = {}
    idmap = {}

    def prov(pid):
        if pid not in providers:
            providers[pid] = _Provider()
            idmap[id(providers[pid])] = pid
        return providers[pid]

    def note(token):
        labels[G["_digest"](token)] = token

    cache = {}
    flights = {}
    entries = r.get("entries")
    if not isinstance(entries, list):
        entries = []
    flights_in = r.get("flights")
    if not isinstance(flights_in, list):
        flights_in = []
    for e in entries:
        token = e.get("token") or ""
        note(token)
        p = prov(e.get("provider"))
        issued = e.get("issued")
        fc = _Session(issued) if issued is not None else None
        if fc is not None and fc.refresh_token:
            note(fc.refresh_token)
        cache[(id(p), G["_digest"](token))] = (0.0, p, fc)
    for f in flights_in:
        token = f.get("token") or ""
        note(token)
        p = prov(f.get("provider"))
        flights[(id(p), G["_digest"](token))] = _Flight(int(f.get("generation") or 0))

    G["_cache"] = cache
    G["_flights"] = flights
    before = list(cache.keys())
    G["forget"](r.get("token") or "")

    def label(key):
        return "%s:%s" % (idmap.get(key[0], key[0]), labels.get(key[1], "?"))

    removed = sorted(label(k) for k in before if k not in cache)
    remaining = [label(k) for k in cache]
    generations = sorted("%s:%s:%d" % (idmap.get(k[0], k[0]), labels.get(k[1], "?"),
                                       f.generation)
                         for k, f in flights.items())
    return {"removed": removed, "remaining": remaining, "generations": generations}


def do_prune(r):
    now = float(r.get("now") or 0.0)
    cache = {}
    for e in r.get("entries") or []:
        cache[e.get("token") or ""] = (float(e.get("expires") or 0.0), None, None)
    G["_cache"] = cache
    G["_prune"](now)
    return {"remaining": list(G["_cache"].keys()),
            "dropped": len(r.get("entries") or []) - len(G["_cache"])}


def do_refresh(r):
    now = float(r.get("now") or 0.0)
    _Clock.now = now
    token = r.get("token") or ""
    p = _Provider(bool(r.get("reuse_detection")))
    key = (id(p), G["_digest"](token))
    cache = {}
    cached = r.get("cached")
    if isinstance(cached, dict):
        cache[key] = (now + float(cached.get("expires_in") or 0.0), p,
                      sess(cached.get("session")))
    G["_cache"] = cache
    G["_flights"] = {}
    calls = {"n": 0}
    payload = r.get("session")

    def refresh_session(refresh_token):
        calls["n"] += 1
        if r.get("expired"):
            raise RefreshExpiredError("refresh token expired")
        return sess(payload)

    p.refresh_session = refresh_session
    ret = G["_refresh_provider"](p, token)
    entry = G["_cache"].get(key)
    return {
        "session": canon(ret),
        "hit": calls["n"] == 0,
        "cached": entry is not None,
        "ttl": (entry[0] - now) if entry is not None else None,
    }


def expected(r):
    f = r.get("func")
    if f == "digest":
        return G["_digest"](r.get("token") or "").hex()
    if f == "constants":
        return {"success_ttl": G["_SUCCESS_TTL"], "failure_ttl": G["_FAILURE_TTL"],
                "max_entries": G["_MAX_ENTRIES"]}
    if f == "forget":
        return do_forget(r)
    if f == "prune":
        return do_prune(r)
    if f == "refresh":
        return do_refresh(r)
    raise AssertionError("unknown func %r" % (f,))


def main():
    load()
    cases = mism = 0
    wanted = {"digest", "constants", "forget", "prune", "refresh"}
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in wanted:
            continue
        cases += 1
        try:
            py = expected(r)
        except Exception as exc:  # noqa: BLE001 - reported as the harness sees it
            mism += 1
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r\n  py=%r\n   c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
