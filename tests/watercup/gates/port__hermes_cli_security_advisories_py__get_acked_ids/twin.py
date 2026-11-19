"""twin for hermes_cli/security_advisories.py:get_acked_ids (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/security_advisories.py"
    PY_FN = "get_acked_ids"
    C_FN = "get_acked_ids"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
