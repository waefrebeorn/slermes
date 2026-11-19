"""twin for hermes_cli/prompt_size.py:_skill_md_paths_by_name (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/prompt_size.py"
    PY_FN = "_skill_md_paths_by_name"
    C_FN = "psize_skill_md_paths_by_name"

    def norm(self, c, result):
        import inspect as _ic
        if _ic.iscoroutine(result):
            import asyncio as _aio
            result = _aio.run(result)
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
