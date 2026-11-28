"""twin: the instructions block from LIVE resolve_runtime_mode.

Only cwd/platform are supplied; the block is built by the live function, not re-implemented.
"""
import json as _j
import agent.coding_context as _cc
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/coding_context.py"
    PY_FN = "resolve_runtime_mode"
    C_FN = "coding_context_join_instructions"
    def call(self, inp, inputs, idx=0):
        raw = _j.loads(inp)
        cfg = {} if raw is None else {"agent": {"coding_instructions": raw}}
        return _cc.resolve_runtime_mode(platform="cli", cwd=".", config=cfg).instructions
    def norm(self, c, result):
        c.valstr(result.encode("utf-8").hex())
_t=_Twin(); trace=_t.trace; run=_t.run
