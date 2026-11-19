"""twin for hermes_cli/config.py:render_personality_prompt (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/config.py"
    PY_FN = "render_personality_prompt"
    C_FN = "render_personality_prompt"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
