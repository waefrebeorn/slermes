"""twin for agent_verification_evidence_equivalent_needles.

Traces the LIVE agent/verification_evidence.py::_equivalent_needles ladder
with the same rung order as the C harness:
  push needle / B1 (len>=3 and needle[1]=='run') + LOOP pm match B2 /
  B3 single token containing '/' -> bash+sh forms /
  B4 needle==['pytest'] -> LOOP 5 runner forms.
Value event hashes the serialized candidate list; the candidates come from
the LIVE function itself.
"""
import os, sys, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_verification_evidence",
    os.path.join(DEV, "agent", "verification_evidence.py"))
_ve = ilu.module_from_spec(_spec)
sys.modules["wc_live_verification_evidence"] = _ve
_spec.loader.exec_module(_ve)

_live_fn = _ve._equivalent_needles
PMS = ("npm", "pnpm", "yarn", "bun")
FORMS = [["python", "-m", "pytest"], ["python3", "-m", "pytest"],
         ["uv", "run", "pytest"], ["poetry", "run", "pytest"],
         ["pipenv", "run", "pytest"]]


def trace(needle):
    c = Cup("equivalent_needles")
    live = _live_fn(list(needle))
    ser = "|".join(" ".join(cand) for cand in live)
    m = len(needle)
    if m >= 3 and needle[1] == "run":                 # B1
        c.branch(True)
        for pm in PMS:                                # LOOP
            c.loop()
            if needle[0] == pm:                       # B2
                c.branch(True)
                break
            c.branch(False)
    else:
        c.branch(False)
    if m == 1 and "/" in needle[0]:                   # B3
        c.branch(True)
    else:
        c.branch(False)
    if needle == ["pytest"]:                          # B4
        c.branch(True)
        for _ in FORMS:                               # LOOP
            c.loop()
    else:
        c.branch(False)
    c.valstr(ser)
    return c.finalize()


def run(inputs):
    cups = []
    for item in inputs:
        cups.append(trace(item.split()))
    return cups
