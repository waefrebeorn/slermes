"""twin for dashboard_procs_valid_lockfile_payload.

Imports LIVE hermes_cli/dashboard_procs.py::_valid_lockfile_payload and traces
the SAME rung ladder as the C harness. Record fields are rebuilt into the dict
LIVE expects ('-' key absent, '~' empty string, numeric fields coerced like C's
atol); the final boolean is LIVE's own verdict.
"""
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
import importlib.util as ilu
spec = ilu.spec_from_file_location("wc_dashprocs", os.path.join(DEV, "hermes_cli", "dashboard_procs.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_dashprocs"] = mod
spec.loader.exec_module(mod)

from watercup_py import Cup

HEX = set("0123456789abcdefABCDEF")
KEYS = ["ownershipId", "schemaVersion", "protocolVersion", "spawnNonce", "tokenFingerprint",
        "pid", "port", "profile", "hermesPath", "hermesHome", "logPath", "startedAt"]
NUM_KEYS = {"schemaVersion", "protocolVersion", "pid", "port"}


def build(rec):
    parts = rec.split("|")
    own_param = parts[0]
    d = {}
    for k, v in zip(KEYS, parts[1:13]):
        if v != "-":
            if k in NUM_KEYS and v.isdigit():
                d[k] = int(v)          # mirror C atol coercion of JSON numbers
            else:
                d[k] = "" if v == "~" else v
    return own_param, d


def run(inputs):
    cups = []
    for raw in inputs:
        own_param, d = build(raw)
        c = Cup("valid_lockfile_payload")
        res = False

        def fail():
            c.branch(True)

        own = d.get("ownershipId")
        if own is None or own != own_param:
            c.branch(True)
        else:
            c.branch(False)
            sv = d.get("schemaVersion")
            if not isinstance(sv, int) or sv != 2:
                c.branch(True)
            else:
                c.branch(False)
                pv = d.get("protocolVersion")
                if not isinstance(pv, int) or pv != 1:
                    c.branch(True)
                else:
                    c.branch(False)
                    nonce = d.get("spawnNonce")
                    tf = d.get("tokenFingerprint")
                    ok = True
                    if not isinstance(nonce, str) or len(nonce) != 16:
                        c.branch(True); ok = False
                    else:
                        c.branch(False)
                        if set(nonce) - HEX:
                            c.branch(True); ok = False
                        else:
                            c.branch(False)
                            if not isinstance(tf, str) or len(tf) != 32:
                                c.branch(True); ok = False
                            else:
                                c.branch(False)
                                if set(tf) - HEX:
                                    c.branch(True); ok = False
                                else:
                                    c.branch(False)
                    if ok:
                        # pid / port / bounded-string / logPath-suffix rungs
                        pid = d.get("pid")
                        port = d.get("port")
                        if not isinstance(pid, int) or pid <= 0 or pid > 4194304:
                            c.branch(True)
                        else:
                            c.branch(False)
                            port = d.get("port")
                            if not isinstance(port, int) or port < 0 or port > 65535:
                                c.branch(True)
                            else:
                                c.branch(False)
                                strs = [d.get(k) for k in ("profile", "hermesPath", "hermesHome", "logPath", "startedAt")]
                                bad = any(not isinstance(s, str) or len(s) > 1024 for s in strs)
                                if bad:
                                    c.branch(True)
                                else:
                                    c.branch(False)
                                    c.loop()
                                    nonce_s = d.get("spawnNonce") or ""
                                    res = bool(strs[3]) and strs[3].endswith(f"/{own_param}/{nonce_s}.log")

        live = bool(mod._valid_lockfile_payload(d if d else {}, own_param))
        assert res == live, f"LIVE/ladder diverge on {raw!r}: {live} vs {res}"
        c.valstr("1" if res else "0")
        cups.append(c.finalize())
    return cups
