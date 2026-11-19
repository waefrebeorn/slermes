"""twin for hermes_cli/security_advisories.py:filter_unacked (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/security_advisories.py"
    PY_FN = "filter_unacked"
    C_FN = "filter_unacked"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
