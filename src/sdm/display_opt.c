#include "edit.h"

static const char *COLOR_NAMES[16] = {
    "검정", "파랑", "녹색", "청록", "빨강", "보라", "갈색", "회색",
    "재색", "하늘색", "연두", "옥색", "주황", "분홍", "노랑", "흰색"
};

/* 원본 5086:0000 화면 표시 옵션: x=10, y=2, w=60, h=20 */
void sdm_run_display_opt(void) {
    const int x = 10, y = 2, w = 60, h = 20;
    int fg = g_ed.fg_color & 0x0F;
    int bg = g_ed.bg_color & 0x07;
    int fg_top = 0, bg_top = 0;
    bool scbar = g_ed.show_scrollbar;
    int tabs = g_ed.tab_stops;
    int focus = 0; /* 0:문자색, 1:바탕색, 2:스크롤바, 3:탭간격, 4:확인, 5:취소, 6:도움말 */

    for (;;) {
        int cx, cy;
        render_base_screen(&cx, &cy);
        render_status_bar("F1=도움말  Enter=실행  ESC=취소  TAB=다음항목  화살표=선택", false);

        scr_fill(x, y, w, h, " ", 0, 7);
        scr_box(x, y, w, h, true, 0, 7);
        scr_shadow(x, y, w, h);
        scr_puts(x + 23, y, " 화면 표시 ", 7, 0, 14);

        /* 상단 그룹 박스: " 화면 색상 " (5086:011e) */
        scr_box(x + 1, y + 1, 58, 12, false, 0, 7);
        scr_puts(x + 3, y + 1, " 화면 색상 ", 0, 7, 13);

        /* 좌측 색상 미리보기 */
        scr_fill(x + 3, y + 3, 24, 7, " ", (uint8_t)fg, (uint8_t)bg);
        scr_puts(x + 5, y + 5, "1. 텍스트", (uint8_t)fg, (uint8_t)bg, 20);
        scr_puts(x + 5, y + 7, "한글 MS-DOS 편집기", (uint8_t)fg, (uint8_t)bg, 20);

        /* F/문자색 (5086:0139) */
        scr_puts_hotkey(x + 30, y + 2,  "F/문자색",     true, 0, 7, 15, 11);
        scr_box(x + 30, y + 3, 11, 9, false, 0, 7);
        if (fg < fg_top) fg_top = fg;
        if (fg >= fg_top + 7) fg_top = fg - 6;
        for (int i = 0; i < 7 && fg_top + i < 16; ++i) {
            int idx = fg_top + i;
            bool sel = (idx == fg);
            uint8_t lfg = (focus == 0 && sel) ? 15 : 0;
            uint8_t lbg = (focus == 0 && sel) ? 1 : (sel ? 3 : 7);
            scr_fill(x + 31, y + 4 + i, 9, 1, " ", lfg, lbg);
            scr_puts(x + 32, y + 4 + i, COLOR_NAMES[idx], lfg, lbg, 7);
        }

        /* B/바탕색 (5086:0144) */
        scr_puts_hotkey(x + 43, y + 2,  "B/바탕색",     true, 0, 7, 15, 11);
        scr_box(x + 43, y + 3, 11, 9, false, 0, 7);
        if (bg < bg_top) bg_top = bg;
        if (bg >= bg_top + 7) bg_top = bg - 6;
        for (int i = 0; i < 7 && bg_top + i < 8; ++i) {
            int idx = bg_top + i;
            bool sel = (idx == bg);
            uint8_t lfg = (focus == 1 && sel) ? 15 : 0;
            uint8_t lbg = (focus == 1 && sel) ? 1 : (sel ? 3 : 7);
            scr_fill(x + 44, y + 4 + i, 9, 1, " ", lfg, lbg);
            scr_puts(x + 45, y + 4 + i, COLOR_NAMES[idx], lfg, lbg, 7);
        }

        /* 하단 그룹 박스: " 창의 상태 " (5086:014f) */
        scr_box(x + 1, y + 13, 58, 3, false, 0, 7);
        scr_puts(x + 3, y + 13, " 창의 상태 ", 0, 7, 13);

        /* S/스크롤바 (5086:015b) & T/탭의 간격: (5086:0168) */
        uint8_t s_fg = (focus == 2) ? 15 : 0;
        uint8_t s_bg = (focus == 2) ? 1  : 7;
        scr_puts(x + 4, y + 14, scbar ? "[X] " : "[ ] ", s_fg, s_bg, 4);
        scr_puts_hotkey(x + 8, y + 14, "S/스크롤바", true, s_fg, s_bg, 15, 14);

        scr_puts_hotkey(x + 33, y + 14, "T/탭의 간격:", true, 0, 7, 15, 15);
        char tbuf[8];
        snprintf(tbuf, sizeof(tbuf), "%2d", tabs);
        scr_puts(x + 49, y + 14, tbuf, focus == 3 ? 15 : 0, focus == 3 ? 1 : 7, 3);

        /* 확인 / 취소 / H/도움말 (5086:0177) */
        scr_puts(x + 9,  y + 17, "<  확인  >",   focus == 4 ? 15 : 0, focus == 4 ? 0 : 7, 12);
        scr_puts(x + 23, y + 17, "<  취소  >",   focus == 5 ? 15 : 0, focus == 5 ? 0 : 7, 12);
        scr_puts_hotkey(x + 37, y + 17, "< H/도움말 >", true, focus == 6 ? 15 : 0, focus == 6 ? 0 : 7, 15, 14);
        int cur_x = x + 32, cur_y = y + 4 + (fg - fg_top);
        if (focus == 1)      { cur_x = x + 45; cur_y = y + 4 + (bg - bg_top); }
        else if (focus == 2) { cur_x = x + 5;  cur_y = y + 14; }
        else if (focus == 3) { cur_x = x + 50; cur_y = y + 14; }
        else if (focus == 4) { cur_x = x + 12; cur_y = y + 17; }
        else if (focus == 5) { cur_x = x + 26; cur_y = y + 17; }
        else if (focus == 6) { cur_x = x + 39; cur_y = y + 17; }
        term_flush(cur_x, cur_y, true);

        KeyEvent k = term_read_key();
        if (k.type == KEY_ESC) return;
        if (k.type == KEY_TAB)       { focus = (focus + 1) % 7; continue; }
        if (k.type == KEY_SHIFT_TAB) { focus = (focus + 6) % 7; continue; }

        /* 모달 내 Alt 단축키 (원본 5086:0139~0177 일치) */
        if (k.type == KEY_ALT_CHAR) {
            if (k.ch == 'F') { focus = 0; continue; }
            if (k.ch == 'B') { focus = 1; continue; }
            if (k.ch == 'S') { focus = 2; scbar = !scbar; continue; }
            if (k.ch == 'T') { focus = 3; continue; }
            if (k.ch == 'H') { sdm_run_welcome(); continue; }
        }

        if (focus == 0) {
            if (k.type == KEY_UP && fg > 0) fg--;
            else if (k.type == KEY_DOWN && fg < 15) fg++;
        } else if (focus == 1) {
            if (k.type == KEY_UP && bg > 0) bg--;
            else if (k.type == KEY_DOWN && bg < 7) bg++;
        } else if (focus == 2 && k.type == KEY_CHAR && k.ch == ' ') {
            scbar = !scbar;
        } else if (focus == 3) {
            if (k.type == KEY_UP && tabs < 16) tabs++;
            else if (k.type == KEY_DOWN && tabs > 1) tabs--;
            else if (k.type == KEY_CHAR && k.ch >= '1' && k.ch <= '9') tabs = (int)(k.ch - '0');
        }
        if (k.type == KEY_ENTER) {
            if (focus == 5) return;
            if (focus == 6) { sdm_run_welcome(); continue; }
            g_ed.fg_color = (uint8_t)fg;
            g_ed.bg_color = (uint8_t)bg;
            g_ed.show_scrollbar = scbar;
            g_ed.tab_stops = tabs;
            return;
        }
    }
}