"""twin for tools/delegation_output_schema.py:extract_json_candidate (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/delegation_output_schema.py"
    PY_FN = "extract_json_candidate"
    C_FN = "extract_json_candidate"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
