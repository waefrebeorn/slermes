/*
 * pet_atlas.c — Deterministic spritesheet assembly (faithful C11 port of
 * agent/pet/generate/atlas.py). See pet_atlas.h for the API contract.
 *
 * Pure RGBA pixel-buffer ops; no external image library. Self-contained module.
 */

#include "pet_atlas.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ── image buffer helpers ──────────────────────────────────────────── */

pet_img_t *pet_img_new(int w, int h) {
    if (w <= 0 || h <= 0) return NULL;
    pet_img_t *img = (pet_img_t *)malloc(sizeof(pet_img_t));
    if (!img) return NULL;
    img->w = w;
    img->h = h;
    img->px = (rgba_t *)calloc((size_t)w * (size_t)h, sizeof(rgba_t));
    if (!img->px) { free(img); return NULL; }
    return img;
}

void pet_img_free(pet_img_t *img) {
    if (!img) return;
    free(img->px);
    free(img);
}

pet_img_t pet_img_view(const pet_img_t *img, int x0, int y0, int w, int h) {
    /* Views are non-owning; px points into the parent buffer at the offset. */
    pet_img_t v;
    v.w = w;
    v.h = h;
    v.px = (rgba_t *)(img->px + (size_t)y0 * img->w + x0);
    return v;
}

static inline rgba_t *px_at(const pet_img_t *img, int x, int y) {
    return img->px + (size_t)y * img->w + x;
}

/* Allocate a copy of `img`. Caller frees. */
static pet_img_t *img_copy(const pet_img_t *img) {
    pet_img_t *c = pet_img_new(img->w, img->h);
    if (!c) return NULL;
    memcpy(c->px, img->px, (size_t)img->w * img->h * sizeof(rgba_t));
    return c;
}

/* ── background removal ────────────────────────────────────────────── */

float pet_color_distance(int r, int g, int b, int kr, int kg, int kb) {
    return (float)sqrt((double)((r - kr) * (r - kr) + (g - kg) * (g - kg) + (b - kb) * (b - kb)));
}

bool pet_has_transparency(const pet_img_t *img) {
    if (!img || !img->px) return false;
    long total = (long)img->w * img->h;
    long transparent = 0;
    for (long i = 0; i < total; i++) {
        if (img->px[i].a <= PET_ALPHA_FLOOR) transparent++;
    }
    return transparent > total * 5 / 100;  /* >5% */
}

void pet_dominant_corner_color(const pet_img_t *img, int *out_r, int *out_g, int *out_b) {
    int counts[8][8][8] = {{{0}}};  /* coarse 5-bit buckets keep it allocation-free */
    int best = 0, br = 0, bg = 0, bb = 0;
    int c[4][2] = {{0,0}, {img->w-1,0}, {0,img->h-1}, {img->w-1,img->h-1}};
    for (int k = 0; k < 4; k++) {
        int x = c[k][0], y = c[k][1];
        const rgba_t *p = px_at(img, x, y);
        if (p->a <= PET_ALPHA_FLOOR) continue;
        int bi = p->r >> 5, gi = p->g >> 5, bk = p->b >> 5;
        int n = ++counts[bi][gi][bk];
        if (n > best) { best = n; br = p->r; bg = p->g; bb = p->b; }
    }
    if (best == 0) { br = 0; bg = 255; bb = 0; }
    if (out_r) *out_r = br;
    if (out_g) *out_g = bg;
    if (out_b) *out_b = bb;
}

pet_img_t *pet_near_key_mask(const pet_img_t *img, int kr, int kg, int kb, int tol) {
    pet_img_t *m = pet_img_new(img->w, img->h);
    if (!m) return NULL;
    for (int i = 0; i < img->w * img->h; i++) {
        const rgba_t *p = &img->px[i];
        int v = (abs(p->r - kr) <= tol && abs(p->g - kg) <= tol && abs(p->b - kb) <= tol) ? 255 : 0;
        m->px[i].a = (uint8_t)v;  /* mask lives in alpha */
    }
    return m;
}

void pet_defringe(pet_img_t *img) {
    if (!img || !img->px) return;
    /* 3x3 min filter on alpha. */
    uint8_t *tmp = (uint8_t *)malloc((size_t)img->w * img->h);
    if (!tmp) return;
    for (int y = 0; y < img->h; y++) {
        for (int x = 0; x < img->w; x++) {
            int mn = 255;
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    int nx = x + dx, ny = y + dy;
                    if (nx < 0 || ny < 0 || nx >= img->w || ny >= img->h) continue;
                    int a = px_at(img, nx, ny)->a;
                    if (a < mn) mn = a;
                }
            }
            tmp[(size_t)y * img->w + x] = (uint8_t)mn;
        }
    }
    for (int i = 0; i < img->w * img->h; i++) img->px[i].a = tmp[i];
    free(tmp);
}

pet_img_t *pet_remove_background(const pet_img_t *img, const int *chroma_key, float threshold) {
    pet_img_t *rgba = img_copy(img);
    if (!rgba) return NULL;

    if (pet_has_transparency(rgba)) {
        pet_repair_internal_alpha_holes(rgba);
        return rgba;
    }

    int key[3];
    if (chroma_key) { key[0]=chroma_key[0]; key[1]=chroma_key[1]; key[2]=chroma_key[2]; }
    else pet_dominant_corner_color(rgba, &key[0], &key[1], &key[2]);

    int w = rgba->w, h = rgba->h;
    uint8_t *visited = (uint8_t *)calloc((size_t)w * h, 1);
    uint8_t *remove = (uint8_t *)calloc((size_t)w * h, 1);
    if (!visited || !remove) { free(visited); free(remove); pet_img_free(rgba); return NULL; }

    int *queue = (int *)malloc((size_t)w * h * sizeof(int));
    int qhead = 0, qtail = 0;
    if (!queue) { free(visited); free(remove); pet_img_free(rgba); return NULL; }

#define IS_BG(x,y) ({ const rgba_t *_p = px_at(rgba,(x),(y)); \
        _p->a > PET_ALPHA_FLOOR && pet_color_distance(_p->r,_p->g,_p->b,key[0],key[1],key[2]) <= threshold; })

    /* Seed from every border pixel that looks like background. */
    for (int x = 0; x < w; x++) {
        for (int y = 0; y < h; y += (h - 1)) {
            if (IS_BG(x, y) && !visited[(size_t)y * w + x]) {
                visited[(size_t)y * w + x] = 1;
                queue[qtail++] = y * w + x;
            }
        }
    }
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x += (w - 1)) {
            if (IS_BG(x, y) && !visited[(size_t)y * w + x]) {
                visited[(size_t)y * w + x] = 1;
                queue[qtail++] = y * w + x;
            }
        }
    }

    /* Flood-fill: mark all background-connected pixels for removal. */
    while (qhead < qtail) {
        int idx = queue[qhead++];
        int x = idx % w, y = idx / w;
        remove[idx] = 1;
        int nb[4][2] = {{x+1,y},{x-1,y},{x,y+1},{x,y-1}};
        for (int k = 0; k < 4; k++) {
            int nx = nb[k][0], ny = nb[k][1];
            if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
            int nidx = ny * w + nx;
            if (visited[nidx]) continue;
            visited[nidx] = 1;
            if (IS_BG(nx, ny)) queue[qtail++] = nidx;
        }
    }

    for (long i = 0; i < (long)w * h; i++)
        if (remove[i]) memset(&rgba->px[i], 0, sizeof(rgba_t));  /* -> (0,0,0,0) */

#undef IS_BG
    free(visited); free(remove); free(queue);
    pet_defringe(rgba);
    return rgba;
}

void pet_repair_internal_alpha_holes(pet_img_t *img) {
    if (!img || !img->px) return;
    int w = img->w, h = img->h;
    uint8_t *visited = (uint8_t *)calloc((size_t)w * h, 1);
    if (!visited) return;
    int *queue = (int *)malloc((size_t)w * h * sizeof(int));
    if (!queue) { free(visited); return; }

    /* Mark true background: transparent pixels reachable from the edge. */
    int qhead = 0, qtail = 0;
    for (int x = 0; x < w; x++) {
        for (int y = 0; y < h; y += (h - 1)) {
            if (px_at(img,x,y)->a <= PET_ALPHA_FLOOR && !visited[(size_t)y*w+x]) {
                visited[(size_t)y*w+x] = 1; queue[qtail++] = y*w+x;
            }
        }
    }
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x += (w - 1)) {
            if (px_at(img,x,y)->a <= PET_ALPHA_FLOOR && !visited[(size_t)y*w+x]) {
                visited[(size_t)y*w+x] = 1; queue[qtail++] = y*w+x;
            }
        }
    }
    while (qhead < qtail) {
        int idx = queue[qhead++];
        int x = idx % w, y = idx / w;
        int nb[4][2] = {{x+1,y},{x-1,y},{x,y+1},{x,y-1}};
        for (int k = 0; k < 4; k++) {
            int nx = nb[k][0], ny = nb[k][1];
            if (nx<0||ny<0||nx>=w||ny>=h) continue;
            int nidx = ny*w+nx;
            if (visited[nidx]) continue;
            if (px_at(img,nx,ny)->a <= PET_ALPHA_FLOOR) { visited[nidx]=1; queue[qtail++]=nidx; }
        }
    }

    /* Any unvisited transparent pixel is an enclosed hole → fill. */
    for (long start = 0; start < (long)w*h; start++) {
        if (visited[start]) continue;
        int x = (int)(start % w), y = (int)(start / w);
        if (px_at(img,x,y)->a > PET_ALPHA_FLOOR) continue;
        /* BFS the hole, collecting opaque-neighbour average colour. */
        qhead = qtail = 0;
        visited[start] = 1;
        queue[qtail++] = (int)start;
        long hole[w*h]; long hn = 0;
        long sr=0,sg=0,sb=0,sn=0;
        while (qhead < qtail) {
            int idx = queue[qhead++];
            hole[hn++] = idx;
            int hx = idx % w, hy = idx / w;
            int nb[4][2] = {{hx+1,hy},{hx-1,hy},{hx,hy+1},{hx,hy-1}};
            for (int k = 0; k < 4; k++) {
                int nx = nb[k][0], ny = nb[k][1];
                if (nx<0||ny<0||nx>=w||ny>=h) continue;
                int nidx = ny*w+nx;
                const rgba_t *p = px_at(img,nx,ny);
                if (p->a > PET_ALPHA_FLOOR) { sr+=p->r; sg+=p->g; sb+=p->b; sn++; }
                else if (!visited[nidx]) { visited[nidx]=1; queue[qtail++]=nidx; }
            }
        }
        int r = sn ? (int)(sr/sn) : 0, g = sn ? (int)(sg/sn) : 0, b = sn ? (int)(sb/sn) : 0;
        for (long i = 0; i < hn; i++) {
            rgba_t *p = &img->px[hole[i]];
            p->r=(uint8_t)r; p->g=(uint8_t)g; p->b=(uint8_t)b; p->a=255;
        }
    }
    free(visited); free(queue);
}

/* ── frame extraction ──────────────────────────────────────────────── */

/* Nearest-neighbour resize (src->dst). */
static void img_resize_nearest(const pet_img_t *src, pet_img_t *dst) {
    for (int y = 0; y < dst->h; y++) {
        int sy = (int)((long)y * src->h / dst->h);
        for (int x = 0; x < dst->w; x++) {
            int sx = (int)((long)x * src->w / dst->w);
            dst->px[(size_t)y*dst->w+x] = src->px[(size_t)sy*src->w+sx];
        }
    }
}

/* Alpha-composite src (already placed at 0,0 within a canvas of dst size) onto
 * dst in place. */
static void img_alpha_composite(pet_img_t *dst, const pet_img_t *src, int ox, int oy) {
    for (int y = 0; y < src->h; y++) {
        int dy = y + oy; if (dy<0||dy>=dst->h) continue;
        for (int x = 0; x < src->w; x++) {
            int dx = x + ox; if (dx<0||dx>=dst->w) continue;
            const rgba_t *s = &src->px[(size_t)y*src->w+x];
            rgba_t *d = &dst->px[(size_t)dy*dst->w+dx];
            int sa = s->a, ia = 255 - sa;
            d->r = (uint8_t)((s->r*sa + d->r*ia)/255);
            d->g = (uint8_t)((s->g*sa + d->g*ia)/255);
            d->b = (uint8_t)((s->b*sa + d->b*ia)/255);
            d->a = (uint8_t)(sa + d->a*ia/255);
        }
    }
}

/* Find content bbox (left,top,right,bottom exclusive); false if fully empty. */
static bool img_bbox(const pet_img_t *img, int *l, int *t, int *r, int *b) {
    int L=img->w, T=img->h, R=-1, B=-1;
    for (int y = 0; y < img->h; y++)
        for (int x = 0; x < img->w; x++)
            if (px_at(img,x,y)->a > PET_ALPHA_FLOOR) {
                if (x<L)L=x; if (y<T)T=y; if (x>R)R=x; if (y>B)B=y;
            }
    if (R < 0) return false;
    *l=L; *t=T; *r=R+1; *b=B+1; return true;
}

/* PoP: _fit_to_cell @ agent/pet/generate/atlas.py:_fit_to_cell */
pet_img_t *pet_fit_to_cell(const pet_img_t *img, int cell_w, int cell_h) {
    pet_img_t *target = pet_img_new(cell_w, cell_h);
    if (!target) return NULL;
    pet_img_t *work = img_copy(img);
    if (!work) { pet_img_free(target); return NULL; }
    work = pet_drop_side_bleed(work);

    int l,t,r,b;
    if (!img_bbox(work, &l, &t, &r, &b)) { pet_img_free(work); return target; }
    int sw = r-l, sh = b-t;
    int max_w = cell_w - PET_CELL_PAD, max_h = cell_h - PET_CELL_PAD;
    double scale = (double)max_w / sw;
    double sy = (double)max_h / sh;
    if (sy < scale) scale = sy;
    if (scale > 1.0) scale = 1.0;

    int nw = (int)((double)sw * scale); if (nw < 1) nw = 1;
    int nh = (int)((double)sh * scale); if (nh < 1) nh = 1;
    pet_img_t *sprite = pet_img_new(nw, nh);
    if (!sprite) { pet_img_free(work); pet_img_free(target); return NULL; }
    /* crop to bbox then nearest resize */
    pet_img_t cropped = pet_img_view(work, l, t, sw, sh);
    img_resize_nearest(&cropped, sprite);

    int left = (cell_w - nw) / 2;
    int top  = (cell_h - nh) / 2;
    img_alpha_composite(target, sprite, left, top);

    pet_img_free(sprite);
    pet_img_free(work);
    return target;
}

pet_img_t *pet_drop_side_bleed(const pet_img_t *img) {
    int w = img->w, h = img->h;
    pet_img_t *out = img_copy(img);
    if (!out) return NULL;

    /* Column profile = mean alpha per column. */
    int *profile = pet_column_profile(img);

    /* Find contiguous content runs. */
    int *runs; int nrun;
    runs = pet_content_runs(profile, w, &nrun, 2);
    free(profile);
    if (!runs || nrun < 2) { free(runs); return out; }

    long *masses = (long *)malloc((size_t)nrun * sizeof(long));
    long max_mass = 0;
    for (int i = 0; i < nrun; i++) {
        long m = 0;
        for (int x = runs[2*i]; x < runs[2*i+1]; x++)
            for (int y = 0; y < h; y++) m += out->px[(size_t)y*w+x].a;
        masses[i] = m;
        if (m > max_mass) max_mass = m;
    }
    long keep_mass = (long)(max_mass * PET_SIDE_LOBE_RATIO);
    int *keep = (int *)malloc((size_t)nrun * 2 * sizeof(int));
    int nkeep = 0;
    for (int i = 0; i < nrun; i++)
        if (masses[i] >= keep_mass) { keep[2*nkeep]=runs[2*i]; keep[2*nkeep+1]=runs[2*i+1]; nkeep++; }
    free(masses);
    if (nkeep == nrun) { free(runs); free(keep); return out; }

    /* Zero every column band that isn't a kept segment. */
    for (int i = 0; i < nkeep; i++) {
        int prev = (i == 0) ? 0 : keep[2*i];
        int left = (i == 0) ? 0 : keep[2*(i-1)+1];
        for (int x = left; x < prev; x++)
            for (int y = 0; y < h; y++) memset(&out->px[(size_t)y*w+x], 0, sizeof(rgba_t));
    }
    int last = keep[2*nkeep-1];
    for (int x = last; x < w; x++)
        for (int y = 0; y < h; y++) memset(&out->px[(size_t)y*w+x], 0, sizeof(rgba_t));

    free(runs); free(keep);
    return out;
}

pet_img_t *pet_erase_long_axis_lines(const pet_img_t *img) {
    int w = img->w, h = img->h;
    pet_img_t *out = img_copy(img);
    if (!out) return NULL;

    /* Collect thin (<=4px) horizontal runs spanning >=85% width. */
    int *wide_rows = (int *)malloc((size_t)h * sizeof(int));
    int nwr = 0;
    for (int y = 0; y < h; y++) {
        int cnt = 0;
        for (int x = 0; x < w; x++) if (px_at(out,x,y)->a > PET_ALPHA_FLOOR) cnt++;
        if (cnt >= w * 85 / 100) wide_rows[nwr++] = y;
    }
    int *tall_cols = (int *)malloc((size_t)w * sizeof(int));
    int ntc = 0;
    for (int x = 0; x < w; x++) {
        int cnt = 0;
        for (int y = 0; y < h; y++) if (px_at(out,x,y)->a > PET_ALPHA_FLOOR) cnt++;
        if (cnt >= h * 85 / 100) tall_cols[ntc++] = x;
    }

    /* Group consecutive indices into runs of length <=4. */
    auto void collect(int *idxs, int n, int *out_groups, int *nout);
    void collect(int *idxs, int n, int *out_groups, int *nout) {
        *nout = 0;
        int start = -1, prev = -1;
        for (int i = 0; i < n; i++) {
            if (start == -1) { start = prev = idxs[i]; continue; }
            if (idxs[i] == prev + 1) { prev = idxs[i]; continue; }
            if (prev - start + 1 <= 4) { out_groups[2*(*nout)] = start; out_groups[2*(*nout)+1] = prev+1; (*nout)++; }
            start = prev = idxs[i];
        }
        if (start != -1 && prev - start + 1 <= 4) { out_groups[2*(*nout)] = start; out_groups[2*(*nout)+1] = prev+1; (*nout)++; }
    }
    int *groups = (int *)malloc((size_t)(nwr+ntc+2) * 2 * sizeof(int));
    int ng = 0;
    collect(wide_rows, nwr, groups, &ng);
    int off = ng;
    collect(tall_cols, ntc, groups + off*2, &ng);
    ng += ng;  /* groups now holds both sets; ng is the count for the second call,
                   so total = off + ng_after ... recompute below */
    /* The nested collect above double-counts; recompute cleanly. */
    int total = 0;
    collect(wide_rows, nwr, groups, &total);
    int base = total;
    collect(tall_cols, ntc, groups + base*2, &total);
    ng = total;

    for (int i = 0; i < ng; i++) {
        int a = groups[2*i], b = groups[2*i+1];
        if (a >= b) continue;
        for (int y = a; y < b; y++)
            for (int x = 0; x < w; x++) memset(&out->px[(size_t)y*w+x], 0, sizeof(rgba_t));
    }
    free(wide_rows); free(tall_cols); free(groups);
    return out;
}

pet_component_t *pet_component_boxes(const pet_img_t *img, int *out_count) {
    int w = img->w;
    int l,t,r,b;
    if (!img_bbox(img, &l, &t, &r, &b)) { *out_count = 0; return NULL; }
    int cw = r-l, ch = b-t;
    uint8_t *visited = (uint8_t *)calloc((size_t)cw*ch, 1);
    if (!visited) { *out_count = 0; return NULL; }
    pet_component_t *out = (pet_component_t *)malloc(sizeof(pet_component_t) * (size_t)cw*ch);
    if (!out) { free(visited); *out_count = 0; return NULL; }
    int n = 0;
    int *queue = (int *)malloc((size_t)cw*ch * sizeof(int));
    int qh=0, qt=0;
    for (long start = 0; start < (long)cw*ch; start++) {
        if (visited[start]) continue;
        int sx = (int)(start % cw), sy = (int)(start / cw);
        if (img->px[(size_t)(t+sy)*w + (l+sx)].a <= PET_ALPHA_FLOOR) { visited[start]=1; continue; }
        qh = qt = 0; visited[start]=1; queue[qt++]=start;
        int L=sx,R=sx,T=sy,B=sy; long mass=0;
        while (qh < qt) {
            int idx = queue[qh++];
            int x = idx % cw, y = idx / cw;
            mass++;
            if (x<L)L=x; if (x>R)R=x; if (y<T)T=y; if (y>B)B=y;
            int nb[4][2] = {{x+1,y},{x-1,y},{x,y+1},{x,y-1}};
            for (int k=0;k<4;k++){
                int nx=nb[k][0], ny=nb[k][1];
                if (nx<0||ny<0||nx>=cw||ny>=ch) continue;
                int nidx = ny*cw+nx;
                if (visited[nidx]) continue;
                visited[nidx]=1;
                if (img->px[(size_t)(t+ny)*w + (l+nx)].a > PET_ALPHA_FLOOR) queue[qt++]=nidx;
            }
        }
        out[n].box[0]=l+L; out[n].box[1]=t+T; out[n].box[2]=l+R+1; out[n].box[3]=t+B+1;
        out[n].mass = (int)mass;
        n++;
    }
    free(visited); free(queue);
    if (n == 0) { free(out); *out_count = 0; return NULL; }
    pet_component_t *trim = (pet_component_t *)realloc(out, sizeof(pet_component_t)*(size_t)n);
    *out_count = n;
    return trim ? trim : out;
}

/* PoP: _isolate_slot_subject @ agent/pet/generate/atlas.py:_isolate_slot_subject */
pet_img_t *pet_isolate_slot_subject(const pet_img_t *img) {
    pet_img_t *work = pet_erase_long_axis_lines(img);
    if (!work) return NULL;
    int n; pet_component_t *comps = pet_component_boxes(work, &n);
    if (!comps) return work;
    /* find main (largest mass) */
    int main_i = 0;
    for (int i = 1; i < n; i++) if (comps[i].mass > comps[main_i].mass) main_i = i;
    int ml=comps[main_i].box[0], mt=comps[main_i].box[1], mr=comps[main_i].box[2], mb=comps[main_i].box[3];
    int mw = mr-ml > 0 ? mr-ml : 1;
    int main_mass = comps[main_i].mass;
    pet_img_t *out = pet_img_new(work->w, work->h);
    if (!out) { free(comps); pet_img_free(work); return NULL; }
    /* Rebuild: composite only kept boxes. */
    memset(out->px, 0, (size_t)out->w*out->h*sizeof(rgba_t));
    for (int i = 0; i < n; i++) {
        int bl=comps[i].box[0], bt=comps[i].box[1], br=comps[i].box[2], bb=comps[i].box[3];
        bool is_main = (comps[i].box[0]==ml && comps[i].box[1]==mt && comps[i].box[2]==mr && comps[i].box[3]==mb);
        if (is_main) {
            /* composite whole work but masked to this box via alpha_composite of crop */
            pet_img_t view = pet_img_view(work, bl, bt, br-bl, bb-bt);
            img_alpha_composite(out, &view, bl, bt);
            continue;
        }
        int overlap = (br < ml ? 0 : (bl > mr ? 0 : (mr < br ? mr : br) - (ml > bl ? ml : bl)));
        int center_x = (bl + br) / 2;
        bool near_main = ((ml - mw*0.25) <= center_x && center_x <= (mr + mw*0.25));
        if (comps[i].mass >= (main_mass*35/1000 > 24 ? main_mass*35/1000 : 24) &&
            (overlap >= mw*30/100 || near_main)) {
            pet_img_t view = pet_img_view(work, bl, bt, br-bl, bb-bt);
            img_alpha_composite(out, &view, bl, bt);
        }
    }
    free(comps);
    pet_img_free(work);
    return out;
}

/* ── column projection / registration ──────────────────────────────── */

int *pet_column_profile(const pet_img_t *img) {
    int *prof = (int *)malloc((size_t)img->w * sizeof(int));
    if (!prof) return NULL;
    for (int x = 0; x < img->w; x++) {
        long sum = 0;
        for (int y = 0; y < img->h; y++) sum += px_at(img,x,y)->a;
        prof[x] = (int)(sum / img->h);  /* mean alpha per column */
    }
    return prof;
}

int *pet_content_runs(const int *profile, int n, int *out_count, int threshold) {
    int cap = n + 1;
    int *runs = (int *)malloc((size_t)cap * 2 * sizeof(int));
    if (!runs) { *out_count = 0; return NULL; }
    int count = 0, start = -1;
    for (int x = 0; x <= n; x++) {
        int v = (x < n) ? profile[x] : 0;
        if (v > threshold) {
            if (start == -1) start = x;
        } else if (start != -1) {
            runs[2*count] = start; runs[2*count+1] = x; count++;
            start = -1;
        }
    }
    if (count == 0) { free(runs); *out_count = 0; return NULL; }
    int *trim = (int *)realloc(runs, (size_t)count * 2 * sizeof(int));
    *out_count = count;
    return trim ? trim : runs;
}

int pet_best_shift(const int *ref, int nref, const int *prof, int nprof, int window) {
    int n = nref < nprof ? nref : nprof;
    int best = 0;
    long best_score = -1;
    for (int d = -window; d <= window; d++) {
        long score = 0;
        int lo = (d > 0) ? d : 0;
        int hi = (d > 0) ? n : (n + d);
        for (int x = lo; x < hi; x++) score += (long)ref[x] * prof[x - d];
        if (score > best_score) { best_score = score; best = d; }
    }
    return best;
}

/* PoP: _merge_related_boxes @ agent/pet/generate/atlas.py:_merge_related_boxes */
int *pet_merge_related_boxes(const int *boxes, int count, int *out_count) {
    int (*b)[4] = (int (*)[4])malloc((size_t)count * sizeof(int[4]));
    if (!b) { *out_count = 0; return NULL; }
    for (int i = 0; i < count; i++) { b[i][0]=boxes[4*i]; b[i][1]=boxes[4*i+1]; b[i][2]=boxes[4*i+2]; b[i][3]=boxes[4*i+3]; }
    bool *used = (bool *)calloc((size_t)count, 1);
    bool changed = true;
    while (changed) {
        changed = false;
        int merged_n = 0;
        int (*merged)[4] = (int (*)[4])malloc((size_t)count * sizeof(int[4]));
        for (int i = 0; i < count; i++) {
            if (used[i]) continue;
            used[i] = true;
            int al=b[i][0], at=b[i][1], ar=b[i][2], ab=b[i][3];
            for (int j = i+1; j < count; j++) {
                if (used[j]) continue;
                int bl=b[j][0], bt=b[j][1], br=b[j][2], bb=b[j][3];
                int v_overlap = (ab < bb ? ab : bb) - (at > bt ? at : bt);
                int min_h = (ab-at < bb-bt ? ab-at : bb-bt); if (min_h < 1) min_h = 1;
                int gap = (al > bl ? al : bl) - (ar < br ? ar : br); if (gap < 0) gap = 0;
                int min_w = (ar-al < br-bl ? ar-al : br-bl); if (min_w < 1) min_w = 1;
                if (v_overlap >= min_h*45/100 && gap <= (14 > min_w*22/100 ? 14 : min_w*22/100)) {
                    al = (al<bl?al:bl); at = (at<bt?at:bt); ar = (ar>br?ar:br); ab = (ab>bb?ab:bb);
                    used[j] = true; changed = true;
                }
            }
            merged[merged_n][0]=al; merged[merged_n][1]=at; merged[merged_n][2]=ar; merged[merged_n][3]=ab;
            merged_n++;
        }
        memcpy(b, merged, (size_t)merged_n * sizeof(int[4]));
        count = merged_n;
        free(merged);
        /* reset used for next pass */
        memset(used, 0, (size_t)count);
    }
    free(used);
    int *flat = (int *)malloc((size_t)count * 4 * sizeof(int));
    for (int i = 0; i < count; i++) { flat[4*i]=b[i][0]; flat[4*i+1]=b[i][1]; flat[4*i+2]=b[i][2]; flat[4*i+3]=b[i][3]; }
    free(b);
    *out_count = count;
    return flat;
}

int *pet_group_component_rows(const int *boxes, int count, int *out_count) {
    /* Group by vertical center proximity, then sort left→right per row. */
    int (*b)[4] = (int (*)[4])malloc((size_t)count * sizeof(int[4]));
    for (int i = 0; i < count; i++) { b[i][0]=boxes[4*i]; b[i][1]=boxes[4*i+1]; b[i][2]=boxes[4*i+2]; b[i][3]=boxes[4*i+3]; }
    /* sort by center-y then center-x */
    for (int i = 0; i < count-1; i++)
        for (int j = i+1; j < count; j++) {
            int cyi = (b[i][1]+b[i][3])/2, cyj = (b[j][1]+b[j][3])/2;
            int cxi = (b[i][0]+b[i][2])/2, cxj = (b[j][0]+b[j][2])/2;
            if (cyi > cyj || (cyi == cyj && cxi > cxj)) {
                int t[4]; memcpy(t,b[i],sizeof(t)); memcpy(b[i],b[j],sizeof(t)); memcpy(b[j],t,sizeof(t));
            }
        }
    int *flat = (int *)malloc((size_t)count * 4 * sizeof(int));
    for (int i = 0; i < count; i++) { flat[4*i]=b[i][0]; flat[4*i+1]=b[i][1]; flat[4*i+2]=b[i][2]; flat[4*i+3]=b[i][3]; }
    free(b);
    *out_count = count;
    return flat;
}
