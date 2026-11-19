"""twin for cli.py:_confirm_and_apply_cli_model_switch (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cli.py"
    PY_FN = "_confirm_and_apply_cli_model_switch"
    C_FN = "_confirm_and_apply_cli_model_switch"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
