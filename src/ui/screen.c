#include "edit.h"

void scr_init(int cols, int rows) {
    free(g_scr.cells);
    free(g_scr.prev);
    g_scr.cols = cols;
    g_scr.rows = rows;
    size_t cnt = (size_t)cols * (size_t)rows;
    g_scr.cells = (ScreenCell *)calloc(cnt, sizeof(ScreenCell));
    g_scr.prev  = (ScreenCell *)calloc(cnt, sizeof(ScreenCell));
    for (size_t i = 0; i < cnt; ++i) {
        strcpy(g_scr.cells[i].utf8, " ");
        g_scr.cells[i].fg = 7;
        g_scr.cells[i].bg = 1;
        g_scr.prev[i].fg  = 0xFF;
    }
}

int utf8_next_cp(const char *s, uint32_t *cp_out, char ch_out[8]) {
    const unsigned char *p = (const unsigned char *)s;
    if (!p[0]) return 0;
    int len = 1;
    uint32_t cp = p[0];
    if ((p[0] & 0x80) == 0) { len = 1; cp = p[0]; }
    else if ((p[0] & 0xE0) == 0xC0 && (p[1] & 0xC0) == 0x80) {
        len = 2; cp = ((p[0] & 0x1F) << 6) | (p[1] & 0x3F);
    } else if ((p[0] & 0xF0) == 0xE0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80) {
        len = 3; cp = ((p[0] & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
    } else if ((p[0] & 0xF8) == 0xF0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80 && (p[3] & 0xC0) == 0x80) {
        len = 4; cp = ((p[0] & 0x07) << 18) | ((p[1] & 0x3F) << 12) | ((p[2] & 0x3F) << 6) | (p[3] & 0x3F);
    }
    memcpy(ch_out, s, (size_t)len);
    ch_out[len] = '\0';
    *cp_out = cp;
    return len;
}

void cp_to_utf8(uint32_t cp, char out[8]) {
    if (cp < 0x80) { out[0] = (char)cp; out[1] = '\0'; }
    else if (cp < 0x800) {
        out[0] = (char)(0xC0 | (cp >> 6)); out[1] = (char)(0x80 | (cp & 0x3F)); out[2] = '\0';
    } else if (cp < 0x10000) {
        out[0] = (char)(0xE0 | (cp >> 12)); out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (char)(0x80 | (cp & 0x3F)); out[3] = '\0';
    } else {
        out[0] = (char)(0xF0 | (cp >> 18)); out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
        out[2] = (char)(0x80 | ((cp >> 6) & 0x3F)); out[3] = (char)(0x80 | (cp & 0x3F)); out[4] = '\0';
    }
}

int cp_cell_width(uint32_t cp) {
    if (cp == 0) return 0;
    if ((cp >= 0x1100 && cp <= 0x115F) || (cp >= 0x2E80 && cp <= 0xA4CF && cp != 0x303F) ||
        (cp >= 0xAC00 && cp <= 0xD7A3) || (cp >= 0xF900 && cp <= 0xFAFF) ||
        (cp >= 0xFE10 && cp <= 0xFE6F) || (cp >= 0xFF01 && cp <= 0xFF60) ||
        (cp >= 0xFFE0 && cp <= 0xFFE6)) return 2;
    return 1;
}

int utf8_display_width(const char *s) {
    int w = 0;
    for (const char *p = s; *p; ) {
        uint32_t cp; char ch[8];
        int adv = utf8_next_cp(p, &cp, ch);
        w += cp_cell_width(cp);
        p += adv;
    }
    return w;
}

static void scr_break_dbcs_at(int x, int y) {
    if (x < 0 || x >= g_scr.cols || y < 0 || y >= g_scr.rows) return;
    ScreenCell *c = &g_scr.cells[y * g_scr.cols + x];
    if (c->kind == CELL_LEAD && x + 1 < g_scr.cols) {
        strcpy(g_scr.cells[y * g_scr.cols + x + 1].utf8, " ");
        g_scr.cells[y * g_scr.cols + x + 1].kind = CELL_NORMAL;
    } else if (c->kind == CELL_TRAIL && x - 1 >= 0) {
        strcpy(g_scr.cells[y * g_scr.cols + x - 1].utf8, " ");
        g_scr.cells[y * g_scr.cols + x - 1].kind = CELL_NORMAL;
    }
    strcpy(c->utf8, " ");
    c->kind = CELL_NORMAL;
}

void scr_put_cell(int x, int y, const char *utf8_ch, int width, uint8_t fg, uint8_t bg) {
    if (y < 0 || y >= g_scr.rows || x < 0 || x >= g_scr.cols) return;
    if (width == 2 && x + 1 >= g_scr.cols) {
        scr_break_dbcs_at(x, y);
        g_scr.cells[y * g_scr.cols + x].fg = fg;
        g_scr.cells[y * g_scr.cols + x].bg = bg;
        return;
    }
    scr_break_dbcs_at(x, y);
    ScreenCell *c = &g_scr.cells[y * g_scr.cols + x];
    snprintf(c->utf8, sizeof(c->utf8), "%s", utf8_ch);
    c->fg = fg; c->bg = bg;
    if (width == 2) {
        scr_break_dbcs_at(x + 1, y);
        c->kind = CELL_LEAD;
        ScreenCell *t = &g_scr.cells[y * g_scr.cols + x + 1];
        t->utf8[0] = '\0'; t->fg = fg; t->bg = bg; t->kind = CELL_TRAIL;
    } else {
        c->kind = CELL_NORMAL;
    }
}

int scr_puts(int x, int y, const char *utf8_str, uint8_t fg, uint8_t bg, int max_w) {
    int cx = x;
    for (const char *p = utf8_str; *p; ) {
        uint32_t cp; char ch[8];
        int adv = utf8_next_cp(p, &cp, ch);
        int w = cp_cell_width(cp);
        if (max_w > 0 && (cx - x) + w > max_w) break;
        if (cx >= g_scr.cols) break;
        scr_put_cell(cx, y, ch, w, fg, bg);
        cx += w; p += adv;
    }
    return cx - x;
}

/* 단축키 알파벳 1글자 백색 강조 함수 */
int scr_puts_hotkey(int x, int y, const char *utf8_str, bool highlight_hk,
                    uint8_t fg, uint8_t bg, uint8_t hk_fg, int max_w) {
    int cx = x;
    bool done_hk = !highlight_hk;
    for (const char *p = utf8_str; *p; ) {
        uint32_t cp; char ch[8];
        int adv = utf8_next_cp(p, &cp, ch);
        int w = cp_cell_width(cp);
        if (max_w > 0 && (cx - x) + w > max_w) break;
        if (cx >= g_scr.cols) break;
        uint8_t use_fg = fg;
        if (!done_hk && cp < 0x80 && isalnum((int)cp)) {
            use_fg = hk_fg;
            done_hk = true;
        }
        scr_put_cell(cx, y, ch, w, use_fg, bg);
        cx += w; p += adv;
    }
    return cx - x;
}

void scr_fill(int x, int y, int w, int h, const char *ch, uint8_t fg, uint8_t bg) {
    for (int r = y; r < y + h; ++r)
        for (int c = x; c < x + w; ++c)
            scr_put_cell(c, r, ch, 1, fg, bg);
}

void scr_box(int x, int y, int w, int h, bool double_line, uint8_t fg, uint8_t bg) {
    if (w < 2 || h < 2) return;
    const char *tl = g_cp.is_utf8 ? (double_line ? "╔" : "┌") : "+";
    const char *tr = g_cp.is_utf8 ? (double_line ? "╗" : "┐") : "+";
    const char *bl = g_cp.is_utf8 ? (double_line ? "╚" : "└") : "+";
    const char *br = g_cp.is_utf8 ? (double_line ? "╝" : "┘") : "+";
    const char *hz = g_cp.is_utf8 ? (double_line ? "═" : "─") : (double_line ? "=" : "-");
    const char *vt = g_cp.is_utf8 ? (double_line ? "║" : "│") : "|";
    scr_put_cell(x, y, tl, 1, fg, bg);
    scr_put_cell(x + w - 1, y, tr, 1, fg, bg);
    scr_put_cell(x, y + h - 1, bl, 1, fg, bg);
    scr_put_cell(x + w - 1, y + h - 1, br, 1, fg, bg);
    for (int c = x + 1; c < x + w - 1; ++c) {
        scr_put_cell(c, y, hz, 1, fg, bg);
        scr_put_cell(c, y + h - 1, hz, 1, fg, bg);
    }
    for (int r = y + 1; r < y + h - 1; ++r) {
        scr_put_cell(x, r, vt, 1, fg, bg);
        scr_put_cell(x + w - 1, r, vt, 1, fg, bg);
    }
}

void scr_shadow(int x, int y, int w, int h) {
    for (int r = y + 1; r <= y + h; ++r) {
        for (int c = x + w; c < x + w + 2; ++c) {
            if (c >= 0 && c < g_scr.cols && r >= 0 && r < g_scr.rows) {
                scr_break_dbcs_at(c, r);
                g_scr.cells[r * g_scr.cols + c].fg = 8;
                g_scr.cells[r * g_scr.cols + c].bg = 0;
            }
        }
    }
    for (int c = x + 2; c <= x + w + 1; ++c) {
        int r = y + h;
        if (c >= 0 && c < g_scr.cols && r >= 0 && r < g_scr.rows) {
            scr_break_dbcs_at(c, r);
            g_scr.cells[r * g_scr.cols + c].fg = 8;
            g_scr.cells[r * g_scr.cols + c].bg = 0;
        }
    }
}
