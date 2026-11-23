#!/usr/bin/env python3
"""Oracle for literal constants of plugins/platforms/qqbot/constants.py."""
import importlib.util, json, os, sys
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, Q)  # quarry must shadow repo tests/ packages
SRC = os.path.join(Q, 'plugins/platforms/qqbot/constants' + ".py")
def _load_live():
    parts = 'plugins/platforms/qqbot/constants'.split("/")
    for i in range(1, len(parts)):
        pname = ".".join(parts[:i])
        if pname in sys.modules: continue
        pdir = os.path.join(Q, *parts[:i])
        init = os.path.join(pdir, "__init__.py")
        loc = [pdir]
        sp = importlib.util.spec_from_file_location(
            pname, init if os.path.isfile(init) else os.path.join(pdir, "__init__.py"),
            submodule_search_locations=loc)
        pm = importlib.util.module_from_spec(sp)
        sys.modules[pname] = pm
        try: sp.loader.exec_module(pm)
        except Exception: pass
    name = ".".join(parts)
    sp = importlib.util.spec_from_file_location(name, SRC)
    mod = importlib.util.module_from_spec(sp)
    sys.modules[name] = mod
    sp.loader.exec_module(mod)
    return mod
live = _load_live()
def compat(v):
    if isinstance(v, (set, frozenset)): return [compat(x) for x in sorted(v, key=repr)]
    if isinstance(v, (list, tuple)): return [compat(x) for x in v]
    if isinstance(v, dict): return {str(k): compat(x) for k, x in v.items()}
    if isinstance(v, int) and abs(v) > 9007199254740992: return str(v)
    return v
names = ["QQBOT_VERSION", "API_BASE", "TOKEN_URL", "GATEWAY_URL_PATH", "ONBOARD_CREATE_PATH", "ONBOARD_POLL_PATH", "QR_URL_TEMPLATE", "DEFAULT_API_TIMEOUT", "FILE_UPLOAD_TIMEOUT", "CONNECT_TIMEOUT_SECONDS", "RECONNECT_BACKOFF", "MAX_RECONNECT_ATTEMPTS", "RATE_LIMIT_DELAY", "QUICK_DISCONNECT_THRESHOLD", "MAX_QUICK_DISCONNECT_COUNT", "ONBOARD_POLL_INTERVAL", "ONBOARD_API_TIMEOUT", "MAX_MESSAGE_LENGTH", "DEDUP_WINDOW_SECONDS", "DEDUP_MAX_SIZE", "MSG_TYPE_TEXT", "MSG_TYPE_MARKDOWN", "MSG_TYPE_MEDIA", "MSG_TYPE_INPUT_NOTIFY", "MEDIA_TYPE_IMAGE", "MEDIA_TYPE_VIDEO", "MEDIA_TYPE_VOICE", "MEDIA_TYPE_FILE"]
py = {n: compat(getattr(live, n)) for n in names}
cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line: continue
    try: r = json.loads(line)
    except Exception: continue
    if r.get("func") != "constants": continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH cn_5f6deb5549 py={py!r} c={got!r}", file=sys.stderr)
print("ORACLE cn_5f6deb5549: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
