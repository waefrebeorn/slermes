#!/usr/bin/env python3
"""
triple_devil_advocate.py — The Plumber, The Painter, The Devil's Advocate

Three-layer audit:
1. PLUMBER: Deep dive into function wiring — are the pipes connected right?
2. PAINTER: UX parity — does the C app paint the same picture as the TS app?  
3. DEVIL'S ADVOCATE: "That sounds too good to be true" — find the lies

Usage:
    python3 triple_devil_advocate.py [--json] [--fix] [--verbose]
"""

import os, re, sys, json, argparse, subprocess, hashlib
from pathlib import Path
from dataclasses import dataclass, field, asdict
from typing import Optional

SLERMES_DIR = "/home/wubu/hermes-agent-dev/slermes"
APPS_DESKTOP = "/home/wubu/hermes-agent-dev/apps/desktop"
APPS_SHARED = "/home/wubu/hermes-agent-dev/apps/shared"
WEB_SRC = "/home/wubu/hermes-agent-dev/web/src"
HERMES_CLI = "/home/wubu/hermes-agent-dev/hermes_cli"

@dataclass
class AuditFinding:
    layer: str       # "PLUMBER", "PAINTER", "DEVIL"
    severity: str    # "CRITICAL", "HIGH", "MEDIUM", "LOW"
    category: str
    file: str
    line: int
    detail: str
    fix: str

# ═══════════════════════════════════════════════════════════════════════════
# LAYER 1: THE PLUMBER — Function wiring deep dive
# ═══════════════════════════════════════════════════════════════════════════

class Plumber:
    """Checks that functions are properly wired — signatures match, 
    return types are correct, parameters are complete."""
    
    def __init__(self):
        self.findings: list[AuditFinding] = []
    
    def audit_c_file(self, filepath: str) -> None:
        try:
            with open(filepath, 'r', errors='replace') as f:
                content = f.read()
                lines = content.split('\n')
        except Exception:
            return
        
        rel_path = filepath.replace(SLERMES_DIR + "/", "")
        
        # Check for common plumbing issues
        self._check_void_returns(content, lines, rel_path)
        self._check_null_checks(content, lines, rel_path)
        self._check_memory_leaks(content, lines, rel_path)
        self._check_uninitialized_vars(content, lines, rel_path)
        self._check_buffer_overflows(content, lines, rel_path)
        self._check_missing_includes(content, lines, rel_path)
        self._check_dead_code(content, lines, rel_path)
        self._check_inconsistent_signatures(content, lines, rel_path)
    
    def _check_void_returns(self, content, lines, filepath):
        """Check for return statements in void functions."""
        func_pattern = r'(?:static\s+)?void\s+(\w+)\s*\([^)]*\)\s*\{'
        for m in re.finditer(func_pattern, content):
            func_name = m.group(1)
            # Find function body
            start = m.end()
            depth = 1
            pos = start
            while depth > 0 and pos < len(content):
                if content[pos] == '{': depth += 1
                elif content[pos] == '}': depth -= 1
                pos += 1
            body = content[start:pos]
            
            # Check for return with value
            for rm in re.finditer(r'return\s+([^;]+);', body):
                ret_val = rm.group(1).strip()
                if ret_val and ret_val != ';' and ret_val != '':
                    line_num = content[:start + rm.start()].count('\n') + 1
                    self.findings.append(AuditFinding(
                        layer="PLUMBER", severity="HIGH", category="VOID_RETURN",
                        file=filepath, line=line_num,
                        detail=f"void function '{func_name}' returns value '{ret_val}'",
                        fix=f"Change return type or remove return value"
                    ))
    
    def _check_null_checks(self, content, lines, filepath):
        """Check for missing NULL checks after malloc/calloc."""
        for i, line in enumerate(lines):
            if re.search(r'\w+\s*=\s*(?:malloc|calloc|realloc)\s*\(', line):
                # Check next few lines for NULL check
                found_check = False
                for j in range(i+1, min(i+5, len(lines))):
                    if re.search(r'if\s*\(?\s*\w+\s*==\s*NULL', lines[j]) or \
                       re.search(r'if\s*\(?\s*!\s*\w+', lines[j]):
                        found_check = True
                        break
                if not found_check:
                    self.findings.append(AuditFinding(
                        layer="PLUMBER", severity="MEDIUM", category="NULL_CHECK",
                        file=filepath, line=i+1,
                        detail=f"malloc/calloc without NULL check",
                        fix=f"Add NULL check after allocation"
                    ))
    
    def _check_memory_leaks(self, content, lines, filepath):
        """Check for malloc without corresponding free."""
        allocs = []
        frees = []
        for i, line in enumerate(lines):
            for m in re.finditer(r'(\w+)\s*=\s*(?:malloc|calloc|realloc)\s*\(', line):
                allocs.append((m.group(1), i+1))
            for m in re.finditer(r'free\s*\(\s*(\w+)\s*\)', line):
                frees.append(m.group(1))
        
        for var, line_num in allocs:
            if var not in frees:
                self.findings.append(AuditFinding(
                    layer="PLUMBER", severity="MEDIUM", category="MEM_LEAK",
                    file=filepath, line=line_num,
                    detail=f"Allocated '{var}' may not be freed",
                    fix=f"Add free({var}) in cleanup path"
                ))
    
    def _check_uninitialized_vars(self, content, lines, filepath):
        """Check for variables used before initialization."""
        for i, line in enumerate(lines):
            # char buf[1024]; followed by strcpy without memset
            if re.search(r'char\s+\w+\[\d+\]\s*;', line) and 'memset' not in line and 'calloc' not in line:
                # Check if used with strcpy/strcat in next few lines
                for j in range(i+1, min(i+10, len(lines))):
                    if re.search(r'strcpy|strcat|snprintf', lines[j]):
                        var_match = re.search(r'char\s+(\w+)\[', line)
                        if var_match:
                            var = var_match.group(1)
                            if var in lines[j]:
                                self.findings.append(AuditFinding(
                                    layer="PLUMBER", severity="LOW", category="UNINIT_VAR",
                                    file=filepath, line=i+1,
                                    detail=f"Buffer '{var}' may be used uninitialized",
                                    fix=f"Add memset({var}, 0, sizeof({var}))"
                                ))
                                break
    
    def _check_buffer_overflows(self, content, lines, filepath):
        """Check for unsafe string operations."""
        unsafe_funcs = ['strcpy', 'strcat', 'sprintf', 'gets']
        for i, line in enumerate(lines):
            for func in unsafe_funcs:
                if func + '(' in line and not line.strip().startswith('/*') and not line.strip().startswith('*'):
                    self.findings.append(AuditFinding(
                        layer="PLUMBER", severity="HIGH", category="BUFFER_OVERFLOW",
                        file=filepath, line=i+1,
                        detail=f"Unsafe function '{func}' used",
                        fix=f"Replace with {func}_n or snprintf"
                    ))
    
    def _check_missing_includes(self, content, lines, filepath):
        """Check for missing includes based on function usage."""
        needed_includes = {
            'malloc': '<stdlib.h>', 'calloc': '<stdlib.h>', 'realloc': '<stdlib.h>', 'free': '<stdlib.h>',
            'strlen': '<string.h>', 'strcpy': '<string.h>', 'strncpy': '<string.h>',
            'strcat': '<string.h>', 'strncat': '<string.h>', 'strcmp': '<string.h>',
            'strstr': '<string.h>', 'memset': '<string.h>', 'memcpy': '<string.h>',
            'memmove': '<string.h>', 'snprintf': '<stdio.h>', 'fprintf': '<stdio.h>',
            'fopen': '<stdio.h>', 'fclose': '<stdio.h>', 'fread': '<stdio.h>',
            'fwrite': '<stdio.h>', 'time': '<time.h>', 'time_t': '<time.h>',
            'opendir': '<dirent.h>', 'readdir': '<dirent.h>', 'closedir': '<dirent.h>',
            'stat': '<sys/stat.h>', 'mkdir': '<sys/stat.h>',
            'remove': '<stdio.h>', 'rename': '<stdio.h>',
            'setenv': '<stdlib.h>', 'unsetenv': '<stdlib.h>', 'getenv': '<stdlib.h>',
            'environ': '<unistd.h>',
        }
        
        for func, include in needed_includes.items():
            if re.search(r'\b' + func + r'\s*\(', content) and include not in content:
                self.findings.append(AuditFinding(
                    layer="PLUMBER", severity="MEDIUM", category="MISSING_INCLUDE",
                    file=filepath, line=1,
                    detail=f"Uses '{func}' but missing {include}",
                    fix=f"#include {include}"
                ))
    
    def _check_dead_code(self, content, lines, filepath):
        """Check for unreachable code after return."""
        for i, line in enumerate(lines):
            stripped = line.strip()
            if stripped.startswith('return ') and not stripped.startswith('return;'):
                # Check next non-empty line
                for j in range(i+1, min(i+5, len(lines))):
                    next_stripped = lines[j].strip()
                    if next_stripped and not next_stripped.startswith('/*') and not next_stripped.startswith('*') and not next_stripped.startswith('}'):
                        if not next_stripped.startswith('//'):
                            self.findings.append(AuditFinding(
                                layer="PLUMBER", severity="LOW", category="DEAD_CODE",
                                file=filepath, line=j+1,
                                detail=f"Unreachable code after return at line {i+1}",
                                fix=f"Remove or restructure code"
                            ))
                        break
    
    def _check_inconsistent_signatures(self, content, lines, filepath):
        """Check for functions declared with one signature but defined with another."""
        # Find all function declarations (forward decls)
        decls = {}
        for m in re.finditer(r'(?:static\s+)?(?:inline\s+)?(?:const\s+)?(?:unsigned\s+)?(?:long\s+)?(\w[\w\s\*]*?)\s+(\w+)\s*\(([^)]*)\)\s*;', content):
            ret_type = m.group(1).strip()
            name = m.group(2).strip()
            params = m.group(3).strip()
            decls[name] = (ret_type, params)
        
        # Find all function definitions
        defs = {}
        for m in re.finditer(r'(?:static\s+)?(?:inline\s+)?(?:const\s+)?(?:unsigned\s+)?(?:long\s+)?(\w[\w\s\*]*?)\s+(\w+)\s*\(([^)]*)\)\s*\{', content):
            ret_type = m.group(1).strip()
            name = m.group(2).strip()
            params = m.group(3).strip()
            if name not in ('if', 'for', 'while', 'switch'):
                defs[name] = (ret_type, params)
        
        # Check for mismatches
        for name in set(decls.keys()) & set(defs.keys()):
            decl_ret, decl_params = decls[name]
            def_ret, def_params = defs[name]
            if decl_ret != def_ret:
                self.findings.append(AuditFinding(
                    layer="PLUMBER", severity="HIGH", category="SIG_MISMATCH",
                    file=filepath, line=1,
                    detail=f"Function '{name}': declared as {decl_ret} but defined as {def_ret}",
                    fix=f"Make declaration and definition match"
                ))
    
    def audit_all(self) -> list[AuditFinding]:
        """Run plumber audit on all C source files."""
        for root, dirs, files in os.walk(SLERMES_DIR):
            dirs[:] = [d for d in dirs if d not in ('node_modules', 'dist', 'build', '.git', 'lib')]
            for f in files:
                if f.endswith('.c'):
                    self.audit_c_file(os.path.join(root, f))
        return self.findings


# ═══════════════════════════════════════════════════════════════════════════
# LAYER 2: THE PAINTER — UX parity check
# ═══════════════════════════════════════════════════════════════════════════

class Painter:
    """Checks that the C apps provide the same user-facing features as TS apps."""
    
    def __init__(self):
        self.findings: list[AuditFinding] = []
    
    def audit_web_app(self) -> None:
        """Check web_app.c against web/src/ features."""
        web_app_path = os.path.join(SLERMES_DIR, "src/web_app.c")
        if not os.path.exists(web_app_path):
            self.findings.append(AuditFinding(
                layer="PAINTER", severity="CRITICAL", category="MISSING_FILE",
                file="src/web_app.c", line=0,
                detail="web_app.c does not exist",
                fix="Create web_app.c"
            ))
            return
        
        with open(web_app_path, 'r') as f:
            content = f.read()
        
        # Count API endpoints
        endpoints = re.findall(r'#define\s+API_\w+\s+"([^"]+)"', content)
        handler_funcs = re.findall(r'web_api_(\w+)\s*\(', content)
        
        # Check for all expected resource types
        expected_resources = [
            'sessions', 'models', 'status', 'settings', 'gateway',
            'profiles', 'cron', 'skills', 'plugins', 'hooks', 'webhooks',
            'mcp', 'files', 'checkpoints', 'toolsets', 'oauth', 'pairing',
            'messaging', 'env', 'config', 'memory', 'curator', 'portal',
            'backup', 'import', 'export', 'auth', 'telegram', 'marketplace',
            'updates', 'analytics', 'credentials', 'automation',
        ]
        
        for resource in expected_resources:
            found = any(resource in ep for ep in endpoints) or \
                    any(resource in hf for hf in handler_funcs)
            if not found:
                self.findings.append(AuditFinding(
                    layer="PAINTER", severity="HIGH", category="MISSING_RESOURCE",
                    file="src/web_app.c", line=0,
                    detail=f"Web app missing resource: {resource}",
                    fix=f"Add API endpoints and handlers for {resource}"
                ))
        
        # Check endpoint count
        if len(endpoints) < 20:
            self.findings.append(AuditFinding(
                layer="PAINTER", severity="MEDIUM", category="LOW_ENDPOINTS",
                file="src/web_app.c", line=0,
                detail=f"Web app only defines {len(endpoints)} API endpoints (TS app expects 30+)",
                fix="Add more API endpoint definitions"
            ))
    
    def audit_desktop_app(self) -> None:
        """Check desktop_app.c against apps/desktop/ features."""
        desktop_app_path = os.path.join(SLERMES_DIR, "src/desktop_app.c")
        if not os.path.exists(desktop_app_path):
            self.findings.append(AuditFinding(
                layer="PAINTER", severity="CRITICAL", category="MISSING_FILE",
                file="src/desktop_app.c", line=0,
                detail="desktop_app.c does not exist",
                fix="Create desktop_app.c"
            ))
            return
        
        with open(desktop_app_path, 'r') as f:
            content = f.read()
        
        # Check for key desktop features
        expected_features = [
            ('chat', 'Chat interface'),
            ('session', 'Session management'),
            ('model', 'Model picker'),
            ('settings', 'Settings'),
            ('command_palette', 'Command palette'),
            ('sidebar', 'Sidebar'),
            ('status', 'Status bar'),
            ('gateway', 'Gateway connection'),
            ('terminal', 'Terminal'),
            ('file', 'File browser'),
            ('cron', 'Cron management'),
            ('skills', 'Skills'),
            ('plugins', 'Plugins'),
            ('profiles', 'Profiles'),
            ('mcp', 'MCP management'),
        ]
        
        for feature, description in expected_features:
            if feature not in content.lower():
                self.findings.append(AuditFinding(
                    layer="PAINTER", severity="MEDIUM", category="MISSING_FEATURE",
                    file="src/desktop_app.c", line=0,
                    detail=f"Desktop app may be missing feature: {description}",
                    fix=f"Add {feature} functionality"
                ))
    
    def audit_all(self) -> list[AuditFinding]:
        self.audit_web_app()
        self.audit_desktop_app()
        return self.findings


# ═══════════════════════════════════════════════════════════════════════════
# LAYER 3: THE DEVIL'S ADVOCATE — "That sounds too good to be true"
# ═══════════════════════════════════════════════════════════════════════════

class DevilAdvocate:
    """Finds the lies — things that look good on the surface but are wrong."""
    
    def __init__(self):
        self.findings: list[AuditFinding] = []
    
    def audit_scanner_results(self) -> None:
        """Cross-check scanner results against actual code quality."""
        # Run the scanner
        try:
            result = subprocess.run(
                ['python3', 'slermes_parity_battleground.py', '--json'],
                capture_output=True, text=True, timeout=120,
                cwd=SLERMES_DIR
            )
            # Parse JSON from stdout (skip non-JSON lines)
            json_start = result.stdout.find('{')
            if json_start >= 0:
                data = json.loads(result.stdout[json_start:])
            else:
                return
        except Exception as e:
            return
        
        # Check for suspicious patterns
        total = data.get('total_features', 0)
        ported = data.get('ported', 0)
        stubs = data.get('stubs', 0)
        partials = data.get('partials', 0)
        
        # If 100% ported but there are still stubs, that's suspicious
        if ported == total and stubs > 0:
            self.findings.append(AuditFinding(
                layer="DEVIL", severity="CRITICAL", category="SCANNER_LIE",
                file="scanner", line=0,
                detail=f"Scanner claims 100% ported but reports {stubs} stubs",
                fix="Investigate scanner classification logic"
            ))
        
        # If 100% ported but there are partials
        if ported == total and partials > 0:
            self.findings.append(AuditFinding(
                layer="DEVIL", severity="HIGH", category="SCANNER_LIE",
                file="scanner", line=0,
                detail=f"Scanner claims 100% ported but reports {partials} partials",
                fix="Investigate PARTIAL classification"
            ))
    
    def audit_port_files(self) -> None:
        """Check port_*.c files for common lies."""
        port_files = []
        for root, dirs, files in os.walk(os.path.join(SLERMES_DIR, "src")):
            for f in files:
                if f.startswith('port_') and f.endswith('.c'):
                    port_files.append(os.path.join(root, f))
        
        for filepath in port_files:
            try:
                with open(filepath, 'r', errors='replace') as f:
                    content = f.read()
            except Exception:
                continue
            
            rel_path = filepath.replace(SLERMES_DIR + "/", "")
            lines = content.split('\n')
            
            # Check for "façade" functions — look real but do nothing
            func_pattern = r'(?:static\s+)?(?:inline\s+)?(\w[\w\s\*]*?)\s+(\w+)\s*\(([^)]*)\)\s*\{'
            for m in re.finditer(func_pattern, content):
                func_name = m.group(2)
                if func_name in ('if', 'for', 'while', 'switch'):
                    continue
                
                # Get function body
                start = m.end()
                depth = 1
                pos = start
                while depth > 0 and pos < len(content):
                    if content[pos] == '{': depth += 1
                    elif content[pos] == '}': depth -= 1
                    pos += 1
                body = content[start:pos]
                
                # Count meaningful lines (not comments, not empty, not just braces)
                meaningful = 0
                has_project_call = False
                has_real_logic = False
                
                stdlib = {'printf', 'fprintf', 'snprintf', 'sprintf', 'malloc', 'calloc',
                         'realloc', 'free', 'memset', 'memcpy', 'memmove', 'strlen',
                         'strcpy', 'strncpy', 'strcat', 'strncat', 'strcmp', 'strncmp',
                         'strstr', 'strchr', 'atoi', 'atol', 'strtol', 'time', 'NULL',
                         'sizeof', 'exit', 'abort', 'rand', 'srand'}
                
                for bline in body.split('\n'):
                    stripped = bline.strip()
                    if not stripped or stripped == '{' or stripped == '}' or \
                       stripped.startswith('/*') or stripped.startswith('*') or \
                       stripped.startswith('//') or stripped.startswith('Port of Python'):
                        continue
                    if stripped.startswith('hermes_log') or stripped.startswith('return') or \
                       stripped.startswith('if (!ctx)') or stripped.startswith('if (ctx') or \
                       stripped.startswith('json_free(NULL)'):
                        continue
                    meaningful += 1
                    
                    # Check for project function calls
                    for word in re.findall(r'(\w+)\s*\(', stripped):
                        if word not in stdlib and word != func_name:
                            has_project_call = True
                    
                    # Check for real logic (assignments, conditionals, loops)
                    if re.search(r'[=!<>]=|[+\-*/]=|&&|\|\||case |default:', stripped):
                        has_real_logic = True
                
                # A function with <= 1 meaningful line and no project calls is a stub
                # But only if it's not a simple accessor/wrapper (which are legitimately short)
                is_simple_accessor = (
                    meaningful <= 1 and 
                    not has_project_call and
                    not has_real_logic and
                    # Skip if it's a static inline helper
                    'static inline' not in content[m.start()-50:m.start()] and
                    # Skip if it's a simple return wrapper
                    (body.strip().count('\n') > 2)  # More than 2 lines = not a one-liner
                )
                
                if is_simple_accessor:
                    line_num = content[:m.start()].count('\n') + 1
                    self.findings.append(AuditFinding(
                        layer="DEVIL", severity="MEDIUM", category="FAÇADE",
                        file=rel_path, line=line_num,
                        detail=f"Function '{func_name}' appears to be a façade ({meaningful} meaningful lines)",
                        fix=f"Implement real logic or verify this is intentionally minimal"
                    ))
                
                # A function that only calls json_free(NULL) is also a façade
                if meaningful <= 2 and 'json_free(NULL)' in body and not has_real_logic:
                    line_num = content[:m.start()].count('\n') + 1
                    self.findings.append(AuditFinding(
                        layer="DEVIL", severity="MEDIUM", category="FAÇADE",
                        file=rel_path, line=line_num,
                        detail=f"Function '{func_name}' only calls json_free(NULL) — likely a façade",
                        fix=f"Add real implementation logic"
                    ))
    
    def audit_build(self) -> None:
        """Check if the build is actually clean."""
        try:
            result = subprocess.run(
                ['make', '-j4'],
                capture_output=True, text=True, timeout=300,
                cwd=SLERMES_DIR
            )
            
            # Check for errors
            errors = [l for l in result.stderr.split('\n') if 'error:' in l.lower()]
            warnings = [l for l in result.stderr.split('\n') if 'warning:' in l.lower()]
            
            if errors:
                self.findings.append(AuditFinding(
                    layer="DEVIL", severity="CRITICAL", category="BUILD_ERRORS",
                    file="Makefile", line=0,
                    detail=f"Build has {len(errors)} errors",
                    fix="Fix build errors"
                ))
            
            if len(warnings) > 50:
                self.findings.append(AuditFinding(
                    layer="DEVIL", severity="MEDIUM", category="BUILD_WARNINGS",
                    file="Makefile", line=0,
                    detail=f"Build has {len(warnings)} warnings",
                    fix="Clean up warnings"
                ))
        except subprocess.TimeoutExpired:
            self.findings.append(AuditFinding(
                layer="DEVIL", severity="HIGH", category="BUILD_TIMEOUT",
                file="Makefile", line=0,
                detail="Build timed out (5 minutes)",
                fix="Check for infinite loops or deadlocks in build"
            ))
        except Exception:
            pass
    
    def audit_all(self) -> list[AuditFinding]:
        self.audit_port_files()
        self.audit_build()
        return self.findings


# ═══════════════════════════════════════════════════════════════════════════
# MAIN
# ═══════════════════════════════════════════════════════════════════════════

def main():
    parser = argparse.ArgumentParser(description="Triple Devil's Advocate Audit")
    parser.add_argument('--json', action='store_true', help='Output as JSON')
    parser.add_argument('--layer', choices=['PLUMBER', 'PAINTER', 'DEVIL', 'ALL'],
                       default='ALL', help='Run specific layer only')
    parser.add_argument('--severity', choices=['CRITICAL', 'HIGH', 'MEDIUM', 'LOW', 'ALL'],
                       default='ALL', help='Filter by severity')
    args = parser.parse_args()
    
    all_findings = []
    
    if args.layer in ('PLUMBER', 'ALL'):
        plumber = Plumber()
        all_findings.extend(plumber.audit_all())
    
    if args.layer in ('PAINTER', 'ALL'):
        painter = Painter()
        all_findings.extend(painter.audit_all())
    
    if args.layer in ('DEVIL', 'ALL'):
        devil = DevilAdvocate()
        all_findings.extend(devil.audit_all())
    
    # Filter by severity
    if args.severity != 'ALL':
        severity_order = {'CRITICAL': 0, 'HIGH': 1, 'MEDIUM': 2, 'LOW': 3}
        min_sev = severity_order[args.severity]
        all_findings = [f for f in all_findings if severity_order.get(f.severity, 99) <= min_sev]
    
    # Output
    if args.json:
        output = {
            'summary': {
                'total_findings': len(all_findings),
                'critical': len([f for f in all_findings if f.severity == 'CRITICAL']),
                'high': len([f for f in all_findings if f.severity == 'HIGH']),
                'medium': len([f for f in all_findings if f.severity == 'MEDIUM']),
                'low': len([f for f in all_findings if f.severity == 'LOW']),
                'plumber': len([f for f in all_findings if f.layer == 'PLUMBER']),
                'painter': len([f for f in all_findings if f.layer == 'PAINTER']),
                'devil': len([f for f in all_findings if f.layer == 'DEVIL']),
            },
            'findings': [asdict(f) for f in all_findings],
        }
        print(json.dumps(output, indent=2))
    else:
        print("🔧🎭😈 TRIPLE DEVIL'S ADVOCATE AUDIT")
        print("=" * 70)
        
        for layer in ['PLUMBER', 'PAINTER', 'DEVIL']:
            layer_findings = [f for f in all_findings if f.layer == layer]
            if not layer_findings:
                continue
            
            icon = {'PLUMBER': '🔧', 'PAINTER': '🎭', 'DEVIL': '😈'}[layer]
            name = {'PLUMBER': 'THE PLUMBER (Function Wiring)', 
                    'PAINTER': 'THE PAINTER (UX Parity)',
                    'DEVIL': "THE DEVIL'S ADVOCATE (Find the Lies)"}[layer]
            
            print(f"\n{icon} {name} — {len(layer_findings)} findings")
            print("-" * 70)
            
            for f in sorted(layer_findings, key=lambda x: {'CRITICAL': 0, 'HIGH': 1, 'MEDIUM': 2, 'LOW': 3}.get(x.severity, 99)):
                sev_icon = {'CRITICAL': '💀', 'HIGH': '🔴', 'MEDIUM': '🟡', 'LOW': '🟢'}.get(f.severity, '⚪')
                print(f"  {sev_icon} [{f.severity}] {f.category}")
                print(f"     File: {f.file}:{f.line}")
                print(f"     {f.detail}")
                print(f"     Fix: {f.fix}")
                print()
        
        # Summary
        print("=" * 70)
        print("SUMMARY")
        print(f"  Total findings: {len(all_findings)}")
        print(f"  💀 CRITICAL: {len([f for f in all_findings if f.severity == 'CRITICAL'])}")
        print(f"  🔴 HIGH:     {len([f for f in all_findings if f.severity == 'HIGH'])}")
        print(f"  🟡 MEDIUM:   {len([f for f in all_findings if f.severity == 'MEDIUM'])}")
        print(f"  🟢 LOW:      {len([f for f in all_findings if f.severity == 'LOW'])}")
        print()
        print(f"  🔧 Plumber: {len([f for f in all_findings if f.layer == 'PLUMBER'])}")
        print(f"  🎭 Painter: {len([f for f in all_findings if f.layer == 'PAINTER'])}")
        print(f"  😈 Devil:   {len([f for f in all_findings if f.layer == 'DEVIL'])}")
    
    return 0 if not all_findings else 1


if __name__ == '__main__':
    sys.exit(main())
