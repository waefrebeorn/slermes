"""twin for cli.py:_snapshot_model_runtime (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cli.py"
    PY_FN = "_snapshot_model_runtime"
    C_FN = "hcli_snapshot_model_runtime"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
