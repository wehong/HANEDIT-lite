/* HANEDIT lite: 글꼴 파일(HANEDIT.FNT)과 글자 캐시
 *
 * HANEDIT.FNT (tools/genfont.py --fnt 로 만듦, 모든 수는 little-endian)
 *   "HFN1", u16 본 표 범위 수, u16 넓은 한글 표 범위 수
 *   범위마다 { u16 첫 글자, u16 끝 글자, u8 넓음(0/1), u8 0, u32 파일 안 위치 }
 *   글자 그림: 8x16 = 16바이트, 16x16 = 32바이트
 *
 * 메모리: 범위 표(수 KB) + 2방향 집합 연관 캐시 512집합 x 2 = 글자 1,024개(약 36KB).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "font.h"

typedef struct {
    unsigned short first, last;
    unsigned char  wide;
    unsigned long  off;
} FR;

static FR  *rmain, *ralt;
static int  nmain, nalt;
static FILE *ff;

int font_hangul_wide = 1;

#define CSETS 512                       /* 집합 수 (2의 거듭제곱) */
typedef struct {
    unsigned short ch;
    unsigned char  alt, used;
    unsigned char  data[32];
} Slot;
static Slot *cache;                     /* CSETS * 2 */
static unsigned char lru[CSETS];        /* 집합마다 최근에 쓴 쪽 (0/1) */

static unsigned rd16(const unsigned char *p) { return p[0] | (p[1] << 8); }
static unsigned long rd32(const unsigned char *p)
{
    return p[0] | ((unsigned)p[1] << 8) | ((unsigned long)p[2] << 16) | ((unsigned long)p[3] << 24);
}

static int read_ranges(FR *r, int n)
{
    unsigned char b[10];
    int i;
    for (i = 0; i < n; i++) {
        if (fread(b, 1, 10, ff) != 10) return -1;
        r[i].first = (unsigned short)rd16(b);
        r[i].last  = (unsigned short)rd16(b + 2);
        r[i].wide  = b[4];
        r[i].off   = rd32(b + 6);
    }
    return 0;
}

int font_open(const char *path)
{
    unsigned char h[8];
    ff = fopen(path, "rb");
    if (!ff) return -1;
    if (fread(h, 1, 8, ff) != 8 || memcmp(h, "HFN1", 4) != 0) { fclose(ff); ff = 0; return -2; }
    nmain = rd16(h + 4);
    nalt  = rd16(h + 6);
    rmain = (FR *)malloc(nmain * sizeof(FR));
    ralt  = (FR *)malloc((nalt ? nalt : 1) * sizeof(FR));
    cache = (Slot *)calloc(CSETS * 2, sizeof(Slot));
    if (!rmain || !ralt || !cache || read_ranges(rmain, nmain) || read_ranges(ralt, nalt)) {
        fclose(ff); ff = 0;
        return -3;
    }
    return 0;
}

void font_close(void)
{
    if (ff) fclose(ff);
    ff = 0;
}

static const FR *find(const FR *rs, int n, unsigned short ch)
{
    int lo = 0, hi = n - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (ch < rs[mid].first) hi = mid - 1;
        else if (ch > rs[mid].last) lo = mid + 1;
        else return &rs[mid];
    }
    return 0;
}

/* 넓은 한글 표를 먼저 봄 (hangul_wide 일 때). *alt: 넓은 한글 표에서 찾았으면 1 */
static const FR *lookup(unsigned short ch, int hangul_wide, int *alt)
{
    const FR *r = 0;
    *alt = 0;
    if (hangul_wide && ch >= 0x3131 && (r = find(ralt, nalt, ch)) != 0) { *alt = 1; return r; }
    return find(rmain, nmain, ch);
}

int font_width_m(unsigned short ch, int hangul_wide)
{
    int alt;
    const FR *r = lookup(ch, hangul_wide, &alt);
    return r ? (r->wide ? 2 : 1) : 0;
}

const unsigned char *font_glyph_m(unsigned short ch, int hangul_wide, int *wide)
{
    int alt, w, n;
    unsigned set;
    Slot *s;
    const FR *r = lookup(ch, hangul_wide, &alt);
    if (!r || !ff) return 0;
    *wide = r->wide;
    set = (ch ^ (ch >> 9) ^ (alt << 7)) & (CSETS - 1);
    s = &cache[set * 2];
    for (w = 0; w < 2; w++)
        if (s[w].used && s[w].ch == ch && s[w].alt == alt) { lru[set] = (unsigned char)w; return s[w].data; }
    w = lru[set] ^ 1;                   /* 덜 최근에 쓴 쪽을 바꿈 */
    n = r->wide ? 32 : 16;
    if (fseek(ff, r->off + (unsigned long)(ch - r->first) * n, SEEK_SET) != 0 ||
        fread(s[w].data, 1, n, ff) != (size_t)n)
        return 0;
    s[w].ch = ch; s[w].alt = (unsigned char)alt; s[w].used = 1;
    lru[set] = (unsigned char)w;
    return s[w].data;
}

/* ---- 미리 읽기: 화면을 그리기 전에 필요한 글자를 모아 파일 순서대로 한꺼번에 읽음.
   글자마다 따로 읽으면(fseek+fread) 그리는 도중에 디스크를 오가느라 화면이 왼쪽 위부터
   천천히 그려지는 것이 보임 */
#define NPEND 400
typedef struct { unsigned long off; unsigned short ch; unsigned char alt, n; } Pend;
static Pend pend[NPEND];
static int  npend;
static short phash[256];                /* 글자 → pend 번호+1 (같은 글자를 빨리 거르려고) */
static short pnext[NPEND];
static unsigned char chunk[4096];

static Slot *cache_find(unsigned short ch, int alt, unsigned *setp)
{
    unsigned set = (ch ^ (ch >> 9) ^ (alt << 7)) & (CSETS - 1);
    Slot *s = &cache[set * 2];
    int w;
    *setp = set;
    for (w = 0; w < 2; w++)
        if (s[w].used && s[w].ch == ch && s[w].alt == alt) return &s[w];
    return 0;
}

void font_want(unsigned short ch, int hangul_wide)
{
    int alt, i;
    unsigned set;
    const FR *r;
    if (!ff || npend >= NPEND) return;
    r = lookup(ch, hangul_wide, &alt);
    if (!r || cache_find(ch, alt, &set)) return;
    for (i = phash[ch & 255]; i; i = pnext[i - 1])
        if (pend[i - 1].ch == ch && pend[i - 1].alt == alt) return;
    pnext[npend] = phash[ch & 255];
    phash[ch & 255] = (short)(npend + 1);
    pend[npend].n = r->wide ? 32 : 16;
    pend[npend].off = r->off + (unsigned long)(ch - r->first) * pend[npend].n;
    pend[npend].ch = ch; pend[npend].alt = (unsigned char)alt;
    npend++;
}

static int pend_cmp(const void *a, const void *b)
{
    unsigned long x = ((const Pend *)a)->off, y = ((const Pend *)b)->off;
    return x < y ? -1 : x > y;
}

void font_fetch(void)
{
    int i = 0;
    if (!npend) return;
    memset(phash, 0, sizeof(phash));
    qsort(pend, npend, sizeof(Pend), pend_cmp);
    while (i < npend) {
        unsigned long base = pend[i].off;
        size_t got;
        if (fseek(ff, base, SEEK_SET) != 0) break;
        got = fread(chunk, 1, sizeof(chunk), ff);
        if (got < pend[i].n) { i++; continue; }  /* 파일 끝 (일어나지 않아야 함) */
        /* 이 4KB 안에 드는 글자는 모두 캐시에 넣음 */
        while (i < npend && pend[i].off + pend[i].n <= base + got) {
            unsigned set;
            Slot *s;
            int w;
            if (!cache_find(pend[i].ch, pend[i].alt, &set)) {
                w = lru[set] ^ 1;
                s = &cache[set * 2 + w];
                memcpy(s->data, chunk + (unsigned)(pend[i].off - base), pend[i].n);
                s->ch = pend[i].ch; s->alt = pend[i].alt; s->used = 1;
                lru[set] = (unsigned char)w;
            }
            i++;
        }
        if (got == 0) break;
    }
    npend = 0;
}

const unsigned char *font_glyph(unsigned short ch, int *wide)
{
    return font_glyph_m(ch, font_hangul_wide, wide);
}
