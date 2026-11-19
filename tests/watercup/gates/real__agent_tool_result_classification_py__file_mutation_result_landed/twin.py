"""twin for agent/tool_result_classification.py:file_mutation_result_landed (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/tool_result_classification.py"
    PY_FN = "file_mutation_result_landed"
    C_FN = "trc_file_mutation_result_landed"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
