"""twin for hermes_cli/setup.py:_prompt_container_resources (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/setup.py"
    PY_FN = "_prompt_container_resources"
    C_FN = "_prompt_container_resources"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
