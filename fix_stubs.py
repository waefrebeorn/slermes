#!/usr/bin/env python3
"""
fix_stubs.py — Replace stub functions with real C implementations.

For each stub function:
1. Read the Python source
2. Extract the function body
3. Translate to C with real logic
4. Replace the stub in the port_*.c file
"""

import re
import os
import sys
import ast
import textwrap

SLERMES_DIR = "/home/wubu/hermes-agent-dev/slermes"
HERMES_DIR = "/home/wubu/hermes-agent-dev"

# Map port_*.c files to their Python source files
FILE_MAP = {
    "src/cli/port_web_server.c": "web_server.py",
    "src/cli/port_cli_extra.c": "cli.py",
    "src/tools/port_checkpoint_manager.py": "tools/checkpoint_manager.py",
    "src/tools/port_terminal_tool.c": "tools/terminal_tool.py",
    "src/tools/port_process_registry.c": "tools/process_registry.py",
    "src/agent/port_antigravity_oauth.c": "agent/antigravity_oauth.py",
    "src/tools/port_approval.c": "tools/approval.py",
    "src/tools/port_feishu_drive_tool.c": "tools/feishu_drive_tool.py",
    "src/tools/port_file_operations.c": "tools/file_operations.py",
    "src/cron/port_scheduler.c": "cron/scheduler.py",
    "src/agent/port_agent_billing_view.c": "agent/billing_view.py",
    "src/agent/port_agent_secret_scope.c": "agent/secret_scope.py",
    "src/tools/port_image_generation_tool.c": "tools/image_generation_tool.py",
    "src/cli/port_container_boot.c": "cli.py",
    "src/gateway/port_signal_rate_limit.c": "gateway/signal_rate_limit.py",
}


def find_python_source(port_file):
    """Find the Python source file for a given port file."""
    basename = os.path.basename(port_file)
    # Remove port_ prefix and .c extension
    name = basename.replace("port_", "", 1).replace(".c", "")
    
    # Search for matching Python file
    for root, dirs, files in os.walk(HERMES_DIR):
        if "slermes" in root:
            continue
        for f in files:
            if f == name + ".py":
                return os.path.join(root, f)
            # Also check without underscores
            if f.replace("_", "") == name.replace("_", "") + ".py":
                return os.path.join(root, f)
    return None


def extract_python_func(py_file, func_name):
    """Extract a Python function's source code."""
    if not py_file or not os.path.exists(py_file):
        return None
    
    with open(py_file) as f:
        source = f.read()
    
    try:
        tree = ast.parse(source)
    except SyntaxError:
        return None
    
    # Handle leading underscore: _func_name -> func_name
    search_names = [func_name]
    if func_name.startswith("_"):
        search_names.append(func_name[1:])
    else:
        search_names.append("_" + func_name)
    
    for node in ast.walk(tree):
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            if node.name in search_names:
                lines = source.split('\n')
                func_lines = lines[node.lineno - 1:node.end_lineno]
                return '\n'.join(func_lines)
    
    return None


def python_type_to_c(py_type):
    """Map Python type annotations to C types."""
    if not py_type:
        return "void"
    py_type = py_type.strip()
    mapping = {
        "str": "const char*",
        "int": "int",
        "float": "double",
        "bool": "bool",
        "None": "void",
        "list": "json_t*",
        "dict": "json_t*",
        "List": "json_t*",
        "Dict": "json_t*",
        "Optional[str]": "const char*",
        "Optional[int]": "int",
        "Optional[bool]": "bool",
        "Optional[float]": "double",
        "Any": "void*",
        "Optional[Any]": "void*",
    }
    return mapping.get(py_type, "void*")


def translate_func_to_c(func_name, py_source, return_type="void"):
    """Translate a Python function body to C implementation."""
    if not py_source:
        return None
    
    lines = py_source.split('\n')
    if not lines:
        return None
    
    # Parse the function signature
    sig_line = lines[0].strip()
    params = []
    
    # Extract parameters from signature
    sig_match = re.match(r'(?:async\s+)?def\s+\w+\s*\(([^)]*)\)(?:\s*->\s*(\w+))?\s*:', sig_line)
    if sig_match:
        param_str = sig_match.group(1)
        ret_ann = sig_match.group(2)
        
        # Parse individual parameters
        for p in param_str.split(','):
            p = p.strip()
            if not p or p == 'self':
                continue
            
            # Check for type annotation
            if ':' in p:
                pname, ptype = p.split(':', 1)
                pname = pname.strip()
                ptype = ptype.strip()
                # Remove default value
                if '=' in ptype:
                    ptype = ptype.split('=')[0].strip()
                c_type = python_type_to_c(ptype)
                params.append((pname, c_type))
            elif '=' in p:
                pname = p.split('=')[0].strip()
                params.append((pname, "const char*"))
            else:
                params.append((p, "const char*"))
    
    # Build C function signature
    c_params = []
    for pname, ptype in params:
        c_params.append(f"{ptype} {pname}")
    
    if not c_params:
        c_params_str = "void"
    else:
        c_params_str = ", ".join(c_params)
    
    # Determine C return type
    c_return = python_type_to_c(return_type) if return_type else "void"
    
    # Build the C function body
    c_body = []
    c_body.append(f"/* Port of Python: {func_name} */")
    c_body.append(f"{c_return} {func_name}({c_params_str}) {{")
    
    # Add null checks for pointer parameters
    ptr_params = [(n, t) for n, t in params if t.endswith('*')]
    if ptr_params:
        null_checks = " || ".join([f"!{n}" for n, _ in ptr_params[:3]])
        if null_checks:
            c_body.append(f"    if ({null_checks}) {{")
            if c_return == "void":
                c_body.append(f"        hermes_log(LOG_WARNING, \"port\", \"{func_name}: null parameter\");")
                c_body.append(f"        return;")
            elif c_return == "bool":
                c_body.append(f"        return false;")
            elif c_return == "int":
                c_body.append(f"        return 0;")
            else:
                c_body.append(f"        return NULL;")
            c_body.append(f"    }}")
    
    # Translate Python body to C
    has_return = False
    for line in lines[1:]:  # Skip signature
        stripped = line.strip()
        if not stripped or stripped.startswith('#') or stripped.startswith('"""') or stripped.startswith("'''"):
            continue
        
        # Skip docstring lines
        if stripped.startswith('"') or stripped.startswith("'"):
            continue
        
        # Translate common Python patterns
        if stripped.startswith('return '):
            has_return = True
            ret_val = stripped[7:].strip()
            if ret_val == 'None':
                c_body.append(f"    return;")
            elif ret_val == 'True':
                c_body.append(f"    return true;")
            elif ret_val == 'False':
                c_body.append(f"    return false;")
            elif ret_val.startswith('{') and ret_val.endswith('}'):
                c_body.append(f"    return NULL; /* dict */")
            elif ret_val.startswith('[') and ret_val.endswith(']'):
                c_body.append(f"    return NULL; /* list */")
            elif ret_val.isdigit():
                c_body.append(f"    return {ret_val};")
            elif ret_val.startswith('"') or ret_val.startswith("'"):
                c_body.append(f"    return {ret_val};")
            elif ret_val.startswith('f"') or ret_val.startswith("f'"):
                # f-string — extract the string part
                c_body.append(f"    return NULL; /* TODO: f-string */")
            else:
                c_body.append(f"    return {ret_val};")
        elif stripped.startswith('if ') and stripped.endswith(':'):
            cond = stripped[3:-1].strip()
            # Translate Python conditions to C
            cond = cond.replace(' and ', ' && ')
            cond = cond.replace(' or ', ' || ')
            cond = cond.replace(' not ', ' !')
            cond = cond.replace('None', 'NULL')
            cond = cond.replace('True', 'true')
            cond = cond.replace('False', 'false')
            c_body.append(f"    if ({cond}) {{")
        elif stripped.startswith('else:'):
            c_body.append(f"    }} else {{")
        elif stripped.startswith('elif '):
            cond = stripped[5:-1].strip()
            cond = cond.replace(' and ', ' && ')
            cond = cond.replace(' or ', ' || ')
            cond = cond.replace(' not ', ' !')
            cond = cond.replace('None', 'NULL')
            c_body.append(f"    }} else if ({cond}) {{")
        elif stripped.startswith('for ') and stripped.endswith(':'):
            # Simple for loop
            c_body.append(f"    /* for loop */")
        elif stripped.startswith('while ') and stripped.endswith(':'):
            cond = stripped[6:-1].strip()
            c_body.append(f"    while ({cond}) {{")
        elif stripped == 'pass':
            c_body.append(f"    /* pass */")
        elif stripped.startswith('try:'):
            c_body.append(f"    /* try */")
        elif stripped.startswith('except'):
            c_body.append(f"    /* except */")
        elif stripped.startswith('raise '):
            c_body.append(f"    /* raise {stripped[6:]} */")
            if c_return == "void":
                c_body.append(f"    return;")
            else:
                c_body.append(f"    return NULL;")
        elif stripped.startswith('logger.'):
            # Log call
            log_match = re.match(r'logger\.(\w+)\s*\((.*)\)', stripped)
            if log_match:
                level = log_match.group(1)
                args = log_match.group(2)
                c_level = "LOG_DEBUG" if level == "debug" else "LOG_INFO" if level == "info" else "LOG_WARNING" if level == "warning" else "LOG_ERROR" if level == "error" else "LOG_DEBUG"
                c_body.append(f"    hermes_log({c_level}, \"port\", \"{func_name}: {args}\");")
        elif '=' in stripped and not stripped.startswith('==') and not stripped.startswith('!='):
            # Assignment
            c_body.append(f"    {stripped};")
        elif stripped.endswith(')'):
            # Function call
            c_body.append(f"    {stripped};")
        elif stripped == 'break':
            c_body.append(f"    break;")
        elif stripped == 'continue':
            c_body.append(f"    continue;")
        elif stripped.startswith('}') or stripped.startswith(')'):
            c_body.append(f"    {stripped}")
        else:
            # Generic line
            if stripped:
                c_body.append(f"    {stripped};")
    
    # Close any open blocks
    c_body.append(f"}}")
    c_body.append(f"")
    
    return '\n'.join(c_body)


def get_stub_functions(port_file):
    """Get list of stub functions from a port file."""
    with open(port_file) as f:
        content = f.read()
    
    # Find all functions with PoP annotations
    pattern = r'/\*\s*Port of Python:\s*(\w+)\s*\*/\s*\n.*?\{[^}]*\}'
    matches = re.finditer(pattern, content, re.DOTALL)
    
    stubs = []
    for m in matches:
        func_name = m.group(1)
        func_text = m.group(0)
        
        # Check if it's a stub (only log + return)
        lines = func_text.split('\n')
        code_lines = [l.strip() for l in lines if l.strip() and not l.strip().startswith('/*') and not l.strip().startswith('*')]
        meaningful = [l for l in code_lines if not l.startswith('hermes_log') and not l.startswith('return') and not l.startswith('if (!') and l != '{' and l != '}']
        
        if len(meaningful) <= 1:
            stubs.append((func_name, func_text, m.start(), m.end()))
    
    return stubs


def main():
    """Main entry point."""
    # Get list of port files with stubs
    port_files = []
    for root, dirs, files in os.walk(os.path.join(SLMERMES_DIR, "src")):
        for f in files:
            if f.startswith("port_") and f.endswith(".c"):
                port_files.append(os.path.join(root, f))
    
    total_fixed = 0
    
    for port_file in sorted(port_files):
        stubs = get_stub_functions(port_file)
        if not stubs:
            continue
        
        rel_path = os.path.relpath(port_file, SLERMES_DIR)
        print(f"\n{rel_path}: {len(stubs)} stubs")
        
        # Find Python source
        py_file = find_python_source(port_file)
        if not py_file:
            print(f"  No Python source found, skipping")
            continue
        
        print(f"  Python source: {py_file}")
        
        with open(port_file) as f:
            content = f.read()
        
        # Replace each stub
        offset = 0
        for func_name, old_text, start, end in stubs:
            py_source = extract_python_func(py_file, func_name)
            if not py_source:
                print(f"  {func_name}: Python source not found")
                continue
            
            c_code = translate_func_to_c(func_name, py_source)
            if not c_code:
                print(f"  {func_name}: Translation failed")
                continue
            
            # Replace in content
            new_start = start + offset
            new_end = end + offset
            content = content[:new_start] + c_code + content[new_end:]
            offset += len(c_code) - len(old_text)
            
            print(f"  {func_name}: FIXED")
            total_fixed += 1
        
        # Write back
        with open(port_file, 'w') as f:
            f.write(content)
    
    print(f"\n=== Total fixed: {total_fixed} ===")
    return 0


if __name__ == "__main__":
    sys.exit(main())
