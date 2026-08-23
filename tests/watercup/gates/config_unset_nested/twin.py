"""twin for config_unset_nested.

Imports LIVE hermes_cli/config.py::_unset_nested and traces the SAME
segment/walk/delete/prune ladder as the linked real C port
(config_py_unset_nested). Input encoding: configJSON\x1fdotted_key. The removal
verdict comes from LIVE itself.
"""
import os, sys, json
HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, os.path.join(DEV, "hermes_cli"))
import importlib.util as ilu
spec = ilu.spec_from_file_location("wc_configpy", os.path.join(DEV, "hermes_cli", "config.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_configpy"] = mod
spec.loader.exec_module(mod)

from watercup_py import Cup


def is_int(s):
    try:
        int(s)
        return True
    except (TypeError, ValueError):
        return False


def run(inputs):
    cups = []
    for raw in inputs:
        cfg_s, _, key = raw.partition("\x1f")
        cfg = json.loads(cfg_s)
        c = Cup("unset_nested")
        removed = False
        if cfg is None or not key:
            c.branch(True)
        else:
            c.branch(False)
            parts = key.split(".")
            bad = False
            for part in parts:
                c.loop()
                if part == "" or len(parts) >= 64:
                    c.branch(True); bad = True; break
                c.branch(False)
            if bad:
                c.branch(True)
            else:
                c.branch(False)
                ok = True
                cur = cfg
                parents = []
                for part in parts[:-1]:
                    if isinstance(cur, list):
                        c.branch(True)
                        if not is_int(part) or not (0 <= int(part) < len(cur)):
                            c.branch(True); ok = False; break
                        c.branch(False)
                        cur = cur[int(part)]
                    elif isinstance(cur, dict):
                        c.branch(False)
                        c.branch(True)
                        if part not in cur:
                            c.branch(True); ok = False; break
                        c.branch(False)
                        cur = cur[part]
                    else:
                        c.branch(False)
                        c.branch(False)
                        c.branch(True)
                        ok = False
                        break
                    parents.append(part)
                if not ok:
                    c.branch(True)
                else:
                    c.branch(False)
                    last = parts[-1]
                    if isinstance(cur, list):
                        c.branch(True)
                        if is_int(last) and 0 <= int(last) < len(cur):
                            c.branch(True); removed = True
                            cur.pop(int(last))
                        else:
                            c.branch(False)
                    elif isinstance(cur, dict):
                        c.branch(False)
                        c.branch(True)
                        if last in cur:
                            c.branch(True); removed = True
                            del cur[last]
                        else:
                            c.branch(False)
                    else:
                        c.branch(False)
                        c.branch(False)
        live = bool(mod._unset_nested(json.loads(cfg_s), key))
        assert removed == live, f"LIVE/ladder diverge on {raw!r}: {live} vs {removed}"
        c.valstr("1" if removed else "0")
        cups.append(c.finalize())
    return cups
