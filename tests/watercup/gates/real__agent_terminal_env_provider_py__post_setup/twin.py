"""twin for agent/terminal_env_provider.py:post_setup (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/terminal_env_provider.py"
    PY_FN = "post_setup"
    C_FN = "tep_post_setup"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
