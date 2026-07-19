/*
 * test_cua_backend_helpers.c — unit tests for the pure cua_backend.py helpers.
 * Expected values derived from a faithful Python oracle of
 * _image_dimensions_from_bytes / _split_tree_text / _parse_key_combo.
 */

#include "cua_backend_helpers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail = 0;

static void check_dims(const unsigned char *raw, size_t len, int ew, int eh)
{
    int w = -1, h = -1;
    int rc = cua_image_dimensions_from_bytes(raw, len, &w, &h);
    if (rc != (ew > 0 ? 1 : 0) || w != ew || h != eh) {
        printf("FAIL: dims (rc=%d w=%d h=%d) expected (rc=%d w=%d h=%d)\n",
               rc, w, h, (ew > 0 ? 1 : 0), ew, eh);
        g_fail++;
    } else {
        printf("ok: dims -> %dx%d\n", w, h);
    }
}

static void check_split(const char *text, const char *es, const char *et)
{
    char *s = NULL, *t = NULL;
    cua_split_tree_text(text, &s, &t);
    if (strcmp(s ? s : "", es) != 0 || strcmp(t ? t : "", et) != 0) {
        printf("FAIL: split\n  summary=[%s] want [%s]\n  tree  =[%s] want [%s]\n",
               s ? s : "(null)", es, t ? t : "(null)", et);
        g_fail++;
    } else {
        printf("ok: split -> [%s] / [%s]\n", s, t);
    }
    free(s); free(t);
}

static void check_key(const char *keys, const char *ek, const char **emods, int nmods)
{
    char **mods = NULL; int nm = 0;
    char *k = cua_parse_key_combo(keys, &mods, &nm);
    int ok = 1;
    if ((k ? k : "") == NULL) {}
    if ((ek == NULL) != (k == NULL) || (k && strcmp(k, ek) != 0)) ok = 0;
    if (nm != nmods) ok = 0;
    else for (int i = 0; i < nm; i++)
        if (strcmp(mods[i], emods[i]) != 0) ok = 0;
    if (!ok) {
        printf("FAIL: key(%s) -> key=[%s] mods=%d\n", keys, k ? k : "NULL", nm);
        g_fail++;
    } else {
        printf("ok: key(%s) -> [%s] + %d mods\n", keys, k ? k : "NULL", nm);
    }
    free(k);
    cua_free_modifiers(mods, nm);
}

int main(void)
{
    /* PNG 640x480 */
    unsigned char png[] = {
        0x89,'P','N','G','\r','\n',0x1a,'\n',
        'I','H','D','R', 0,0,2,0x80, 0,0,1,0xE0, 0x08,0x06,0,0,0
    };
    check_dims(png, sizeof(png), 640, 480);

    /* JPEG 800x600 (SOF0 fixture, faithful-python parses to 800x600) */
    unsigned char jpg[] = {
        0xFF,0xD8, 0xFF,0xC0,0x00,0x11,0x08, 0x02,0x58,0x03,0x20,
        0x03,0x01,0x00,0x00,0x00,
        0xFF,0xDB,0x00,0x04,0x00,0x00, 0xFF,0xD9
    };
    check_dims(jpg, sizeof(jpg), 800, 600);

    /* unrecognized bytes -> 0,0 */
    unsigned char junk[] = { 0x00,0x01,0x02,0x03 };
    check_dims(junk, sizeof(junk), 0, 0);

    check_split("SUMMARY LINE\nmarkdown tree\nmore", "SUMMARY LINE", "markdown tree\nmore");
    check_split("only-summary", "only-summary", "");

    const char *m1[] = {"cmd", "shift"};
    check_key("cmd+shift+s", "s", m1, 2);
    const char *m2[] = {"ctrl", "option"};
    check_key("Control-Alt-Delete", "delete", m2, 2);
    check_key("a", "a", NULL, 0);
    check_key("  CMD + '  ", "'", (const char *[]){"cmd"}, 1);

    if (g_fail) { printf("\n%d FAIL\n", g_fail); return 1; }
    printf("\nALL PASSED\n");
    return 0;
}
