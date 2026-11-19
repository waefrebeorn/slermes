"""twin for hermes_cli/setup_hidden_env.py:is_setup_hidden_env (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/setup_hidden_env.py"
    PY_FN = "is_setup_hidden_env"
    C_FN = "setup_wizard_is_hidden_env"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
