#include "edit.h"
#ifndef _WIN32
  #include <sys/ioctl.h>
#endif

/* 표준 VGA 16색 DAC RGB (CGA Blue 1 = #0000AA) */
static const uint8_t VGA_RGB[16][3] = {
    {  0,   0,   0}, {  0,   0, 170}, {  0, 170,   0}, {  0, 170, 170},
    {170,   0,   0}, {170,   0, 170}, {170,  85,   0}, {170, 170, 170},
    { 85,  85,  85}, { 85,  85, 255}, { 85, 255,  85}, { 85, 255, 255},
    {255,  85,  85}, {255,  85, 255}, {255, 255,  85}, {255, 255, 255}
};

void cp_init(void) {
    memset(&g_cp, 0, sizeof(g_cp));
    setlocale(LC_ALL, "");
#ifdef _WIN32
    g_cp.cp = GetConsoleOutputCP();
    if (g_cp.cp == 0) g_cp.cp = GetACP();
    snprintf(g_cp.codeset, sizeof(g_cp.codeset), "CP%u", (unsigned)g_cp.cp);
    g_cp.is_utf8 = (g_cp.cp == CP_UTF8);
    g_cp.is_dbcs = (g_cp.cp == 949 || g_cp.cp == 932 || g_cp.cp == 936 || g_cp.cp == 950);
#else
    const char *cs = nl_langinfo(CODESET);
    snprintf(g_cp.codeset, sizeof(g_cp.codeset), "%s", cs ? cs : "UTF-8");
    for (char *p = g_cp.codeset; *p; ++p) *p = (char)toupper((unsigned char)*p);
    g_cp.is_utf8 = (strstr(g_cp.codeset, "UTF-8") != NULL || strstr(g_cp.codeset, "UTF8") != NULL);
    g_cp.is_dbcs = (strstr(g_cp.codeset, "EUC-KR") != NULL || strstr(g_cp.codeset, "CP949") != NULL ||
                    strstr(g_cp.codeset, "UHC") != NULL || strstr(g_cp.codeset, "949") != NULL);
    if (!g_cp.is_utf8) {
        char target[80];
        snprintf(target, sizeof(target), "%s//TRANSLIT", g_cp.codeset);
        g_cp.cd_from_utf8 = iconv_open(target, "UTF-8");
        if (g_cp.cd_from_utf8 == (iconv_t)-1) g_cp.cd_from_utf8 = iconv_open("ASCII//TRANSLIT", "UTF-8");
        g_cp.cd_to_utf8 = iconv_open("UTF-8", g_cp.codeset);
    } else {
        g_cp.cd_from_utf8 = (iconv_t)-1;
        g_cp.cd_to_utf8   = (iconv_t)-1;
    }
#endif
}

size_t utf8_to_sys(const char *src, char *dst, size_t dst_cap) {
    if (!src || dst_cap == 0) return 0;
    if (g_cp.is_utf8) {
        size_t n = strlen(src);
        if (n >= dst_cap) n = dst_cap - 1;
        memcpy(dst, src, n);
        dst[n] = '\0';
        return n;
    }
#ifdef _WIN32
    int wlen = MultiByteToWideChar(CP_UTF8, 0, src, -1, NULL, 0);
    if (wlen <= 0) { dst[0] = '\0'; return 0; }
    wchar_t *wb = (wchar_t *)malloc((size_t)wlen * sizeof(wchar_t));
    if (!wb) { dst[0] = '\0'; return 0; }
    MultiByteToWideChar(CP_UTF8, 0, src, -1, wb, wlen);
    int mlen = WideCharToMultiByte(g_cp.cp, 0, wb, -1, dst, (int)dst_cap, NULL, NULL);
    free(wb);
    if (mlen <= 0) { dst[0] = '\0'; return 0; }
    return (size_t)(mlen - 1);
#else
    if (g_cp.cd_from_utf8 == (iconv_t)-1) {
        size_t n = strlen(src);
        if (n >= dst_cap) n = dst_cap - 1;
        memcpy(dst, src, n);
        dst[n] = '\0';
        return n;
    }
    iconv(g_cp.cd_from_utf8, NULL, NULL, NULL, NULL);
    char *inb = (char *)src, *outb = dst;
    size_t inl = strlen(src), outl = dst_cap - 1;
    iconv(g_cp.cd_from_utf8, &inb, &inl, &outb, &outl);
    *outb = '\0';
    return (size_t)(outb - dst);
#endif
}

void sys_to_utf8(const char *src, size_t src_len, char *dst, size_t dst_cap) {
    if (!src || dst_cap == 0) return;
    if (g_cp.is_utf8) {
        size_t n = src_len < dst_cap - 1 ? src_len : dst_cap - 1;
        memcpy(dst, src, n);
        dst[n] = '\0';
        return;
    }
#ifdef _WIN32
    int wlen = MultiByteToWideChar(g_cp.cp, 0, src, (int)src_len, NULL, 0);
    if (wlen <= 0) { dst[0] = '\0'; return; }
    wchar_t *wb = (wchar_t *)malloc((size_t)wlen * sizeof(wchar_t));
    if (!wb) { dst[0] = '\0'; return; }
    MultiByteToWideChar(g_cp.cp, 0, src, (int)src_len, wb, wlen);
    int ulen = WideCharToMultiByte(CP_UTF8, 0, wb, wlen, dst, (int)dst_cap - 1, NULL, NULL);
    free(wb);
    dst[ulen > 0 ? ulen : 0] = '\0';
#else
    if (g_cp.cd_to_utf8 == (iconv_t)-1) {
        size_t n = src_len < dst_cap - 1 ? src_len : dst_cap - 1;
        memcpy(dst, src, n);
        dst[n] = '\0';
        return;
    }
    iconv(g_cp.cd_to_utf8, NULL, NULL, NULL, NULL);
    char *inb = (char *)src, *outb = dst;
    size_t inl = src_len, outl = dst_cap - 1;
    iconv(g_cp.cd_to_utf8, &inb, &inl, &outb, &outl);
    *outb = '\0';
#endif
}

#ifdef _WIN32
static HANDLE g_hIn, g_hOut;
static DWORD  g_orig_in_mode, g_orig_out_mode;

void term_init(void) {
    g_hIn  = GetStdHandle(STD_INPUT_HANDLE);
    g_hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    GetConsoleMode(g_hIn, &g_orig_in_mode);
    GetConsoleMode(g_hOut, &g_orig_out_mode);
    SetConsoleMode(g_hIn, ENABLE_WINDOW_INPUT | ENABLE_EXTENDED_FLAGS);

    /* 항상 80x25로 초기화 (버퍼와 창 크기를 일치시켜 스크롤백 제거) */
    SMALL_RECT tiny = { 0, 0, 1, 1 };
    SetConsoleWindowInfo(g_hOut, TRUE, &tiny);
    COORD sz = { SCREEN_INIT_COLS, SCREEN_INIT_ROWS };
    SetConsoleScreenBufferSize(g_hOut, sz);
    SMALL_RECT rc = { 0, 0, SCREEN_INIT_COLS - 1, SCREEN_INIT_ROWS - 1 };
    SetConsoleWindowInfo(g_hOut, TRUE, &rc);
    scr_init(SCREEN_INIT_COLS, SCREEN_INIT_ROWS);
}

void term_cleanup(void) {
    SetConsoleMode(g_hIn, g_orig_in_mode);
    SetConsoleMode(g_hOut, g_orig_out_mode);
    SetConsoleTextAttribute(g_hOut, 7);
}

void term_flush(int cur_x, int cur_y, bool show_cursor) {
    int cols = g_scr.cols, rows = g_scr.rows;
    CHAR_INFO *ci = (CHAR_INFO *)calloc((size_t)cols * (size_t)rows, sizeof(CHAR_INFO));
    if (!ci) return;
    for (int i = 0; i < cols * rows; ++i) {
        ScreenCell *sc = &g_scr.cells[i];
        ci[i].Attributes = (WORD)((sc->bg << 4) | (sc->fg & 0x0F));
        if (sc->kind == CELL_TRAIL) {
            ci[i].Char.UnicodeChar = L' ';
            ci[i].Attributes |= COMMON_LVB_TRAILING_BYTE;
        } else {
            wchar_t wch[4] = { L' ', 0 };
            MultiByteToWideChar(CP_UTF8, 0, sc->utf8, -1, wch, 4);
            ci[i].Char.UnicodeChar = wch[0] ? wch[0] : L' ';
            if (sc->kind == CELL_LEAD) ci[i].Attributes |= COMMON_LVB_LEADING_BYTE;
        }
    }
    COORD buf_sz = { (SHORT)cols, (SHORT)rows }, zero = { 0, 0 };
    SMALL_RECT rc = { 0, 0, (SHORT)(cols - 1), (SHORT)(rows - 1) };
    WriteConsoleOutputW(g_hOut, ci, buf_sz, zero, &rc);
    free(ci);

    CONSOLE_CURSOR_INFO cci = { 15, show_cursor ? TRUE : FALSE };
    SetConsoleCursorInfo(g_hOut, &cci);
    COORD cpos = { (SHORT)cur_x, (SHORT)cur_y };
    SetConsoleCursorPosition(g_hOut, cpos);
}

KeyEvent term_read_key(void) {
    KeyEvent ev = { KEY_NONE, 0 };
    static bool alt_down_clean = false;
    INPUT_RECORD ir;
    DWORD nr = 0;
    while (ReadConsoleInputW(g_hIn, &ir, 1, &nr)) {
        if (ir.EventType != KEY_EVENT) continue;
        KEY_EVENT_RECORD *k = &ir.Event.KeyEvent;
        bool alt   = (k->dwControlKeyState & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED)) != 0;
        bool ctrl  = (k->dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) != 0;
        bool shift = (k->dwControlKeyState & SHIFT_PRESSED) != 0;

        if (!k->bKeyDown) {
            if (k->wVirtualKeyCode == VK_MENU && alt_down_clean) {
                alt_down_clean = false;
                ev.type = KEY_ALT_ONLY;
                return ev;
            }
            continue;
        }
        if (k->wVirtualKeyCode == VK_MENU) { alt_down_clean = true; continue; }
        alt_down_clean = false;

        switch (k->wVirtualKeyCode) {
            case VK_RETURN: ev.type = KEY_ENTER; return ev;
            case VK_ESCAPE: ev.type = KEY_ESC;   return ev;
            case VK_BACK:   ev.type = KEY_BS;    return ev;
            case VK_TAB:    ev.type = shift ? KEY_SHIFT_TAB : KEY_TAB; return ev;
            case VK_UP:     ev.type = KEY_UP;    return ev;
            case VK_DOWN:   ev.type = KEY_DOWN;  return ev;
            case VK_LEFT:   ev.type = KEY_LEFT;  return ev;
            case VK_RIGHT:  ev.type = KEY_RIGHT; return ev;
            case VK_HOME:   ev.type = KEY_HOME;  return ev;
            case VK_END:    ev.type = KEY_END;   return ev;
            case VK_PRIOR:  ev.type = KEY_PGUP;  return ev;
            case VK_NEXT:   ev.type = KEY_PGDN;  return ev;
            case VK_F1:     ev.type = KEY_F1;    return ev;
            case VK_F3:     ev.type = KEY_F3;    return ev;
            case VK_INSERT: ev.type = ctrl ? KEY_CTRL_INS : (shift ? KEY_SHIFT_INS : KEY_INS); return ev;
            case VK_DELETE: ev.type = shift ? KEY_SHIFT_DEL : KEY_DEL; return ev;
            default: break;
        }
        if (alt && k->wVirtualKeyCode >= 'A' && k->wVirtualKeyCode <= 'Z') {
            ev.type = KEY_ALT_CHAR; ev.ch = (uint32_t)k->wVirtualKeyCode; return ev;
        }
        if (ctrl && k->wVirtualKeyCode >= 'A' && k->wVirtualKeyCode <= 'Z') {
            ev.type = KEY_CTRL_CHAR; ev.ch = (uint32_t)k->wVirtualKeyCode; return ev;
        }
        if (k->uChar.UnicodeChar >= 0x20) {
            ev.type = KEY_CHAR; ev.ch = (uint32_t)k->uChar.UnicodeChar; return ev;
        }
    }
    return ev;
}

#else /* Linux / POSIX */

static struct termios g_orig_termios;

void term_init(void) {
    tcgetattr(STDIN_FILENO, &g_orig_termios);
    struct termios raw = g_orig_termios;
    raw.c_iflag &= (tcflag_t)~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= (tcflag_t)~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= (tcflag_t)~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN]  = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);

    /* 대체 화면 진입(?1049h) + 자동 줄바꿈 해제(?7l) + 80x25 요청 */
    const char *init_seq =
        "\x1b[?1049h"
        "\x1b[?7l"
        "\x1b[8;25;80t"
        "\x1b[0m\x1b[2J\x1b[H";
    ssize_t u = write(STDOUT_FILENO, init_seq, strlen(init_seq));
    (void)u;
    scr_init(SCREEN_INIT_COLS, SCREEN_INIT_ROWS);
}

void term_cleanup(void) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_orig_termios);
    /* 자동 줄바꿈 복원(?7h) + 기본 화면 복귀(?1049l) */
    const char *rst = "\x1b[0m\x1b[?25h\x1b[?7h\x1b[?1049l";
    ssize_t u = write(STDOUT_FILENO, rst, strlen(rst));
    (void)u;
}

void term_flush(int cur_x, int cur_y, bool show_cursor) {
    struct winsize ws;
    int max_r = g_scr.rows, max_c = g_scr.cols;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_row > 0 && ws.ws_col > 0) {
        if (ws.ws_row < max_r) max_r = ws.ws_row;
        if (ws.ws_col < max_c) max_c = ws.ws_col;
    }

    char out[65536];
    size_t pos = 0;
    uint8_t last_fg = 0xFF, last_bg = 0xFF;

    pos += (size_t)snprintf(out + pos, sizeof(out) - pos, "\x1b[?25l");
    for (int y = 0; y < max_r; ++y) {
        bool moved = false;
        for (int x = 0; x < max_c; ++x) {
            int idx = y * g_scr.cols + x;
            ScreenCell *sc = &g_scr.cells[idx];
            ScreenCell *pv = &g_scr.prev[idx];
            /* 우측 하단 끝 셀 출력 시 터미널 강제 스크롤 방지 */
            if (y == max_r - 1 && x == max_c - 1) { *pv = *sc; moved = false; continue; }
            if (sc->kind == CELL_TRAIL) { *pv = *sc; moved = false; continue; }
            if (memcmp(sc, pv, sizeof(ScreenCell)) == 0) { moved = false; continue; }
            if (pos > sizeof(out) - 160) {
                ssize_t w = write(STDOUT_FILENO, out, pos); (void)w; pos = 0;
            }
            if (!moved) {
                pos += (size_t)snprintf(out + pos, sizeof(out) - pos, "\x1b[%d;%dH", y + 1, x + 1);
            }
            if (sc->fg != last_fg || sc->bg != last_bg) {
                const uint8_t *f = VGA_RGB[sc->fg & 0x0F];
                const uint8_t *b = VGA_RGB[sc->bg & 0x07];
                pos += (size_t)snprintf(out + pos, sizeof(out) - pos,
                                        "\x1b[38;2;%u;%u;%um\x1b[48;2;%u;%u;%um",
                                        f[0], f[1], f[2], b[0], b[1], b[2]);
                last_fg = sc->fg;
                last_bg = sc->bg;
            }
            char sys_ch[16];
            size_t slen = utf8_to_sys(sc->utf8, sys_ch, sizeof(sys_ch));
            if (slen == 0) { sys_ch[0] = ' '; slen = 1; }
            memcpy(out + pos, sys_ch, slen);
            pos += slen;
            *pv = *sc;
            moved = true;
        }
    }
    if (show_cursor) {
        if (cur_x >= max_c) cur_x = max_c - 1;
        if (cur_y >= max_r) cur_y = max_r - 1;
        pos += (size_t)snprintf(out + pos, sizeof(out) - pos, "\x1b[%d;%dH\x1b[?25h", cur_y + 1, cur_x + 1);
    }
    ssize_t w = write(STDOUT_FILENO, out, pos);
    (void)w;
}

static bool stdin_has_byte(int timeout_ms) {
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(STDIN_FILENO, &rfds);
    struct timeval tv = { timeout_ms / 1000, (timeout_ms % 1000) * 1000 };
    return select(STDIN_FILENO + 1, &rfds, NULL, NULL, &tv) > 0;
}

KeyEvent term_read_key(void) {
    KeyEvent ev = { KEY_NONE, 0 };
    unsigned char b0 = 0;
    if (read(STDIN_FILENO, &b0, 1) <= 0) return ev;

    if (b0 == '\r' || b0 == '\n') { ev.type = KEY_ENTER; return ev; }
    if (b0 == 0x7F || b0 == 0x08) { ev.type = KEY_BS;    return ev; }
    if (b0 == '\t')               { ev.type = KEY_TAB;   return ev; }

    if (b0 == 0x1B) {
        if (!stdin_has_byte(35)) { ev.type = KEY_ESC; return ev; }
        unsigned char b1 = 0;
        if (read(STDIN_FILENO, &b1, 1) <= 0) { ev.type = KEY_ESC; return ev; }
        if (b1 == '[') {
            char seq[16] = {0};
            int si = 0;
            while (si < 14 && stdin_has_byte(15)) {
                unsigned char c = 0;
                if (read(STDIN_FILENO, &c, 1) <= 0) break;
                seq[si++] = (char)c;
                if ((c >= 'A' && c <= 'Z') || c == '~') break;
            }
            if (strcmp(seq, "A") == 0)      { ev.type = KEY_UP;        return ev; }
            if (strcmp(seq, "B") == 0)      { ev.type = KEY_DOWN;      return ev; }
            if (strcmp(seq, "C") == 0)      { ev.type = KEY_RIGHT;     return ev; }
            if (strcmp(seq, "D") == 0)      { ev.type = KEY_LEFT;      return ev; }
            if (strcmp(seq, "Z") == 0)      { ev.type = KEY_SHIFT_TAB; return ev; }
            if (strcmp(seq, "H") == 0 || strcmp(seq, "1~") == 0) { ev.type = KEY_HOME; return ev; }
            if (strcmp(seq, "F") == 0 || strcmp(seq, "4~") == 0) { ev.type = KEY_END;  return ev; }
            if (strcmp(seq, "5~") == 0)     { ev.type = KEY_PGUP;      return ev; }
            if (strcmp(seq, "6~") == 0)     { ev.type = KEY_PGDN;      return ev; }
            if (strcmp(seq, "2~") == 0)     { ev.type = KEY_INS;       return ev; }
            if (strcmp(seq, "2;5~") == 0)   { ev.type = KEY_CTRL_INS;  return ev; }
            if (strcmp(seq, "2;2~") == 0)   { ev.type = KEY_SHIFT_INS; return ev; }
            if (strcmp(seq, "3~") == 0)     { ev.type = KEY_DEL;       return ev; }
            if (strcmp(seq, "3;2~") == 0)   { ev.type = KEY_SHIFT_DEL; return ev; }
            if (strcmp(seq, "13~") == 0)    { ev.type = KEY_F3;        return ev; }
            return ev;
        }
        if (b1 == 'O') {
            unsigned char b2 = 0;
            if (stdin_has_byte(15) && read(STDIN_FILENO, &b2, 1) > 0) {
                if (b2 == 'P') { ev.type = KEY_F1; return ev; }
                if (b2 == 'R') { ev.type = KEY_F3; return ev; }
            }
            return ev;
        }
        if (isalpha(b1)) {
            ev.type = KEY_ALT_CHAR;
            ev.ch = (uint32_t)toupper(b1);
            return ev;
        }
        ev.type = KEY_ESC;
        return ev;
    }

    if (b0 < 0x20) { ev.type = KEY_CTRL_CHAR; ev.ch = (uint32_t)('A' + b0 - 1); return ev; }

    char raw_in[8] = { (char)b0, 0 };
    int rlen = 1;
    if (g_cp.is_utf8) {
        if ((b0 & 0xE0) == 0xC0) rlen = 2;
        else if ((b0 & 0xF0) == 0xE0) rlen = 3;
        else if ((b0 & 0xF8) == 0xF0) rlen = 4;
        for (int i = 1; i < rlen && stdin_has_byte(10); ++i) {
            if (read(STDIN_FILENO, &raw_in[i], 1) <= 0) break;
        }
        char tmp[8];
        utf8_next_cp(raw_in, &ev.ch, tmp);
        ev.type = KEY_CHAR;
        return ev;
    } else if (g_cp.is_dbcs && b0 >= 0x81) {
        if (stdin_has_byte(15) && read(STDIN_FILENO, &raw_in[1], 1) > 0) rlen = 2;
        char u8[8] = {0};
        sys_to_utf8(raw_in, (size_t)rlen, u8, sizeof(u8));
        char tmp[8];
        utf8_next_cp(u8, &ev.ch, tmp);
        ev.type = KEY_CHAR;
        return ev;
    }
    ev.type = KEY_CHAR;
    ev.ch = (uint32_t)b0;
    return ev;
}
#endif
