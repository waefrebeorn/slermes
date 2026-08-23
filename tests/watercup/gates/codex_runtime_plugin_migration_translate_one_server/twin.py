"""twin for codex_runtime_plugin_migration_translate_one_server.

Traces the LIVE hermes_cli/codex_runtime_plugin_migration.py::
_translate_one_server semantics with the same rung order as the C harness
(a traced copy of the FIXED ports_wrappers.c implementation).

Input: one Hermes MCP server config as a JSON object string.
"""
import os, sys, json, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_codex_mig",
    os.path.join(DEV, "hermes_cli", "codex_runtime_plugin_migration.py"))
_cm = ilu.module_from_spec(_spec)
sys.modules["wc_live_codex_mig"] = _cm
try:
    _spec.loader.exec_module(_cm)
    KNOWN = set(_cm._KNOWN_HERMES_KEYS)
    DROPPED = set(_cm._KEYS_DROPPED_WITH_WARNING)
except Exception:
    KNOWN = {"command", "args", "env", "cwd", "url", "headers", "transport",
             "timeout", "connect_timeout", "enabled", "description"}
    DROPPED = {"sampling"}


def trace(cfg):
    c = Cup("translate_one_server")
    out = {}
    skipped = []

    def add_skip(msg):
        c.valstr(msg)
        skipped.append(msg)

    if not isinstance(cfg, dict):                       # B1
        c.branch(True)
        ok = False
        return ok, c.finalize()
    c.branch(False)

    has_command = bool(cfg.get("command"))
    has_url = bool(cfg.get("url"))

    if has_command and has_url:                         # B2
        c.branch(True)
        add_skip("url (both command and url set; preferring stdio)")
        has_url = False
    else:
        c.branch(False)

    if has_command:                                     # B3 stdio
        c.branch(True)
        out["command"] = str(cfg["command"])
        args = cfg.get("args") or []
        if args:                                        # args present
            c.branch(True)
            out["args"] = [str(a) for a in args]
            for _ in args:
                c.loop()
        else:
            c.branch(False)
        env = cfg.get("env") or {}
        if env:                                         # env present
            c.branch(True)
            out["env"] = {str(k): str(v) for k, v in env.items()}
            for _ in env:
                c.loop()
        else:
            c.branch(False)
        cwd = cfg.get("cwd")
        if cwd:                                         # cwd present
            c.branch(True)
            out["cwd"] = str(cwd)
        else:
            c.branch(False)
        ok = True
    elif has_url:                                       # B4 http
        c.branch(False)
        c.branch(True)
        out["url"] = str(cfg["url"])
        headers = cfg.get("headers") or {}
        if headers:                                     # headers present
            c.branch(True)
            out["http_headers"] = {str(k): str(v) for k, v in headers.items()}
            for _ in headers:
                c.loop()
        else:
            c.branch(False)
        if cfg.get("transport") == "sse":               # sse hint
            c.branch(True)
            add_skip("transport=sse (codex auto-negotiates)")
        else:
            c.branch(False)
        ok = True
    else:                                               # B5 neither
        c.branch(False)
        c.branch(False)
        add_skip("no command or url field")
        ok = False

    if ok:
        for key, okey, smsg in (
            ("timeout", "tool_timeout_sec", "timeout (not numeric)"),
            ("connect_timeout", "startup_timeout_sec",
             "connect_timeout (not numeric)"),
        ):                                              # LOOP timeouts
            c.loop()
            if key not in cfg:                          # absent -> F,F
                c.branch(False)
                c.branch(False)
                continue
            c.branch(True)
            try:
                out[okey] = float(cfg[key])             # numeric
                c.branch(True)
            except (TypeError, ValueError):
                c.branch(False)
                add_skip(smsg)

        if cfg.get("enabled") is False:                 # explicit false
            c.branch(True)
            out["enabled"] = False
        else:
            c.branch(False)

        for key in cfg:                                 # LOOP keys
            c.loop()
            if key in DROPPED:
                c.branch(True)
                c.branch(False)
                add_skip(f"{key} (no codex equivalent)")
            elif key not in KNOWN:
                c.branch(False)
                c.branch(True)
                add_skip(f"{key} (unknown Hermes key)")
            else:
                c.branch(False)
                c.branch(False)
        c.branch(True)                                  # finalize marker
    else:
        c.branch(False)

    c.valstr("1" if ok else "0")
    return ok, c.finalize()


def run(inputs):
    cups = []
    for raw in inputs:
        try:
            cfg = json.loads(raw)
        except ValueError:
            cfg = None
        _, cup = trace(cfg)
        cups.append(cup)
    return cups
