/*
 * recovery.c — C home for upstream Hermes pm/recovery.py (gate w5_recovery_15f063).
 *
 * PoP: w5_recovery_15f063_startup_script @ pm/recovery.py:validate_environment
 * PoP: w5_recovery_15f063_normalize_dependency @ pm/recovery.py:validate_environment
 *
 * The deterministic pieces of dependency-recovery validation: the exact script
 * handed to the candidate interpreter, and the requirement-name normalisation
 * that script applies. Subprocess/filesystem behaviour is out of scope for a
 * differential oracle.
 */
#include "noc_w5_recovery_15f063.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

char *w5_recovery_15f063_startup_script(void) {
    static const char SCRIPT[] = "import importlib, importlib.metadata, pathlib, re, tomllib\nproject = tomllib.loads(pathlib.Path('pyproject.toml').read_text(encoding='utf-8-sig'))['project']\nrequired = {re.split(r'[\\[<>=!~; @]', dep, 1)[0].lower().replace('_', '-')\n            for dep in project.get('dependencies', [])}\nchecks = (('ruamel.yaml', 'ruamel.yaml', 'YAML'), ('python-dotenv', 'dotenv', 'load_dotenv'), ('click', 'click', 'Command'), ('certifi', 'certifi', 'contents'), ('rich', 'rich', 'print'), ('cryptography', 'cryptography.hazmat.bindings._rust', 'openssl'), ('PyJWT', 'jwt', 'encode'))\nfor distribution, module, attribute in checks:\n    try:\n        importlib.metadata.distribution(distribution)\n    except importlib.metadata.PackageNotFoundError:\n        if distribution.lower().replace('_', '-') in required:\n            raise\n        continue\n    loaded = importlib.import_module(module)\n    getattr(loaded, attribute)\n    if module == 'certifi':\n        bundle = pathlib.Path(loaded.where())\n        assert bundle.is_file() and bundle.stat().st_size >= 1024, 'CA bundle is missing'\n";
    size_t n = sizeof(SCRIPT); /* includes NUL */
    char *out = (char *)malloc(n);
    if (!out) return NULL;
    memcpy(out, SCRIPT, n);
    return out;
}

char *w5_recovery_15f063_normalize_dependency(const char *dep) {
    if (!dep) dep = "";
    /* re.split(r'[\\[<>=!~; @]', dep, 1)[0] -- first separator ends the name. */
    static const char SEP[] = "\\[<>=!~; @";
    size_t n = strcspn(dep, SEP);
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    for (size_t i = 0; i < n; i++) {
        char c = dep[i];
        if (c == '_') c = '-';
        out[i] = (char)tolower((unsigned char)c);
    }
    out[n] = '\0';
    return out;
}

