"""twin for hermes_cli/local_runtime/catalog.py:find_entry_for_model (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/local_runtime/catalog.py"
    PY_FN = "find_entry_for_model"
    C_FN = "find_entry_for_model"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
