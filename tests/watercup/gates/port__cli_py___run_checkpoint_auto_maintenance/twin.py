"""twin for cli.py:_run_checkpoint_auto_maintenance (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cli.py"
    PY_FN = "_run_checkpoint_auto_maintenance"
    C_FN = "cli_run_checkpoint_auto_maintenance"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
