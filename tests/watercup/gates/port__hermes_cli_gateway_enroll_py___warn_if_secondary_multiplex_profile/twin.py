"""twin for hermes_cli/gateway_enroll.py:_warn_if_secondary_multiplex_profile (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/gateway_enroll.py"
    PY_FN = "_warn_if_secondary_multiplex_profile"
    C_FN = "_warn_if_secondary_multiplex_profile"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
