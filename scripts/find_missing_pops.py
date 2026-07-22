"""Find REAL_GAP features where a matching C function EXISTS (so the port is
done) but no PoP annotation credits it. Uses the battleground's own
classify_feature + C-discovery so results are real, not guessed.

For each such feature we print the C function name + file so a single-line
PoP can be added, closing genuine already-ported gaps whose annotation was
dropped/split during monolith work.
"""
import sys, importlib.util, os
sys.path.insert(0, 'tests')
spec = importlib.util.spec_from_file_location("bg", "tests/slermes_parity_battleground.py")
bg = importlib.util.module_from_spec(spec); spec.loader.exec_module(bg)

a = bg.ParityAnalyzer(); ci = a.c_index
credited = set(p.c_function for p in ci.pop_annotations)

providers = ["openai","anthropic","google","xai","groq","mistral","deepseek",
             "ollama","bedrock","azure","openrouter","qwen","meta","cohere","moa"]

def c_func_for_feature(feat, py_file):
    for prov in providers:
        if py_file.replace('\\','/').endswith(f"providers/{prov}.py"):
            cf = ci.find_c_function_with_prefix(feat.name, prov)
            if cf: return cf
    cf = ci.find_c_function(feat.name, py_file, feat.parent_class)
    if cf: return cf
    cf = ci.find_vtable_defaults_global(py_file.replace('.py',''), feat.name)
    if cf: return cf
    _, claims = ci.find_wrapper_for_module(py_file)
    for c in claims:
        if feat.name in c.lower():
            return [c]
    return []

# Replicate analyze()'s file walk
from pathlib import Path
PYTHON_SOURCE_DIRS = bg.PYTHON_SOURCE_DIRS
all_py = []
for key, d in PYTHON_SOURCE_DIRS.items():
    if isinstance(d, Path) and d.is_file():
        all_py.append((d, f"{key}/{d.name}"))
    elif isinstance(d, Path) and d.is_dir():
        for f in sorted(d.rglob("*.py")):
            if f.name == "__init__.py": continue
            rel = f.relative_to(d)
            all_py.append((f, f"{key}/" + str(rel)))
all_py.sort(key=lambda x: x[1])

found = []
for py_file, display in all_py:
    feats = a.extractor.extract_file(py_file)
    for feat in feats:
        gap = a.classify_feature(display, feat)
        if gap.classification != 'REAL_GAP':
            continue
        cfs = c_func_for_feature(feat, display)
        cfs = [c.name for c in cfs if c.name.lower() not in credited]
        if cfs:
            found.append((display, feat.name, sorted(set(cfs))))

print(f"REAL_GAP features with an existing-but-uncredited C function: {len(found)}")
for mod, name, cfs in found:
    print(f"  {mod}:{name}  ->  {', '.join(cfs)}")
