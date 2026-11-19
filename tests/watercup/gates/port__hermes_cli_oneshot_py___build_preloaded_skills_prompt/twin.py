"""twin for hermes_cli/oneshot.py:_build_preloaded_skills_prompt (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/oneshot.py"
    PY_FN = "_build_preloaded_skills_prompt"
    C_FN = "_build_preloaded_skills_prompt"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
