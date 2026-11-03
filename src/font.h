#ifndef FONT_H
#define FONT_H

/* HANEDIT lite: 글꼴은 HANEDIT.FNT 파일에 있고, 화면에 나온 글자만 읽어 캐시에 둠.
   메모리에는 범위 표(글자 폭을 아는 데 씀)와 글자 1,024개분 캐시만 올림. */

/* font_hangul_wide: 지금 글자를 놓을 때 쓰는 한글 글꼴 (1 넓은, 0 좁은).
   메뉴·대화 상자는 늘 1, 본문은 글꼴 메뉴 설정을 따름 */
extern int font_hangul_wide;

int  font_open(const char *path);    /* 0: 성공 */
void font_close(void);

/* 글자 폭 (1 또는 2). 글꼴에 없으면 0. 디스크를 읽지 않음 */
int  font_width_m(unsigned short ch, int hangul_wide);

/* 글자 그림 (16 또는 32바이트). 없으면 NULL. 다음 호출 전까지만 유효 */
const unsigned char *font_glyph_m(unsigned short ch, int hangul_wide, int *wide);
const unsigned char *font_glyph(unsigned short ch, int *wide);

/* 그리기 전에 필요한 글자를 알려 두고(font_want) 한꺼번에 읽음(font_fetch) */
void font_want(unsigned short ch, int hangul_wide);
void font_fetch(void);

#endif
