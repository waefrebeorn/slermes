#!/usr/bin/env python3
"""Oracle for agent/anthropic_credentials.py pure classification core.

Harness families (anthropic_credentials), emitted per line:
  oauth_token {family, token, ret:int}  -> _is_oauth_token(token)
  host        {family, url, host, native:int}
              -> base_url_hostname(url) [live utils._hostname_of] + is_native_anthropic_host
  route       {family, url, cred, provider, ret:int} -> anthropic_route_is_oauth(...)
  token_valid {family, case, json, now, ret:int}
              -> the live validity predicate but with the EMITTED now (live uses
                 time.time() which we can't replay).
  creds_path  {family, case, out}        -> the C's own string logic replicated
                 (env override expanduser + home join) -- it is the port's contract.

Ast-extracts the live classifiers and utils._hostname_of / base_url_hostname.
No module import is done.
"""
import ast
import json
import re
import sys
from urllib.parse import urlparse

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/agent/anthropic_credentials.py"
UTILS = "C:/Users/eman5/hermes-agent/utils.py"
FUNS = ("_is_oauth_token", "is_native_anthropic_host", "anthropic_route_is_oauth")
UFUNS = ("_parse_base_url", "_hostname_of", "base_url_hostname")


def main() -> int:
    src = open(SRC, encoding="utf-8").read()
    ns = {"Any": object}
    for n in ast.parse(src).body:
        if isinstance(n, ast.FunctionDef) and n.name in FUNS:
            exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
    for n in ast.parse(open(UTILS, encoding="utf-8").read()).body:
        if isinstance(n, ast.FunctionDef) and n.name in UFUNS:
            exec(compile(ast.Module(body=[n], type_ignores=[]), UTILS, "exec"), ns)
    ns["urlparse"] = urlparse
    is_oauth = ns["_is_oauth_token"]
    is_native = ns["is_native_anthropic_host"]
    route_is_oauth = ns["anthropic_route_is_oauth"]
    hostname = ns.get("base_url_hostname")
    _hostname_of = ns.get("_hostname_of")

    env = json.loads

    def token_valid(creds_json, now):
        data = json.loads(creds_json)
        expires_at = data.get("expiresAt", 0)
        if expires_at:
            return now < (expires_at - 60000)
        return bool(data.get("accessToken"))

    def creds_path(d, home):
        home = home or ""
        buf = (d or "").strip()
        if buf:
            if buf[0] == "~":
                if len(buf) == 1 or buf[1] in "/\\":
                    root = home + buf[1:]
                else:
                    root = buf
            else:
                root = buf
        else:
            root = home + "/.claude"
        return root + "/.credentials.json"

    cases = []
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            cases.append(json.loads(line))
        except Exception:
            pass
    if not cases:
        print("ORACLE anthropic_credentials: 0 cases (link failure in disguise)")
        return 1
    mism = 0
    total = 0
    for c in cases:
        total += 1
        fam = c.get("family")
        if fam == "oauth_token":
            want = is_oauth(c.get("token"))
            if bool(c.get("ret")) != want:
                mism += 1
                print(f"MISMATCH oauth_token {c.get('token')!r} C={c.get('ret')} PY={want}")
        elif fam == "host":
            u = c.get("url")
            wh = hostname(u) if hostname else ""
            wn = is_native(u)
            if (c.get("host") or "") != wh:
                mism += 1
                print(f"MISMATCH host url={u!r} C={c.get('host')!r} PY={wh!r}")
            if bool(c.get("native")) != bool(wn):
                mism += 1
                print(f"MISMATCH native url={u!r} C={c.get('native')} PY={wn}")
        elif fam == "route":
            want = route_is_oauth(c.get("url"), c.get("cred"),
                                  provider=c.get("provider"))
            if bool(c.get("ret")) != want:
                mism += 1
                print(f"MISMATCH route url={c.get('url')!r} cred={c.get('cred')!r} "
                      f"prov={c.get('provider')!r} C={c.get('ret')} PY={want}")
        elif fam == "token_valid":
            want = token_valid(c.get("json"), c.get("now"))
            if bool(c.get("ret")) != want:
                mism += 1
                print(f"MISMATCH token_valid case={c.get('case')} "
                      f"now={c.get('now')} C={c.get('ret')} PY={want}")
        elif fam == "creds_path":
            want = creds_path(c.get("dir") if "dir" in c else None, c.get("home"))
            if (c.get("out") or "") != want:
                mism += 1
                print(f"MISMATCH creds_path case={c.get('case')} C={c.get('out')!r} PY={want!r}")
    print(f"ORACLE anthropic_credentials: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
