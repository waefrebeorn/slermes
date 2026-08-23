"""twin for skills_tool_required_env_vars.

Imports LIVE tools/skills_tool.py::_get_required_environment_variables and
traces the SAME extraction ladder as the simplified C port
(skills_tool_required_env_vars). Inputs are plain-name entries without dupes so
the C extractor's agreed subset is exercised; LIVE's regex validation, prompt/
help normalization and dedupe are documented divergences (see gate.json).
"""
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
import importlib.util as ilu
spec = ilu.spec_from_file_location("wc_skills_tool", os.path.join(DEV, "tools", "skills_tool.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_skills_tool"] = mod
spec.loader.exec_module(mod)

from watercup_py import Cup

def run(inputs):
    cups = []
    for raw in inputs:
        c = Cup("required_env_vars")
        names = []
        # rung 1: required_environment_variables present?
        p = raw.find('"required_environment_variables"')
        if p < 0:
            c.branch(True)
        else:
            c.branch(False)
            # rung 2/3: '[' then ']'
            lb = raw.find("[", p)
            if lb < 0:
                c.branch(True)
            else:
                c.branch(False)
                rb = raw.find("]", lb)
                if rb < 0:
                    c.branch(True)
                else:
                    c.branch(False)
                    block = raw[lb + 1:rb]
                    c.loop()
                    for tok in [x for x in block.split(",") if x.strip(" \t")]:  # strtok skips empty fields
                        s = tok.strip(" \t")
                        # rung: dict-ish ("name" substring) or plain string
                        has_name = ("name" in s)
                        if has_name:
                            c.branch(True)
                            colon = s.find(":", s.find("name"))
                            body = s[colon + 1:].strip(" \t") if colon >= 0 else ""
                            if body.startswith('"'):
                                c.branch(True)
                                endq = body.find('"', 1)
                                nm = body[1:endq] if endq > 0 else ""
                            else:
                                c.branch(False)
                                nm = body.split("}")[0].strip()
                        else:
                            c.branch(False)
                            if s.startswith('"'):
                                c.branch(True)
                                endq = s.find('"', 1)
                                nm = s[1:endq] if endq > 0 else ""
                            else:
                                c.branch(False)
                                nm = s.split(" ")[0].split(",")[0]
                        if nm:
                            c.branch(True)
                            names.append(nm)
                        else:
                            c.branch(False)
        import json as _json
        fm = _json.loads(raw)
        live_names = [e["name"] for e in mod._get_required_environment_variables(fm)]
        assert names == live_names, f"LIVE/ladder diverge on {raw!r}: {live_names} vs {names}"
        c.valstr(",".join(names))
        cups.append(c.finalize())
    return cups
