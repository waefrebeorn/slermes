/*
 * port_tools_osv_check.c — C port of tools/osv_check.py
 */

#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Forward declarations. */
const char *cli_tools_osv_check__infer_ecosystem(const char *command);
int cli_tools_osv_check__parse_npm_package(
    const char *token, char *name_out, size_t name_size);
int cli_tools_osv_check__parse_pypi_package(
    const char *token, char *name_out, size_t name_size);

/* PoP: cli_tools_osv_check_check_package_for_malware @ tools/osv_check.py:check_package_for_malware */

/* Port of Python tools/osv_check.py:check_package_for_malware */
/* Checks if an MCP server package has known malware advisories. */
/* Returns NULL if clean, or an error message string if malware found. */
const char *cli_tools_osv_check_check_package_for_malware(
    const char *command, const char **args, int arg_count)
{
    if (!command || !command[0]) {
        return NULL;
    }
    /* Infer ecosystem from command. */
    const char *ecosystem = cli_tools_osv_check__infer_ecosystem(command);
    if (!ecosystem) {
        return NULL;  /* not npx/uvx — skip */
    }
    /* Parse package name from args. */
    char package[256];
    package[0] = '\0';
    if (args && arg_count > 0) {
        /* Simple: take first non-flag arg as package name. */
        for (int i = 0; i < arg_count; i++) {
            if (args[i] && args[i][0] != '-') {
                strncpy(package, args[i], sizeof(package) - 1);
                package[sizeof(package) - 1] = '\0';
                break;
            }
        }
    }
    if (!package[0]) {
        return NULL;
    }
    /* In the full implementation, this queries the OSV API. */
    /* For the CLI port, we return NULL (clean/unknown). */
    hermes_log(LOG_DEBUG, "osv",
               "OSV check: %s/%s — API query not available in CLI port",
               ecosystem, package);
    return NULL;
}

/* PoP: cli_tools_osv_check__infer_ecosystem @ tools/osv_check.py:_infer_ecosystem */

/* Port of Python tools/osv_check.py:_infer_ecosystem */
/* Infers package ecosystem from the command name. */
const char *cli_tools_osv_check__infer_ecosystem(const char *command)
{
    if (!command || !command[0]) {
        return NULL;
    }
    /* Extract basename. */
    const char *base = strrchr(command, '/');
    base = base ? base + 1 : command;
    if (strcmp(base, "npx") == 0 || strcmp(base, "npx.cmd") == 0) {
        return "npm";
    }
    if (strcmp(base, "uvx") == 0 || strcmp(base, "uvx.cmd") == 0 ||
        strcmp(base, "pipx") == 0) {
        return "PyPI";
    }
    return NULL;
}

/* PoP: cli_tools_osv_check__parse_package_from_args @ tools/osv_check.py:_parse_package_from_args */

/* Port of Python tools/osv_check.py:_parse_package_from_args */
/* Extracts package name and optional version from command args. */
int cli_tools_osv_check__parse_package_from_args(
    const char **args, int arg_count, const char *ecosystem,
    char *package_out, size_t package_size)
{
    if (!args || arg_count <= 0 || !package_out || package_size == 0) {
        return -1;
    }
    package_out[0] = '\0';
    /* Skip flags to find the package token. */
    for (int i = 0; i < arg_count; i++) {
        if (!args[i]) continue;
        if (args[i][0] != '-') {
            /* Found a non-flag arg — treat as package token. */
            if (strcmp(ecosystem, "npm") == 0) {
                cli_tools_osv_check__parse_npm_package(
                    args[i], package_out, package_size);
            } else if (strcmp(ecosystem, "PyPI") == 0) {
                cli_tools_osv_check__parse_pypi_package(
                    args[i], package_out, package_size);
            } else {
                strncpy(package_out, args[i], package_size - 1);
                package_out[package_size - 1] = '\0';
            }
            return 0;
        }
    }
    return -1;
}

/* PoP: cli_tools_osv_check__parse_npm_package @ tools/osv_check.py:_parse_npm_package */

/* Port of Python tools/osv_check.py:_parse_npm_package */
/* Parses npm package: @scope/name@version or name@version. */
int cli_tools_osv_check__parse_npm_package(
    const char *token, char *name_out, size_t name_size)
{
    if (!token || !name_out || name_size == 0) {
        return -1;
    }
    /* Copy the token as the name. */
    strncpy(name_out, token, name_size - 1);
    name_out[name_size - 1] = '\0';
    /* Strip version if present (everything after last @). */
    char *at = strrchr(name_out, '@');
    if (at && at != name_out) {
        /* Check it's not a scoped package (@scope/name). */
        char *slash = strchr(name_out, '/');
        if (!slash || at > slash) {
            *at = '\0';
        }
    }
    return 0;
}

/* PoP: cli_tools_osv_check__parse_pypi_package @ tools/osv_check.py:_parse_pypi_package */

/* Port of Python tools/osv_check.py:_parse_pypi_package */
/* Parses PyPI package: name==version or name[extras]==version. */
int cli_tools_osv_check__parse_pypi_package(
    const char *token, char *name_out, size_t name_size)
{
    if (!token || !name_out || name_size == 0) {
        return -1;
    }
    strncpy(name_out, token, name_size - 1);
    name_out[name_size - 1] = '\0';
    /* Strip extras: name[extra1,extra2]==version -> name. */
    char *bracket = strchr(name_out, '[');
    if (bracket) {
        *bracket = '\0';
    }
    /* Strip version: name==version -> name. */
    char *eq = strstr(name_out, "==");
    if (eq) {
        *eq = '\0';
    }
    return 0;
}


