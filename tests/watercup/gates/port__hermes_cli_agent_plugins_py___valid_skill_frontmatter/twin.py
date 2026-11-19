"""twin for hermes_cli/agent_plugins.py:_valid_skill_frontmatter (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/agent_plugins.py"
    PY_FN = "_valid_skill_frontmatter"
    C_FN = "ap_valid_skill_frontmatter"

    def norm(self, c, result):
        import inspect as _ic
        if _ic.iscoroutine(result):
            import asyncio as _aio
            result = _aio.run(result)
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
