"""twin for hermes_cli/gateway.py:_runtime_dir_is_ours (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/gateway.py"
    PY_FN = "_runtime_dir_is_ours"
    C_FN = "gw_runtime_dir_is_ours"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
