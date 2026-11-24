#!/usr/bin/env python3
"""Differential oracle for plugins/teams_pipeline/graph_auth.py (gate w19_graphauth_31d6da)."""
import ast, asyncio, json, os, sys, types, __future__
from pathlib import Path
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
REPO = Path(__file__).resolve().parent.parent
SRC = REPO.parent / "hermes-agent" / "plugins" / "teams_pipeline" / "graph_auth.py"
_ANN = __future__.annotations.compiler_flag
GATE = "w19_graphauth_31d6da"
WANTED_FN = {"format_graph_error", "_extract_error_detail"}
WANTED_CLS = {"GraphCredentials", "CachedAccessToken", "MicrosoftGraphTokenProvider",
              "MicrosoftGraphAuthError", "MicrosoftGraphConfigError", "MicrosoftGraphTokenError"}


class FakeTime:
    """Injected clock: the C side takes `now` as an argument, the live code reads time.time()."""

    def __init__(self):
        self.now = 0.0

    def time(self):
        return self.now


FT = FakeTime()


def extract():
    tree = ast.parse(SRC.read_text(encoding="utf-8"))
    mod = types.ModuleType("graph_auth_live")
    mod.__dict__["__builtins__"] = __builtins__
    sys.modules["graph_auth_live"] = mod
    g = mod.__dict__
    g.update(dict(dataclass=__import__("dataclasses").dataclass, Any=object, asyncio=asyncio,
                  os=os, time=FT, httpx=types.ModuleType("httpx")))
    for node in tree.body:
        if isinstance(node, ast.Assign):
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                         flags=_ANN, dont_inherit=True), g)
        elif isinstance(node, ast.ClassDef) and node.name in WANTED_CLS:
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                         flags=_ANN, dont_inherit=True), g)
        elif isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in WANTED_FN:
            exec(compile(ast.Module(body=[node], type_ignores=[]), str(SRC), "exec",
                         flags=_ANN, dont_inherit=True), g)
    return g


G = extract()


class FakeResponse:
    def __init__(self, payload, is_json, text):
        self._payload = payload
        self._is_json = is_json
        self.text = text or ""

    def json(self):
        if not self._is_json:
            raise ValueError("not json")
        return self._payload


def creds(r):
    return G["GraphCredentials"](r.get("tenant_id", ""), r.get("client_id", ""), r.get("client_secret", ""),
                                 r.get("scope", G["DEFAULT_GRAPH_SCOPE"]),
                                 r.get("authority_url", G["DEFAULT_GRAPH_AUTHORITY_URL"]))


def run_case(r):
    f = r.get("func")
    if f == "format_error":
        return G["format_graph_error"](r.get("error"))
    if f == "from_env":
        try:
            c = G["GraphCredentials"].from_env(r.get("env_map") or {}, required=bool(r.get("required", True)))
        except G["MicrosoftGraphConfigError"] as e:
            return {"error": str(e)}
        if c is None:
            return None
        return {"tenant_id": c.tenant_id, "client_id": c.client_id, "client_secret": c.client_secret,
                "scope": c.scope, "authority_url": c.authority_url}
    if f == "token_url":
        return creds(r).token_url
    if f == "is_expired":
        FT.now = float(r.get("now", 0.0))
        tok = G["CachedAccessToken"]("tok", float(r.get("expires_at", 0.0)))
        return bool(tok.is_expired(skew_seconds=int(r.get("skew_seconds", 120))))
    if f == "expires_in":
        FT.now = float(r.get("now", 0.0))
        tok = G["CachedAccessToken"]("tok", float(r.get("expires_at", 0.0)))
        return tok.expires_in_seconds
    if f == "fresh_cached":
        FT.now = float(r.get("now", 0.0))
        p = G["MicrosoftGraphTokenProvider"](creds(r), skew_seconds=int(r.get("skew_seconds", 120)))
        if r.get("has_cached"):
            p._cached_token = G["CachedAccessToken"]("tok", float(r.get("expires_at", 0.0)))
        return p._fresh_cached() is not None
    if f == "inspect_health":
        FT.now = float(r.get("now", 0.0))
        p = G["MicrosoftGraphTokenProvider"](creds(r), skew_seconds=int(r.get("skew_seconds", 120)))
        if r.get("has_cached"):
            p._cached_token = G["CachedAccessToken"]("tok", float(r.get("expires_at", 0.0)))
        return p.inspect_token_health()
    if f == "error_detail":
        resp = FakeResponse(r.get("payload"), bool(r.get("is_json", True)), r.get("text", ""))
        return G["_extract_error_detail"](resp)
    return None


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
        f = r.get("func")
        if f not in {"format_error", "from_env", "token_url", "is_expired", "expires_in",
                     "fresh_cached", "inspect_health", "error_detail"}:
            continue
        cases += 1
        try:
            py = run_case(r)
        except BaseException as e:
            mism += 1
            print("MISMATCH %s oracle raised %r" % (f, e), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s args=%r py=%r c=%r" % (f, r, py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if (mism or not cases) else 0)


if __name__ == "__main__":
    main()
