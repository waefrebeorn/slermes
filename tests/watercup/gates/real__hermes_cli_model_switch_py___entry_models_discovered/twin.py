"""twin for hermes_cli/model_switch.py:_entry_models_discovered (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/model_switch.py"
    PY_FN = "_entry_models_discovered"
    C_FN = "_entry_models_discovered"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
