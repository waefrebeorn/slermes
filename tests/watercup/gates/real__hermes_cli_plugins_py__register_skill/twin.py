"""twin for hermes_cli/plugins.py:register_skill (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/plugins.py"
    PY_FN = "register_skill"
    C_FN = "plug_register_skill"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
