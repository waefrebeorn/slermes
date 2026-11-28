"""twin: agent/checkpoints/manager.py:_git_env, LIVE.

build_subprocess_env and selected_git_env are stubbed to passthrough so the dict the
real function builds is just its own assignments; the config-isolation vars python
gets from build_subprocess_env are therefore not compared here (the C adds them
unconditionally and that is asserted separately).
"""
import agent.checkpoints.manager as _m
import tools.environments.local as _local
from watercup_twin_runtime import TwinBase
_local.build_subprocess_env = lambda *a, **k: {}
_m.build_subprocess_env = lambda *a, **k: {}
_m.selected_git_env = lambda base=None: dict(base or {})
class _Twin(TwinBase):
    PY_MODULE = "agent/checkpoints/manager.py"
    PY_FN = "_git_env"
    C_FN = "git_env_value"
    NORM_NONE = True
    def call(self, inp, inputs, idx=0):
        key, _, rest = inp.partition("|")
        store, _, rest = rest.partition("|")
        wt, _, idxf = rest.partition("|")
        env = _m._git_env(store, wt, idxf or None)
        return env.get(key)
    def norm(self, c, result):
        c.valstr("None" if result is None else str(result))
_t=_Twin(); trace=_t.trace; run=_t.run
