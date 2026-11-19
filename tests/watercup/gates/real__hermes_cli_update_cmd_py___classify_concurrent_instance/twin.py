"""twin for hermes_cli/update_cmd.py:_classify_concurrent_instance (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/update_cmd.py"
    PY_FN = "_classify_concurrent_instance"
    C_FN = "_classify_concurrent_instance"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
