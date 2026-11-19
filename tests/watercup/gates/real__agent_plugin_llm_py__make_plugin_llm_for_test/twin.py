"""twin for agent/plugin_llm.py:make_plugin_llm_for_test (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/plugin_llm.py"
    PY_FN = "make_plugin_llm_for_test"
    C_FN = "make_plugin_llm_for_test"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
