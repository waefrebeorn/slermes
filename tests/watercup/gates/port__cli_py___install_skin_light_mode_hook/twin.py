"""twin for cli.py:_install_skin_light_mode_hook (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cli.py"
    PY_FN = "_install_skin_light_mode_hook"
    C_FN = "_install_skin_light_mode_hook"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
