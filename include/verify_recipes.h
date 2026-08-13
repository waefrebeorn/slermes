#ifndef HERMES_VERIFY_RECIPES_H
#define HERMES_VERIFY_RECIPES_H

/* Faithful C11 port of agent/verify/recipes.py.
 *
 * Recipe mirrors grok-cli's VerifyRecipe: name/kind + command lists and the
 * start/port/readiness metadata that lets `hermes verify` boot the app and
 * prove it serves HTTP. */

#include <stddef.h>

typedef struct recipe_s {
    char *name;         /* human label — "appLabel" in grok */
    char *kind;         /* detector id — "appKind", default "unknown" */
    char **bootstrap;   /* install commands */
    size_t n_bootstrap;
    char **build;       /* build commands */
    size_t n_build;
    char **test;        /* test commands */
    size_t n_test;
    char *start;        /* NULL when none */
    int port;           /* 0 when unknown — NULL in grok is modeled as 0 */
    char *readiness_path;
    char **evidence;    /* human strings */
    size_t n_evidence;
} recipe_t;

/* Free a recipe and all its owned strings. */
void recipe_free(recipe_t *r);

/* Parse a package.json into a JSON object, or NULL if absent/malformed.
 * Mirrors _read_package_json: tolerates OSError and JSONDecodeError. */
char *recipe_read_package_json(const char *root);

/* Lockfile-based package-manager detection. Returns "pnpm"|"bun"|"yarn"
 * |"npm"|"uv"|"poetry"|"pipenv", or NULL. Mirrors detect_package_manager. */
char *recipe_detect_package_manager(const char *root);

/* Infer a port (1..65535) from a start command's --port/-p or PORT= env,
 * else NULL. Mirrors _infer_port_from_command. */
int recipe_infer_port_from_command(const char *command);

/* Node recipe detection — always returns a recipe (kind=node). */
recipe_t *recipe_detect_node(const char *root, const char *pkg_json);
/* Python recipe detection — NULL if not a Python project */
recipe_t *recipe_detect_python(const char *root);
/* Go recipe detection — NULL if no go.mod */
recipe_t *recipe_detect_go(const char *root);
/* Rust recipe detection — NULL if no Cargo.toml */
recipe_t *recipe_detect_rust(const char *root);
/* Java recipe detection — NULL if no Maven/Gradle marker */
recipe_t *recipe_detect_java(const char *root);
/* Makefile recipe detection — NULL if no Makefile */
recipe_t *recipe_detect_make(const char *root);
/* docker-compose recipe detection — NULL if no compose file */
recipe_t *recipe_detect_compose(const char *root);

/* Entry point: detect a recipe for `root`, or return NULL. Mirrors
 * detect_recipe — package.json wins, then Python/Go/Rust/Java/Make/compose. */
recipe_t *recipe_detect(const char *root);

/* Tolerant loader of a Recipe from a JSON dict string (Recipe.from_dict).
 * Returns NULL if malformed/empty. Caller frees with recipe_free. */
recipe_t *recipe_from_dict(const char *raw);

/* Serialize a recipe to JSON (Recipe.to_dict equivalent). Caller frees. */
char *recipe_to_json(const recipe_t *r);

#endif /* HERMES_VERIFY_RECIPES_H */
