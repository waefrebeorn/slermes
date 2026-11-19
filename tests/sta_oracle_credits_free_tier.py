"""Oracle for credits_free_tier: C port vs LIVE quarry Python.

Replays the harness's pricing-cache seeds in order (both sides accumulate
identical cache state), then compares is_free_tier_model verdicts. The
quarry import chain needs ruamel et al.; when the current interpreter lacks
them, re-exec under the quarry venv (stdin survives execv).
"""
import os
import sys
import json

for _quarry in ("~/hermes-agent", "~/hermes-agent-dev"):
    _p = os.path.expanduser(_quarry)
    if os.path.isdir(os.path.join(_p, "agent")):
        sys.path.insert(0, _p)
        break

try:
    from agent.credits_tracker import is_free_tier_model
    from hermes_cli.models_pricing import _cache_catalog
except ImportError:
    _venv = os.path.expanduser("~/hermes-agent/.venv/Scripts/python.exe")
    if not os.path.isfile(_venv):
        _venv = os.path.expanduser("~/hermes-agent/.venv/bin/python")
    if os.path.isfile(_venv) and \
            os.path.abspath(sys.executable) != os.path.abspath(_venv):
        os.execv(_venv, [_venv, os.path.abspath(__file__)] + sys.argv[1:])
    raise

mism = 0
n = 0
for line in sys.stdin:
    line = line.strip()
    if not line.startswith('{'):
        continue
    rec = json.loads(line)
    n += 1
    if rec.get('seed_key') is not None:
        _cache_catalog(rec['seed_key'], rec['seed_catalog'])
    try:
        exp = is_free_tier_model(rec['model'], rec['base_url'])
    except Exception as e:
        print('PYERR', rec['model'], e)
        continue
    got = rec['ret']
    if bool(got) != bool(exp):
        mism += 1
        print('MISMATCH', rec['model'], rec['base_url'],
              'got', got, 'exp', exp)
print("AUTO oracle: %d cases, %d mismatches" % (n, mism))
sys.exit(1 if mism else 0)
