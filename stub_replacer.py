#!/usr/bin/env python3
"""
stub_replacer.py — Replace stub functions with real C implementations.

Reads the Python source for each gap function, translates the logic to C,
and replaces the stub body in the port_*.c file.

Strategy:
1. For each stub function, read the Python source
2. Translate Python logic to C using the project's existing patterns
3. Replace the stub body with the translated C code
4. Build and verify
"""

import os
import re
import json
import sys

SLERMES_DIR = os.path.dirname(os.path.abspath(__file__))
MODULES_DIR = os.path.join(os.path.dirname(SLERMES_DIR), '..')
DEPTH_CHECK_RESULTS = os.path.join(SLERMES_DIR, 'depth_check_results.json')

# ── Python-to-C translation helpers ──

def python_type_to_c(py_type):
    """Map Python type annotation to C type."""
    if not py_type:
        return 'void*'
    py_type = py_type.strip()
    mapping = {
        'int': 'int', 'float': 'double', 'str': 'const char*',
        'bool': 'bool', 'None': 'void', 'list': 'json_t*',
        'dict': 'json_t*', 'Any': 'void*', 'Optional[str]': 'const char*',
        'Optional[int]': 'int', 'Optional[bool]': 'bool', 'Optional[float]': 'double',
        'Optional[dict]': 'json_t*', 'Optional[list]': 'json_t*',
        'List[str]': 'json_t*', 'List[int]': 'json_t*', 'Dict[str, Any]': 'json_t*',
        'Dict[str, str]': 'json_t*', 'Path': 'const char*', 'float | None': 'double',
        'str | None': 'const char*', 'int | None': 'int',
    }
    if py_type in mapping:
        return mapping[py_type]
    if 'Optional[' in py_type:
        return 'void*'
    if 'List[' in py_type or 'Dict[' in py_type:
        return 'json_t*'
    if 'Path' in py_type:
        return 'const char*'
    return 'void*'

def extract_python_func(filepath, func_name):
    """Extract a Python function's source code."""
    with open(filepath) as f:
        lines = f.readlines()
    
    # Find function definition
    pattern = rf'^(\s*)(async\s+)?def\s+{re.escape(func_name)}\s*\('
    for i, line in enumerate(lines):
        if re.match(pattern, line):
            # Get signature
            sig_lines = [line]
            paren_depth = line.count('(') - line.count(')')
            j = i + 1
            while paren_depth > 0 and j < len(lines):
                sig_lines.append(lines[j])
                paren_depth += lines[j].count('(') - lines[j].count(')')
                j += 1
            
            # Get return type annotation
            ret_type = None
            if '->' in sig_lines[-1]:
                ret_match = re.search(r'->\s*([^:]+)\s*:', sig_lines[-1])
                if ret_match:
                    ret_type = ret_match.group(1).strip()
            
            # Get parameters
            sig = ''.join(sig_lines)
            params = []
            # Extract param names and types from signature
            param_str = sig[sig.index('(')+1:sig.rindex('))')]
            param_str = param_str.replace('\n', ' ').strip()
            if param_str and param_str not in ('self', 'cls', ''):
                for p in param_str.split(','):
                    p = p.strip()
                    if p in ('self', 'cls', '*args', '**kwargs') or p.startswith('**'):
                        continue
                    if p.startswith('*') and p != '*args':
                        p = p.lstrip('*')
                    # Parse: name: type = default
                    pm = re.match(r'(\w+)\s*(?::\s*([^=]+?))?\s*(?:=\s*.+)?$', p)
                    if pm:
                        pname = pm.group(1)
                        ptype = pm.group(2).strip() if pm.group(2) else None
                        params.append((pname, python_type_to_c(ptype)))
            
            # Get body
            base_indent = len(line) - len(line.lstrip())
            body_lines = []
            k = j
            while k < len(lines):
                l = lines[k]
                if l.strip() == '':
                    body_lines.append(l)
                    k += 1
                    continue
                curr_indent = len(l) - len(l.lstrip())
                if curr_indent <= base_indent and (l.strip().startswith('def ') or l.strip().startswith('async def ') or l.strip().startswith('class ')):
                    break
                body_lines.append(l)
                k += 1
            
            return {
                'signature': ''.join(sig_lines).strip(),
                'body': ''.join(body_lines),
                'params': params,
                'ret_type': python_type_to_c(ret_type),
                'is_async': 'async def' in line,
                'line': i + 1,
            }
    return None

def needs_json_import(body):
    """Check if the Python function uses JSON."""
    return bool(re.search(r'\bjson\.', body))

def needs_string_ops(body):
    """Check if the Python function uses string operations."""
    return bool(re.search(r'\.split\(|\.strip\(|\.replace\(|\.startswith\(|\.endswith\(', body))

def needs_file_ops(body):
    """Check if the Python function uses file operations."""
    return bool(re.search(r'\bopen\(|\.read\(|\.write\(|\.exists\(|\.mkdir\(', body))

def needs_subprocess(body):
    """Check if the Python function uses subprocess."""
    return 'subprocess.' in body

def needs_import_basic(body):
    """Check if the Python function uses basic imports."""
    return bool(re.search(r'\bimport\s+(os|sys|re|json|time|pathlib|shutil|base64|hmac|secrets|urllib)', body))

def build_includes_for_func(body, existing_includes):
    """Determine what C headers are needed based on Python usage."""
    includes = set(existing_includes)
    
    if needs_json_import(body):
        includes.add('#include "hermes_json.h"')
    if needs_string_ops(body):
        includes.add('#include <string.h>')
    if needs_file_ops(body):
        includes.add('#include <stdio.h>')
    if needs_subprocess(body):
        includes.add('#include <stdlib.h>')
    if 'Path(' in body or 'os.path' in body:
        includes.add('#include <sys/stat.h>')
    if 'time.' in body or 'sleep' in body:
        includes.add('#include <time.h>')
        includes.add('#include <unistd.h>')
    if 'threading.' in body:
        includes.add('#include <pthread.h>')
    if 'base64.' in body:
        includes.add('#include "libbase64/base64.h"')
    if 're.' in body:
        includes.add('#include "libregex/hermes_regex.h"')
    if 'yaml.' in body or 'yaml ' in body:
        includes.add('#include "libyaml/yaml.h"')
    if 'hmac.' in body:
        includes.add('#include <openssl/hmac.h>')
    if 'secrets.' in body:
        includes.add('#include <openssl/rand.h>')
    if 'urllib.' in body:
        includes.add('#include "libhttp/http.h"')
    
    return sorted(includes)

def translate_python_body_to_c(func_name, py_info):
    """
    Translate a Python function body to C.
    Returns the C function body as a string, or None if too complex.
    """
    body = py_info['body'].strip()
    params = py_info['params']
    ret_type = py_info['ret_type']
    
    # Build C function signature
    c_params = []
    for pname, ptype in params:
        c_params.append(f'{ptype} {pname}')
    params_str = ', '.join(c_params) if c_params else 'void'
    
    # Return statement
    if ret_type == 'void':
        ret_stmt = 'return;'
    elif ret_type == 'int':
        ret_stmt = 'return 0;'
    elif ret_type == 'bool':
        ret_stmt = 'return false;'
    elif ret_type == 'const char*':
        ret_stmt = 'return NULL;'
    elif ret_type == 'double':
        ret_stmt = 'return 0.0;'
    else:
        ret_stmt = 'return NULL;'
    
    # Try to translate the Python body to C
    c_body_lines = []
    
    # Track variable declarations
    local_vars = {}
    
    for line in body.split('\n'):
        stripped = line.strip()
        if not stripped or stripped.startswith('#') or stripped.startswith('"""') or stripped.startswith("'''"):
            continue
        
        indent = len(line) - len(line.lstrip())
        c_indent = '    ' + ' ' * indent
        
        # Skip async/await
        if stripped.startswith('await '):
            stripped = stripped[6:]
        
        # Skip decorator lines
        if stripped.startswith('@'):
            continue
        
        # Skip docstring lines
        if stripped.startswith('"""') or stripped.startswith("'''"):
            # Check if it's a single-line docstring
            if stripped.count('"""') == 2 or stripped.count("'''") == 2:
                continue
            # Multi-line docstring - skip until closing
            continue
        
        # Translate common Python patterns
        translated = translate_python_line(stripped, ret_stmt, local_vars)
        if translated:
            if isinstance(translated, list):
                c_body_lines.extend([c_indent + l for l in translated])
            else:
                c_body_lines.append(c_indent + translated)
    
    # If we couldn't translate anything meaningful, return None
    if not c_body_lines:
        return None
    
    # Build the full function
    func_lines = [
        f'/* Port of Python: {func_name} */',
        f'{ret_type} {func_name}({params_str})',
        '{',
    ]
    func_lines.extend(c_body_lines)
    
    # Add return if not already there
    has_return = any('return ' in l for l in c_body_lines)
    if not has_return and ret_type != 'void':
        func_lines.append('    ' + ret_stmt)
    
    func_lines.append('}')
    
    return '\n'.join(func_lines) + '\n'

def translate_python_line(line, ret_stmt, local_vars):
    """Translate a single Python line to C. Returns C line(s) or None."""
    
    # Function call: module.func(args)
    m = re.match(r'(\w+)\.(\w+)\((.+)\)', line)
    if m:
        module, func, args = m.group(1), m.group(2), m.group(3)
        return translate_module_call(module, func, args)
    
    # Assignment: var = value
    m = re.match(r'(\w+)\s*=\s*(.+)', line)
    if m:
        var, value = m.group(1), m.group(2).strip()
        return translate_assignment(var, value, local_vars)
    
    # Return statement
    m = re.match(r'return\s+(.+)', line)
    if m:
        value = m.group(1).strip()
        return 'return ' + translate_expression(value) + ';'
    
    # If statement
    m = re.match(r'if\s+(.+):', line)
    if m:
        cond = m.group(1).strip()
        return 'if (' + translate_condition(cond) + ') {'
    
    # For loop
    m = re.match(r'for\s+(\w+)\s+in\s+(.+):', line)
    if m:
        var, iterable = m.group(1), m.group(2).strip()
        return translate_for_loop(var, iterable)
    
    # Try/except
    m = re.match(r'try:', line)
    if m:
        return '/* try */ {'
    
    m = re.match(r'except\s*(.*?):', line)
    if m:
        return '} /* catch */ {'
    
    # Pass
    if line == 'pass':
        return '/* pass */'
    
    # Raise
    m = re.match(r'raise\s+(.+)', line)
    if m:
        exc = m.group(1).strip()
        return f'/* raise {exc} */'
    
    # Bare function call
    m = re.match(r'(\w+)\((.+)\)', line)
    if m:
        func, args = m.group(1), m.group(2)
        return func + '(' + translate_args(args) + ');'
    
    return None

def translate_module_call(module, func, args):
    """Translate module.func(args) calls."""
    args_c = translate_args(args)
    
    # Common patterns
    if module == 'json' and func == 'loads':
        return f'json_parse({args_c})'
    if module == 'json' and func == 'dumps':
        return f'json_stringify({args_c})'
    if module == 'os' and func == 'getenv':
        return f'getenv({args_c})'
    if module == 'os' and func == 'path' and 'join' in args:
        return f'path_join({args_c})'
    if module == 're' and func == 'search':
        return f'hermes_regex_search({args_c})'
    if module == 're' and func == 'match':
        return f'hermes_regex_match({args_c})'
    if module == 'time' and func == 'sleep':
        return f'sleep({args_c})'
    if module == 'time' and func == 'time':
        return f'(int)time(NULL)'
    if module == 'shutil' and func == 'which':
        return f'({args_c} ? {args_c} : NULL)'
    if module == 'subprocess' and func == 'run':
        return f'system({args_c})'
    if module == 'base64' and func in ('b64encode', 'b64decode'):
        return f'base64_{func[3:]}({args_c})'
    if module == 'hmac' and func == 'compare_digest':
        return f'(strcmp({args_c}) == 0)'
    if module == 'secrets' and func == 'token_hex':
        return f'rand_hex({args_c})'
    if module == 'os' and func == 'path':
        return f'({args_c})'
    if module == 'builtins' or module == '':
        return f'{func}({args_c})'
    
    # Generic: try the function name directly
    return f'{module}_{func}({args_c})'

def translate_args(args_str):
    """Translate Python function arguments to C."""
    if not args_str.strip():
        return ''
    
    # Simple approach: just clean up common patterns
    result = args_str.strip()
    
    # Replace Python True/False/None
    result = result.replace('True', 'true').replace('False', 'false').replace('None', 'NULL')
    
    # Replace f-strings (simplified)
    result = re.sub(r'f"([^"]*)"', r'"\1"', result)
    result = re.sub(r"f'([^']*)'", r'"\1"', result)
    
    # Replace .format()
    result = re.sub(r'\.format\((.+?)\)', r' /* format(\1) */', result)
    
    # Replace string concatenation with +
    result = result.replace(" + ", ", ")
    
    return result

def translate_assignment(var, value, local_vars):
    """Translate Python assignment to C."""
    value = value.strip()
    
    # Track variable type
    if value.startswith("'") or value.startswith('"'):
        local_vars[var] = 'const char*'
    elif value.replace('.', '').replace('-', '').isdigit():
        if '.' in value:
            local_vars[var] = 'double'
        else:
            local_vars[var] = 'int'
    elif value == 'True' or value == 'False':
        local_vars[var] = 'bool'
    elif value == 'None':
        local_vars[var] = 'void*'
    elif value.startswith('['):
        local_vars[var] = 'json_t*'
    elif value.startswith('{'):
        local_vars[var] = 'json_t*'
    else:
        local_vars[var] = 'const char*'
    
    c_type = local_vars.get(var, 'const char*')
    c_value = translate_expression(value)
    
    return f'{c_type} {var} = {c_value};'

def translate_expression(expr):
    """Translate a Python expression to C."""
    expr = expr.strip()
    
    # Literals
    if expr == 'True':
        return 'true'
    if expr == 'False':
        return 'false'
    if expr == 'None':
        return 'NULL'
    
    # f-strings → string literals
    if expr.startswith('f"') or expr.startswith("f'"):
        inner = expr[2:-1]
        inner = re.sub(r'\{(\w+)\}', r'" \1 "', inner)
        return inner
    
    # String methods
    m = re.match(r'(\w+)\.strip\(\)', expr)
    if m:
        var = m.group(1)
        return f'(/* strip({var}) */ {var})'
    
    m = re.match(r'(\w+)\.split\((.+)\)', expr)
    if m:
        var, sep = m.group(1), m.group(2)
        return f'({var} /* split({sep}) */)'
    
    m = re.match(r'(\w+)\.startswith\((.+)\)', expr)
    if m:
        var, prefix = m.group(1), m.group(2)
        clean_prefix = prefix.strip("'\"")
        return f'(strncmp({var}, "{clean_prefix}", {len(clean_prefix)}) == 0)'
    
    m = re.match(r'(\w+)\.encode\(\)', expr)
    if m:
        return m.group(1)
    
    m = re.match(r'(\w+)\.decode\(\)', expr)
    if m:
        return m.group(1)
    
    m = re.match(r'(\w+)\.get\((.+)\)', expr)
    if m:
        var, key = m.group(1), m.group(2).strip().strip("'\"")
        return f'json_object_get({var}, "{key}")'
    
    m = re.match(r'len\((\w+)\)', expr)
    if m:
        return f'(int)strlen({m.group(1)})'
    
    m = re.match(r'bool\((.+)\)', expr)
    if m:
        return f'!!({m.group(1)})'
    
    m = re.match(r'str\((.+)\)', expr)
    if m:
        return m.group(1)
    
    m = re.match(r'int\((.+)\)', expr)
    if m:
        return f'(int)({m.group(1)})'
    
    m = re.match(r'not\s+(.+)', expr)
    if m:
        return f'!({m.group(1)})'
    
    # Boolean operations
    if ' and ' in expr:
        parts = expr.split(' and ')
        return ' && '.join(translate_expression(p.strip()) for p in parts)
    if ' or ' in expr:
        parts = expr.split(' or ')
        return ' || '.join(translate_expression(p.strip()) for p in parts)
    
    # Comparison
    expr = expr.replace('==', '==').replace('!=', '!=').replace('>=', '>=').replace('<=', '<=')
    
    # Return statement with expression
    return expr

def translate_condition(cond):
    """Translate Python condition to C."""
    cond = cond.strip()
    cond = cond.replace('True', 'true').replace('False', 'false').replace('None', 'NULL')
    cond = cond.replace(' and ', ' && ').replace(' or ', ' || ')
    cond = cond.replace(' not ', ' !')
    cond = re.sub(r'not\s+', '!', cond)
    cond = re.sub(r'(\w+)\.startswith\((.+?)\)', r'(strncmp(\1, \2, strlen(\2)) == 0)', cond)
    cond = re.sub(r'(\w+)\.endswith\((.+?)\)', r'/* endswith */ false', cond)
    cond = re.sub(r'(\w+)\.isdigit\(\)', r'isdigit(\1)', cond)
    cond = re.sub(r'len\((\w+)\)', r'strlen(\1)', cond)
    return cond

def translate_for_loop(var, iterable):
    """Translate Python for loop to C."""
    # range(n)
    m = re.match(r'range\((\d+)\)', iterable)
    if m:
        n = m.group(1)
        return f'int {var}; for ({var} = 0; {var} < {n}; {var}++) {{'
    
    m = re.match(r'range\((\w+),\s*(\w+)\)', iterable)
    if m:
        start, end = m.group(1), m.group(2)
        return f'int {var}; for ({var} = {start}; {var} < {end}; {var}++) {{'
    
    # iter over a list
    return f'/* foreach {var} in {iterable} */ {{'


def main():
    # Load depth check results
    with open(DEPTH_CHECK_RESULTS) as f:
        depth = json.load(f)
    
    # Get list of stub functions to fix, sorted by file priority
    stub_files = {}
    for func_info in depth.get('functions', []):
        if func_info['classification'] == 'STUB':
            f = func_info['file']
            if f not in stub_files:
                stub_files[f] = []
            stub_files[f].append(func_info)
    
    print(f"Found {sum(len(v) for v in stub_files.values())} stubs in {len(stub_files)} files")
    print(f"\nTop 10 files by stub count:")
    for f, funcs in sorted(stub_files.items(), key=lambda x: -len(x[1]))[:10]:
        print(f"  {len(funcs):3d} stubs  {f}")
    
    # For now, just report — the actual translation is complex
    print(f"\n=== Translation strategy ===")
    print(f"For each stub function:")
    print(f"  1. Read Python source from upstream hermes_cli/agent/gateway/tools/")
    print(f"  2. Translate Python logic to C using project types")
    print(f"  3. Replace stub body in port_*.c file")
    print(f"  4. Build and verify")
    print(f"\nThis requires careful per-function translation.")
    print(f"The automated translator would need to handle:")
    print(f"  - Python string operations → C string.h")
    print(f"  - Python dict/list → json_t*")
    print(f"  - Python subprocess → system()/popen()")
    print(f"  - Python imports → C #include")
    print(f"  - Python class methods → C functions with ctx param")

if __name__ == '__main__':
    main()
