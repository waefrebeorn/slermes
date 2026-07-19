/*
 * test_atlas.c — Faithful port of agent/pet/generate/atlas.py pixel ops.
 * Asserts deterministic invariants of the C11 atlas module.
 */

#include "pet_atlas.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

static int g_fail = 0;
#define TEST(cond, msg) do { if (!(cond)) { printf("  FAIL: %s\n", msg); g_fail++; } } while (0)

static pet_img_t *make(int w, int h) { return pet_img_new(w, h); }

int main(void) {
    /* pet_color_distance */
    TEST(fabsf(pet_color_distance(0,0,0, 10,0,0) - 10.0f) < 1e-3f, "color_distance axis");
    TEST(fabs(pet_color_distance(255,255,255, 0,0,0) - sqrt(3*255.0*255.0)) < 1e-2, "color_distance diag");

    /* pet_has_transparency: fully opaque -> false; 10% transparent -> true */
    {
        pet_img_t *a = make(10,10);
        for (int i=0;i<100;i++) a->px[i].a=255;
        TEST(!pet_has_transparency(a), "opaque not transparent");
        pet_img_t *b = make(10,10);
        for (int i=0;i<100;i++){ b->px[i].a=255; if(i%10==0) b->px[i].a=0; } /* 10 transparent = 10% */
        TEST(pet_has_transparency(b), "10% transparent detected");
        pet_img_free(a); pet_img_free(b);
    }

    /* pet_dominant_corner_color: corners opaque green -> (0,255,0) */
    {
        pet_img_t *a = make(4,4);
        /* set first three corners to green, last to red */
        a->px[0]=(rgba_t){0,255,0,255};
        a->px[3]=(rgba_t){0,255,0,255};
        a->px[12]=(rgba_t){0,255,0,255};
        a->px[15]=(rgba_t){255,0,0,255};
        int r,g,bl; pet_dominant_corner_color(a,&r,&g,&bl);
        TEST(r==0 && g==255 && bl==0, "dominant corner = green majority");
        pet_img_free(a);
    }

    /* pet_near_key_mask: pure key pixels -> 255, far -> 0 */
    {
        pet_img_t *a = make(2,1);
        a->px[0]=(rgba_t){255,0,255,255}; /* magenta key */
        a->px[1]=(rgba_t){0,0,0,255};
        pet_img_t *m = pet_near_key_mask(a,255,0,255,48);
        TEST(m->px[0].a==255 && m->px[1].a==0, "near_key_mask magenta");
        pet_img_free(a); pet_img_free(m);
    }

    /* pet_defringe: 3x3 min filter on alpha. Solid 255 interior stays 255;
       a single interior 255 surrounded by 0 -> becomes 0. */
    {
        pet_img_t *a = make(3,3);
        for (int i=0;i<9;i++) a->px[i].a=0;
        a->px[4].a=255; /* lone center */
        pet_defringe(a);
        TEST(a->px[4].a==0, "defringe clears lone alpha pixel");
        pet_img_free(a);
    }

    /* pet_column_profile + content_runs */
    {
        pet_img_t *a = make(6,2);
        /* columns 1,2,3 have alpha; 0,4,5 empty -> one run (1,4) */
        for (int y=0;y<2;y++) for (int x=0;x<6;x++) a->px[y*6+x].a = (x>=1 && x<=3)?200:0;
        int *prof = pet_column_profile(a);
        TEST(prof[0]==0 && prof[1]==200 && prof[4]==0, "column_profile");
        int n; int *runs = pet_content_runs(prof, 6, &n, 2);
        TEST(n==1 && runs[0]==1 && runs[1]==4, "content_runs single span");
        free(prof); free(runs); pet_img_free(a);
    }

    /* pet_component_boxes: two separated opaque blobs -> 2 components */
    {
        pet_img_t *a = make(5,1);
        a->px[0].a=255; a->px[1].a=255;   /* blob A */
        /* gap at 2,3 */
        a->px[4].a=255;                    /* blob B */
        int n; pet_component_t *c = pet_component_boxes(a,&n);
        TEST(c && n==2, "two separated components");
        free(c); pet_img_free(a);
    }

    /* pet_best_shift: identical profiles -> 0; shifted right by 2 -> +2 */
    {
        int ref[8]={0,0,5,5,5,0,0,0};
        int prof[8]={0,0,5,5,5,0,0,0};
        TEST(pet_best_shift(ref,8,prof,8,4)==0, "best_shift identical=0");
        int prof2[8]={0,0,0,0,5,5,5,0}; /* ref's 5s at 2-4 moved to 4-6 = right by 2 */
        TEST(pet_best_shift(ref,8,prof2,8,4)==-2, "best_shift -2 (right-shifted profile)");
    }

    /* pet_remove_background: border-connected backdrop keyed out, interior pet kept.
       Build a 5x5 image: magenta border, opaque red 3x3 interior. */
    {
        pet_img_t *a = make(5,5);
        for (int y=0;y<5;y++) for (int x=0;x<5;x++)
            a->px[y*5+x] = (rgba_t){255,0,255,255}; /* all magenta */
        /* carve an interior red square (not connected to border) */
        for (int y=1;y<=3;y++) for (int x=1;x<=3;x++)
            a->px[y*5+x] = (rgba_t){200,0,0,255};
        int key[3]={255,0,255};
        pet_img_t *out = pet_remove_background(a, key, 90.0f);
        /* border magenta should now be transparent */
        TEST(out->px[0].a==0, "remove_background clears border bg");
        /* interior red preserved */
        TEST(out->px[2*5+2].a==255 && out->px[2*5+2].r==200, "remove_background keeps interior pet");
        pet_img_free(a); pet_img_free(out);
    }

    /* pet_fit_to_cell: empty image -> transparent cell of correct size */
    {
        pet_img_t *a = make(4,4); /* fully transparent */
        pet_img_t *c = pet_fit_to_cell(a, 192, 208);
        TEST(c->w==192 && c->h==208, "fit_to_cell size");
        int any=0; for (int i=0;i<192*208;i++) if (c->px[i].a>0) any=1;
        TEST(!any, "fit_to_cell empty stays empty");
        pet_img_free(a); pet_img_free(c);
    }

    if (g_fail==0) printf("ALL PASSED\n"); else printf("%d FAIL\n", g_fail);
    return g_fail ? 1 : 0;
}
