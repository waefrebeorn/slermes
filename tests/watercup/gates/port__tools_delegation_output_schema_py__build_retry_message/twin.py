"""twin for tools/delegation_output_schema.py:build_retry_message (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/delegation_output_schema.py"
    PY_FN = "build_retry_message"
    C_FN = "build_retry_message"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
