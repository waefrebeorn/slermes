"""twin for hermes_cli/model_normalize.py:_repair_prefix_from_catalogue (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/model_normalize.py"
    PY_FN = "_repair_prefix_from_catalogue"
    C_FN = "_repair_prefix_from_catalogue"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
