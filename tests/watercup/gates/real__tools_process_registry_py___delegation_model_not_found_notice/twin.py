"""twin for tools/process_registry.py:_delegation_model_not_found_notice (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/process_registry.py"
    PY_FN = "_delegation_model_not_found_notice"
    C_FN = "_delegation_model_not_found_notice"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
