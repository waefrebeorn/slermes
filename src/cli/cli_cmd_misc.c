/*
 * cli_cmd_misc.c — Misc slash-command handlers extracted from commands.c.
 * Self-contained command-category module.
 */

#include "libplugin/plugin.h"
#include "cli_cmd_misc.h"
#include "commands_shared.h"
#include "hermes_core_types.h"

/* /plugins: List installed plugins and their status */
void cmd_plugins(const char *args, agent_state_t *state) {
    (void)state;
    /* Resolve plugins directory */
    char plugins_dir[HERMES_PATH_MAX];
    char home[HERMES_PATH_MAX];
    hermes_get_home(home, sizeof(home));
    snprintf(plugins_dir, sizeof(plugins_dir), "%s/plugins", home);

    /* Parse subcommand */
    char subcmd[64] = "", subarg[256] = "";
    if (args && args[0]) {
        if (sscanf(args, "%63s %255[^\n]", subcmd, subarg) < 1)
            snprintf(subcmd, sizeof(subcmd), "%s", args);
    }

    /* Default: list */
    if (!subcmd[0] || strcmp(subcmd, "list") == 0) {
        printf("Plugin system status:\n");
        printf("  Directory: %s\n", plugins_dir);

        DIR *d = opendir(plugins_dir);
        if (!d) {
            printf("  Error: cannot open plugins directory\n");
            return;
        }

        int count = 0;
        struct dirent *entry;
        while ((entry = readdir(d)) != NULL) {
            if (entry->d_type != DT_REG && entry->d_type != DT_LNK) continue;
            const char *dot = strrchr(entry->d_name, '.');
            if (!dot || strcmp(dot, ".so") != 0) continue;

            char full_path[HERMES_PATH_MAX];
            snprintf(full_path, sizeof(full_path), "%s/%s", plugins_dir, entry->d_name);

            plugin_t *p = plugin_load(full_path);
            if (p) {
                const plugin_version_t *ver = plugin_version(p);
                char ver_buf[32];
                plugin_version_str(ver, ver_buf, sizeof(ver_buf));
                printf("  %s v%s (%s) — %s\n",
                       plugin_name(p), ver_buf,
                       plugin_type_str(plugin_type(p)),
                       plugin_description(p) ? plugin_description(p) : "no description");
                plugin_unload(p);
            } else {
                printf("  %s — (unloadable: %s)\n", entry->d_name, plugin_error());
            }
            count++;
        }
        closedir(d);
        printf("  Total plugins: %d\n", count);
        return;
    }

    /* show <name>: Show detailed info about a specific plugin */
    if (strcmp(subcmd, "show") == 0) {
        if (!subarg[0]) {
            printf("Usage: /plugins show <plugin_name>\n");
            return;
        }
        char full_path[HERMES_PATH_MAX];
        snprintf(full_path, sizeof(full_path), "%s/%s.so", plugins_dir, subarg);

        plugin_t *p = plugin_load(full_path);
        if (!p) {
            /* Try without .so suffix */
            snprintf(full_path, sizeof(full_path), "%s/%s", plugins_dir, subarg);
            p = plugin_load(full_path);
        }
        if (!p) {
            printf("Plugin '%s' not found or unloadable: %s\n", subarg, plugin_error());
            return;
        }

        const plugin_version_t *ver = plugin_version(p);
        char ver_buf[32];
        plugin_version_str(ver, ver_buf, sizeof(ver_buf));
        printf("Name:        %s\n", plugin_name(p));
        printf("Version:     %s\n", ver_buf);
        printf("Type:        %s\n", plugin_type_str(plugin_type(p)));
        printf("Path:        %s\n", full_path);
        printf("Description: %s\n", plugin_description(p) ? plugin_description(p) : "(none)");
        /* Dependencies */
        int dep_count = 0;
        const plugin_dep_t *deps = plugin_deps(p, &dep_count);
        if (deps && dep_count > 0) {
            printf("Deps:        ");
            for (int di = 0; di < dep_count && di < PLUGIN_MAX_DEPS; di++) {
                if (di > 0) printf(", ");
                printf("%s", deps[di].name);
                if (deps[di].optional) printf(" (optional)");
            }
            printf("\n");
        } else {
            printf("Deps:        (none)\n");
        }
        plugin_unload(p);
        return;
    }

    /* install <path>: Copy a plugin .so into the plugins directory */
    if (strcmp(subcmd, "install") == 0) {
        if (!subarg[0]) {
            printf("Usage: /plugins install <source_path>\n");
            return;
        }

        /* Resolve destination filename from source path */
        const char *src_name = strrchr(subarg, '/');
        src_name = src_name ? src_name + 1 : subarg;

        /* Ensure .so extension */
        const char *dot = strrchr(src_name, '.');
        if (!dot || strcmp(dot, ".so") != 0) {
            printf("Error: plugin file must have .so extension\n");
            return;
        }

        /* Build destination path */
        char dest_path[HERMES_PATH_MAX];
        snprintf(dest_path, sizeof(dest_path), "%s/%s", plugins_dir, src_name);

        /* Check if plugin already exists */
        struct stat st;
        if (stat(dest_path, &st) == 0) {
            printf("Error: plugin '%s' already exists at %s\n", src_name, dest_path);
            printf("  Use /plugins remove %s first to replace it\n", src_name);
            return;
        }

        /* Open source file */
        FILE *src_fp = fopen(subarg, "rb");
        if (!src_fp) {
            printf("Error: cannot open source '%s': %s\n", subarg, strerror(errno));
            return;
        }

        /* Create destination file */
        FILE *dst_fp = fopen(dest_path, "wb");
        if (!dst_fp) {
            printf("Error: cannot create '%s': %s\n", dest_path, strerror(errno));
            fclose(src_fp);
            return;
        }

        /* Copy contents */
        char copy_buf[8192];
        size_t nread;
        size_t total = 0;
        while ((nread = fread(copy_buf, 1, sizeof(copy_buf), src_fp)) > 0) {
            if (fwrite(copy_buf, 1, nread, dst_fp) != nread) {
                printf("Error: write error during copy\n");
                fclose(src_fp);
                fclose(dst_fp);
                unlink(dest_path);
                return;
            }
            total += nread;
        }
        fclose(src_fp);
        fclose(dst_fp);

        /* Verify: try to load the plugin */
        plugin_t *p = plugin_load(dest_path);
        if (p) {
            printf("Plugin '%s' installed successfully (%zu bytes)\n", src_name, total);
            printf("  Name:    %s\n", plugin_name(p));
            printf("  Version: %s\n", plugin_version_str(plugin_version(p),
                   (char[32]){0}, 32));
            printf("  Type:    %s\n", plugin_type_str(plugin_type(p)));
            plugin_unload(p);
        } else {
            printf("Warning: plugin '%s' copied but failed to load: %s\n",
                   src_name, plugin_error());
            printf("  The file is at %s\n", dest_path);
        }
        return;
    }

    /* remove <name>: Delete a plugin from the plugins directory */
    if (strcmp(subcmd, "remove") == 0) {
        if (!subarg[0]) {
            printf("Usage: /plugins remove <plugin_name>\n");
            return;
        }

        char full_path[HERMES_PATH_MAX];
        snprintf(full_path, sizeof(full_path), "%s/%s.so", plugins_dir, subarg);

        struct stat st;
        if (stat(full_path, &st) != 0) {
            /* Try without .so suffix */
            snprintf(full_path, sizeof(full_path), "%s/%s", plugins_dir, subarg);
            if (stat(full_path, &st) != 0) {
                printf("Error: plugin '%s' not found in %s\n", subarg, plugins_dir);
                return;
            }
        }

        if (unlink(full_path) != 0) {
            printf("Error: cannot remove '%s': %s\n", full_path, strerror(errno));
            return;
        }

        printf("Plugin '%s' removed successfully\n", subarg);
        return;
    }

    /* Unknown subcommand */
    printf("Usage: /plugins [list|show <name>|install <path>|remove <name>]\n");
    printf("  list                List all installed plugins\n");
    printf("  show <name>         Show detailed information about a plugin\n");
    printf("  install <path>      Install a plugin .so file\n");
    printf("  remove <name>       Remove an installed plugin\n");
}

