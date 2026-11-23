#!/usr/bin/env python3
"""Oracle for upstream Hermes hermes_cli/web_server_session_token.py (wsstoken gate)."""
import importlib.util, json, sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
PYROOT = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, PYROOT)
SRC = PYROOT + "/hermes_cli/web_server_session_token.py"
spec = importlib.util.spec_from_file_location("wsstoken_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["wsstoken_live"] = live
spec.loader.exec_module(live)


def make_path(rec):
    class FP:
        def __init__(self, p):
            pass

        def read_text(self, encoding=None):
            if "file_error" in rec:
                raise OSError(rec["file_error"])
            s = rec.get("file_content", "")
            # encoding="utf-8-sig" strips a leading BOM before returning str.
            return s[1:] if s.startswith("\ufeff") else s
    return FP


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if r.get("func") != "resolve":
        continue
    env = r.get("env_value")
    oeget, otok, opath = live.os.environ.get, live.secrets.token_urlsafe, live.Path

    def env_get(k, d=None):
        if k == "HERMES_DASHBOARD_SESSION_TOKEN" and isinstance(env, str):
            return env
        return d

    live.os.environ.get = env_get
    live.secrets.token_urlsafe = lambda n: r.get("minted", "")
    live.Path = make_path(r)
    try:
        try:
            token = live.resolve_session_token()
            py = {"token": token}
        except SystemExit as e:
            py = {"exit": str(e)}
    finally:
        live.os.environ.get = oeget
        live.secrets.token_urlsafe = otok
        live.Path = opath
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH resolve case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE wsstoken: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
