"""twin for hermes_cli/auth.py:_refuse_env_adoption_if_config_corrupt (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/auth.py"
    PY_FN = "_refuse_env_adoption_if_config_corrupt"
    C_FN = "_refuse_env_adoption_if_config_corrupt"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
