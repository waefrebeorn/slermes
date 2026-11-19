"""twin for cli.py:_restore_model_runtime_snapshot (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cli.py"
    PY_FN = "_restore_model_runtime_snapshot"
    C_FN = "hcli_restore_model_runtime_snapshot"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
