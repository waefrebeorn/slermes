"""twin for hermes_cli/process_identity.py:_own_create_time (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/process_identity.py"
    PY_FN = "_own_create_time"
    C_FN = "process_identity_own_create_time"

    def norm(self, c, result):
        c.valstr("%.17g" % float(result))


_t = _Twin()
trace = _t.trace
run = _t.run
