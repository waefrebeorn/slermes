/* gate: pet_atlas_repair_internal_alpha_holes — drives the REAL
 * src/pet/atlas.c::pet_repair_internal_alpha_holes (linked via extra_src).
 * LIVE twin: agent/pet/generate/atlas.py::_repair_internal_alpha_holes.
 *
 * Input per cup: "<w> <h> <r,g,b,a>x<w*h>" row-major.
 * Rungs: B1 null/empty image guard, B2 any border seed found, B3 at least
 * one enclosed hole filled. V = resulting pixel dump "r.g.b.a|...".
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include "pet_atlas.h"

void pet_repair_internal_alpha_holes(pet_img_t *img);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s repair \"w h p,p,...\"...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("repair_internal_alpha_holes");
        int w = 0, h = 0;
        const char *s = argv[i];
        /* parse: "<w> <h> r,g,b,a r,g,b,a ..." */
        int nread = 0;
        if (s) nread = sscanf(s, "%d %d", &w, &h);
        int ok = nread == 2 && w > 0 && h > 0 && w * h <= 4096;
        WCUP_BRANCH(ok);                                   /* B1 image usable */
        int filled_any = 0;
        if (ok) {
            pet_img_t img;
            img.w = w; img.h = h;
            img.px = (rgba_t *)malloc((size_t)w * h * sizeof(rgba_t));
            memset(img.px, 0, (size_t)w * h * sizeof(rgba_t));
            const char *p = s;
            while (*p && *p != ' ') p++;   /* skip w */
            while (*p && *p == ' ') p++;
            while (*p && *p != ' ') p++;   /* skip h */
            for (int k = 0; k < w * h; k++) {
                int r, g, b, a;
                while (*p == ' ') p++;
                if (sscanf(p, "%d,%d,%d,%d", &r, &g, &b, &a) != 4) break;
                img.px[k].r = (uint8_t)r; img.px[k].g = (uint8_t)g;
                img.px[k].b = (uint8_t)b; img.px[k].a = (uint8_t)a;
                while (*p && *p != ' ') p++;
            }
            pet_repair_internal_alpha_holes(&img);
            for (int k = 0; k < w * h; k++)
                if (img.px[k].a == 255 &&
                    !(img.px[k].r | img.px[k].g | img.px[k].b))
                    { }
            /* B2: any pixel changed to opaque-fill from transparent */
            /* recompute cheaply: compare alpha channel against input alphas */
            /* (inputs keep alphas; we detect fills as a==255 on was<=16 px) */
            {
                /* rebuild input alphas again */
                const char *q = s;
                while (*q && *q != ' ') q++;
                while (*q && *q == ' ') q++;
                while (*q && *q != ' ') q++;
                for (int k = 0; k < w * h; k++) {
                    int r, g, b, a;
                    while (*q == ' ') q++;
                    if (sscanf(q, "%d,%d,%d,%d", &r, &g, &b, &a) != 4) break;
                    if (a <= 16 && img.px[k].a > 16) filled_any = 1;
                    while (*q && *q != ' ') q++;
                }
            }
            WCUP_BRANCH(filled_any);                       /* B2 holes filled */
            char valbuf[4096]; int pos = 0;
            for (int k = 0; k < w * h && pos < 4000; k++)
                pos += snprintf(valbuf + pos, sizeof valbuf - (size_t)pos,
                                "%d.%d.%d.%d|",
                                img.px[k].r, img.px[k].g, img.px[k].b, img.px[k].a);
            WCUP_VALSTR(valbuf);                           /* V */
            free(img.px);
        } else {
            WCUP_BRANCH(0);                                /* B2 */
            WCUP_VALSTR("");                               /* V */
        }
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("repair_internal_alpha_holes");
    }
    return 0;
}
