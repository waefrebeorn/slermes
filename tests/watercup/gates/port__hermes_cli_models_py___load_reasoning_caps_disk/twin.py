"""twin for hermes_cli/models.py:_load_reasoning_caps_disk (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/models.py"
    PY_FN = "_load_reasoning_caps_disk"
    C_FN = "_load_reasoning_caps_disk"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
