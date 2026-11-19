"""twin for agent/secret_sources/command.py:_parse_dotenv_map (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/secret_sources/command.py"
    PY_FN = "_parse_dotenv_map"
    C_FN = "_parse_dotenv_map"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
