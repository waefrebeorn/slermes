"""twin for tools/delegation_output_schema.py:append_output_contract (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/delegation_output_schema.py"
    PY_FN = "append_output_contract"
    C_FN = "append_output_contract"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
