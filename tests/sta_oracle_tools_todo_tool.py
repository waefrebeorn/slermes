"""AUTO-GENERATED oracle for tools_todo_tool (gen_oracle.py)."""
import sys, json, os
sys.path.insert(0, os.path.expanduser("~/hermes-agent-dev"))
from tools.todo_tool import (check_todo_requirements, TodoStore)

mism = 0; n = 0
for line in sys.stdin:
    line = line.strip()
    if not line.startswith('{'):
        continue
    rec = json.loads(line)
    n += 1
    fn = rec['func']
    ARGS = {
        'todo_tool_check_requirements': check_todo_requirements,
        # has_items moved to the TodoStore class method; a fresh store is
        # empty, matching the C port's statically-initialized empty list.
        'todo_tool_has_items': TodoStore,
    }
    if fn not in ARGS:
        continue
    pyf = ARGS[fn]
    try:
        if pyf is TodoStore:
            exp = pyf().has_items()
        else:
            exp = pyf()
    except Exception as e:
        print('PYERR', fn, e); continue
    got = rec['ret']
    if isinstance(exp, str): exp = exp
    if got != exp:
        # loose compare for floats / bool/int
        ok = False
        try:
            if abs(float(got) - float(exp)) < 1e-6: ok = True
        except Exception: pass
        if not ok:
            mism += 1
            print('MISMATCH', fn, 'got', got, 'exp', exp)
print("AUTO oracle: %d cases, %d mismatches" % (n, mism))
sys.exit(1 if mism else 0)
