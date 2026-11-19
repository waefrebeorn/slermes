"""twin for agent/prompt_builder.py:build_context_files_prompt (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/prompt_builder.py"
    PY_FN = "build_context_files_prompt"
    C_FN = "build_context_files_prompt"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
