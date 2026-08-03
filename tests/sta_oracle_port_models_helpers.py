"""AUTO-GENERATED integration oracle for port_models_helpers (gen_integration_oracle.py)."""
import os, sys, json, importlib.util

MODS = {}
def _load(rel):
    repo = os.path.realpath(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
    devroot = os.path.dirname(repo)  # hermes_cli/ lives in the dev-tree parent of slermes
    for p in (repo, devroot):
        if p not in sys.path: sys.path.insert(0, p)
    for base in sys.path:
        cand = os.path.join(base, rel)
        try:
            spec = importlib.util.spec_from_file_location('live_' + rel.replace('/', '_').replace('.', '_'), cand)
            mod = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(mod)
            return mod
        except Exception:
            continue
    return None
MODS['hermes_cli.models'] = _load('hermes_cli/models.py')

DISPATCH = {
    'base_url_looks_like_anthropic_messages': ('hermes_cli.models', '_base_url_looks_like_anthropic_messages'),
    'is_openai_fast_model': ('hermes_cli.models', '_is_openai_fast_model'),
    'is_anthropic_fast_model': ('hermes_cli.models', '_is_anthropic_fast_model'),
    'model_supports_fast_mode': ('hermes_cli.models', 'model_supports_fast_mode'),
    'openrouter_model_is_free': ('hermes_cli.models', '_openrouter_model_is_free'),
    'is_github_models_base_url': ('hermes_cli.models', '_is_github_models_base_url'),
    'should_use_copilot_responses_api': ('hermes_cli.models', '_should_use_copilot_responses_api'),
}

def main():
    if len(sys.argv) < 2: sys.stderr.write("usage: sta_oracle_port_models_helpers.py <cases.json>\n"); return 2
    with open(sys.argv[1], 'r', encoding='utf-8') as f: cases = json.load(f)
    for c in cases:
        op = c.get('op'); value = c.get('value', '')
        d = DISPATCH.get(op)
        if not d: sys.stdout.write(json.dumps({'fn':op}, separators=(',',':')) + '\n'); continue
        pymod, pyfn = d
        mod = MODS.get(pymod)
        # Production callers pass structured values (dicts/lists) to these
        # functions — e.g. _openrouter_model_is_free receives the parsed
        # pricing dict, not its JSON text. Parse JSON-looking inputs for them
        # so the live Python sees the same object the C port sees after
        # json_parse. Other functions take strings and must keep the raw text.
        STRUCTURED_OPS = {'openrouter_model_is_free'}
        if op in STRUCTURED_OPS and isinstance(value, str):
            try:
                parsed = json.loads(value)
                if isinstance(parsed, (dict, list)):
                    value = parsed
            except Exception:
                pass
        try:
            out = getattr(mod, pyfn)(value) if mod else None
        except Exception as e:
            out = 'PYERR:' + str(e)
        if isinstance(out, bool): out = bool(out)
        elif isinstance(out, (int, float)) and not isinstance(out, bool): out = int(out)
        elif out is None: out = ''
        else: out = str(out)
        sys.stdout.write(json.dumps({'fn':op,'out':out}, ensure_ascii=True, separators=(',',':')) + '\n')
    return 0

if __name__ == '__main__':
    sys.exit(main())
