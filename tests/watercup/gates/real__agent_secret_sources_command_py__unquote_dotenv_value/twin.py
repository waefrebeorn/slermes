"""twin for agent/secret_sources/command.py:unquote_dotenv_value (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/secret_sources/command.py"
    PY_FN = "unquote_dotenv_value"
    C_FN = "unquote_dotenv_value"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
