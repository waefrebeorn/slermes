"""twin for hermes_cli/local_runtime/presets.py:_args_to_keys (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/local_runtime/presets.py"
    PY_FN = "_args_to_keys"
    C_FN = "_args_to_keys"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
