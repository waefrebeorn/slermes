"""twin for approval_interpreter_family.

Imports LIVE tools/approval.py::_interpreter_family and traces the SAME
branch-ladder rung order as the C harness (src/tools/approval_wrappers.c::
appr_u_interpreter_family): null/empty -> basename+.exe strip -> python ->
node -> perl -> ruby -> php -> powershell -> result value.
"""
import os, sys, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

spec = ilu.spec_from_file_location("wc_approval", os.path.join(DEV, "tools", "approval.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_approval"] = mod
spec.loader.exec_module(mod)

from watercup_py import Cup

FN = "_interpreter_family"


def run(inputs):
    cups = []
    for raw in inputs:
        c = Cup(FN)
        res = None
        base = ""
        if raw is None or raw == "":
            c.branch(True)
        else:
            c.branch(False)
            # LIVE lowercases the basename; the C port compares the raw
            # basename. Agreed-input set uses lowercase basenames only.
            name = os.path.basename(raw).lower()
            if name.endswith(".exe"):
                name = name[:-4]
            base = name
            c.valstr(base)
            import re
            checks = [
                (r"py(?:\.exe)?|python[23]?(?:\.\d+)*(?:\.exe)?", "python"),
                (r"node(?:js)?(?:\.exe)?", "node"),
                (r"perl[0-9.]*(?:\.exe)?", "perl"),
                (r"ruby[0-9.]*(?:\.exe)?", "ruby"),
                (r"php(?:\.exe)?", "php"),
                (r"powershell(?:\.exe)?|pwsh(?:\.exe)?", "powershell"),
            ]
            for pattern, family in checks:
                hit = re.fullmatch(pattern, base) is not None
                c.branch(hit)
                if hit:
                    res = family
                    break
        # Result value comes from the LIVE function itself.
        live_res = mod._interpreter_family(raw) if (raw is not None and raw != "") else None
        assert (live_res or None) == (res or None), "twin ladder diverged from LIVE"
        c.valstr(live_res if live_res else "")
        cups.append(c.finalize())
    return cups
