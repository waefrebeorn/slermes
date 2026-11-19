"""twin for hermes_cli/config.py:get_env_value_prefer_dotenv (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/config.py"
    PY_FN = "get_env_value_prefer_dotenv"
    C_FN = "get_env_value_prefer_dotenv"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
