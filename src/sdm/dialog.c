#include "edit.h"
#ifndef _WIN32
  #include <dirent.h>
  #include <sys/stat.h>
#endif

static void sdm_center(int w, int h, int *ox, int *oy) {
    *ox = (g_scr.cols - w) / 2;
    *oy = (g_scr.rows - h) / 2;
    if (*ox < 1) *ox = 1;
    if (*oy < 1) *oy = 1;
}

static void draw_button(int x, int y, const char *label, bool focused) {
    uint8_t fg = focused ? 15 : 0;
    uint8_t bg = focused ? 0  : 7;
    scr_puts(x, y, label, fg, bg, 16);
}

/* 현재 디렉터리의 하위 폴더 목록 수집 (.. 포함) */
static int list_subdirs(char entries[][64], int max_entries) {
    int cnt = 0;
    if (cnt < max_entries) strcpy(entries[cnt++], "..");
#ifdef _WIN32
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA("*", &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
                strcmp(fd.cFileName, ".") != 0 && strcmp(fd.cFileName, "..") != 0) {
                if (cnt < max_entries) snprintf(entries[cnt++], 64, "%s", fd.cFileName);
            }
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
#else
    DIR *dp = opendir(".");
    if (dp) {
        struct dirent *de;
        while ((de = readdir(dp)) != NULL && cnt < max_entries) {
            if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) continue;
            struct stat st;
            if (stat(de->d_name, &st) == 0 && S_ISDIR(st.st_mode)) {
                snprintf(entries[cnt++], 64, "%s", de->d_name);
            }
        }
        closedir(dp);
    }
#endif
    return cnt;
}

static void compat_chdir_wrap(const char *dir) {
#ifdef _WIN32
    _chdir(dir);
#else
    if (chdir(dir) != 0) { /* ignore */ }
#endif
}

void sdm_run_welcome(void) {
    int w = 58, h = 11, x, y;
    sdm_center(w, h, &x, &y);
    for (;;) {
        int cx, cy;
        render_base_screen(&cx, &cy);
        scr_fill(x, y, w, h, " ", 0, 7);
        scr_box(x, y, w, h, true, 0, 7);
        scr_shadow(x, y, w, h);
        scr_puts(x + 19, y + 1, "한글 MS-DOS 편집기", 0, 7, w - 4);
        scr_puts(x + 7,  y + 3, "저작권자 (C) 1987-1993 (주)마이크로소프트.", 0, 7, w - 8);
        scr_puts(x + 9,  y + 5, "Enter키를 누르면 안내를 볼 수 있습니다.", 0, 7, w - 10);
        scr_puts(x + 9,  y + 7, "Esc키를 누르면 이 대화창이 없어집니다.", 0, 7, w - 10);
        draw_button(x + 23, y + 9, "<  확인  >", true);
        term_flush(x + 26, y + 9, true);

        KeyEvent k = term_read_key();
        if (k.type == KEY_ESC || k.type == KEY_ENTER) break;
    }
}

/* 원본 503a:00aa (다른 이름으로 보관) 좌표: x=19, y=3, w=43, h=18 */
bool sdm_run_file_dialog(const char *title, char *path_buf, size_t cap) {
    const int x = 19, y = 3, w = 43, h = 18;
    int len = (int)strlen(path_buf);
    int focus = 0; /* 0: 파일이름, 1: 목록 리스트, 2: 확인, 3: 취소, 4: 도움말 */

    char dirs[128][64];
    int dir_cnt = list_subdirs(dirs, 128);
    int dir_sel = 0, dir_top = 0;
    char cwd[260] = {0};

    for (;;) {
        if (!compat_getcwd(cwd, sizeof(cwd))) strcpy(cwd, ".");
        int cx, cy;
        render_base_screen(&cx, &cy);

        scr_fill(x, y, w, h, " ", 0, 7);
        scr_box(x, y, w, h, true, 0, 7);
        scr_shadow(x, y, w, h);

        int tw = utf8_display_width(title);
        scr_puts(x + (w - tw) / 2, y, title, 7, 0, w - 4);

        /* 파일이름 라벨 + 회색 배경 단선 테두리 입력 박스 */
        scr_puts_hotkey(x + 1,  y + 2,  "N/파일이름:", true, 0, 7, 15, 11);
        scr_box(x + 13, y + 1, 27, 3, false, 0, 7);
        scr_fill(x + 14, y + 2, 25, 1, " ", 0, 7);
        int drawn = scr_puts(x + 14, y + 2, path_buf, 0, 7, 25);

        /* 현재 경로 (y+4) */
        scr_puts(x + 1, y + 4, cwd, 0, 7, 39);

        /* (14, 4, 13, 1) 목록 라벨 + (13, 5, 16, 9) 디렉터리 리스트박스 */
        scr_puts_hotkey(x + 14, y + 4,  "D/목록:",     true, 0, 7, 15, 13);
        scr_fill(x + 13, y + 5, 16, 9, " ", 0, 7);
        scr_box(x + 13, y + 5, 16, 9, false, 0, 7);
        int vis = 7;
        if (dir_sel < dir_top) dir_top = dir_sel;
        if (dir_sel >= dir_top + vis) dir_top = dir_sel - vis + 1;
        for (int i = 0; i < vis && dir_top + i < dir_cnt; ++i) {
            int idx = dir_top + i;
            bool sel = (focus == 1 && idx == dir_sel);
            uint8_t lfg = sel ? 15 : 0, lbg = sel ? 1 : 7;
            scr_fill(x + 14, y + 6 + i, 14, 1, " ", lfg, lbg);
            scr_puts(x + 15, y + 6 + i, dirs[idx], lfg, lbg, 12);
        }

        /* (3, 15) 확인, (15, 15) 취소, (27, 15) 도움말 */
        draw_button(x + 3,  y + 15, "<  확인  >", focus == 2);
        draw_button(x + 15, y + 15, "<  취소  >", focus == 3);
        scr_puts_hotkey(x + 27, y + 15, "< H/도움말 >", true, focus == 4 ? 15 : 0, focus == 4 ? 0 : 7, 15, 14);

        int cur_x = x + 14 + drawn, cur_y = y + 1;
        if (focus == 1)      { cur_x = x + 15; cur_y = y + 6 + (dir_sel - dir_top); }
        else if (focus == 2) { cur_x = x + 6;  cur_y = y + 15; }
        else if (focus == 3) { cur_x = x + 18; cur_y = y + 15; }
        else if (focus == 4) { cur_x = x + 29; cur_y = y + 15; }
        term_flush(cur_x, cur_y, true);

        KeyEvent k = term_read_key();
        if (k.type == KEY_ESC) return false;
        if (k.type == KEY_TAB)       { focus = (focus + 1) % 5; continue; }
        if (k.type == KEY_SHIFT_TAB) { focus = (focus + 4) % 5; continue; }
        /*Alt 단축키 분기*/
        if (k.type == KEY_ALT_CHAR) {
            if (k.ch == 'N') { focus = 0; continue; }
            if (k.ch == 'D') { focus = 1; continue; }
            if (k.ch == 'H') { sdm_run_welcome(); continue; }
        }

        if (focus == 1) {
            if (k.type == KEY_UP && dir_sel > 0) dir_sel--;
            else if (k.type == KEY_DOWN && dir_sel + 1 < dir_cnt) dir_sel++;
            else if (k.type == KEY_ENTER && dir_cnt > 0) {
                compat_chdir_wrap(dirs[dir_sel]);
                dir_cnt = list_subdirs(dirs, 128);
                dir_sel = dir_top = 0;
            }
            continue;
        }
        if (focus >= 2) {
            if (k.type == KEY_RIGHT) { focus = (focus == 4) ? 2 : focus + 1; continue; }
            if (k.type == KEY_LEFT)  { focus = (focus == 2) ? 4 : focus - 1; continue; }
        }
        if (k.type == KEY_ENTER) {
            if (focus == 0 || focus == 2) return true;
            if (focus == 3) return false;
            if (focus == 4) { sdm_run_welcome(); continue; }
        }
        if (focus == 0) {
            if (k.type == KEY_BS && len > 0) {
                while (len > 0 && ((unsigned char)path_buf[len - 1] & 0xC0) == 0x80) len--;
                if (len > 0) len--;
                path_buf[len] = '\0';
            } else if (k.type == KEY_CHAR && k.ch >= 0x20) {
                char ch[8]; cp_to_utf8(k.ch, ch);
                size_t cl = strlen(ch);
                if ((size_t)len + cl < cap) {
                    memcpy(path_buf + len, ch, cl + 1);
                    len += (int)cl;
                }
            }
        }
    }
}

bool sdm_run_prompt(const char *title, const char *prompt, char *buf, size_t cap) {
    int w = 54, h = 9, x, y;
    sdm_center(w, h, &x, &y);
    int len = (int)strlen(buf);
    int focus = 0;

    for (;;) {
        int cx, cy;
        render_base_screen(&cx, &cy);
        render_status_bar("F1=도움말  Enter=실행  ESC=취소  TAB=다음항목  화살표=선택", false);
        scr_fill(x, y, w, h, " ", 0, 7);
        scr_box(x, y, w, h, true, 0, 7);
        scr_shadow(x, y, w, h);

        int tw = utf8_display_width(title);
        scr_puts(x + (w - tw) / 2, y, title, 7, 0, w - 4);
        scr_puts(x + 3, y + 2, prompt, 0, 7, w - 6);

        uint8_t in_fg = (focus == 0) ? 15 : 7;
        scr_fill(x + 3, y + 3, w - 6, 1, " ", in_fg, 1);
        int drawn = scr_puts(x + 4, y + 3, buf, in_fg, 1, w - 8);

        draw_button(x + 8,  y + 6, "<  확인  >", focus == 1);
        draw_button(x + 22, y + 6, "<  취소  >", focus == 2);
        draw_button(x + 36, y + 6, "< 도움말 >", focus == 3);

        int cur_x = x + 4 + drawn, cur_y = y + 3;
        if (focus == 1)      { cur_x = x + 11; cur_y = y + 6; }
        else if (focus == 2) { cur_x = x + 25; cur_y = y + 6; }
        else if (focus == 3) { cur_x = x + 38; cur_y = y + 6; }
        term_flush(cur_x, cur_y, true);

        KeyEvent k = term_read_key();
        if (k.type == KEY_ESC) return false;
        if (k.type == KEY_TAB)       { focus = (focus + 1) % 4; continue; }
        if (k.type == KEY_SHIFT_TAB) { focus = (focus + 3) % 4; continue; }
        if (focus >= 1 && k.type == KEY_RIGHT) { focus = (focus == 3) ? 1 : focus + 1; continue; }
        if (focus >= 1 && k.type == KEY_LEFT)  { focus = (focus == 1) ? 3 : focus - 1; continue; }
        if (k.type == KEY_ENTER) {
            if (focus == 0 || focus == 1) return true;
            if (focus == 2) return false;
            if (focus == 3) { sdm_run_welcome(); continue; }
        }
        if (focus == 0) {
            if (k.type == KEY_BS && len > 0) {
                while (len > 0 && ((unsigned char)buf[len - 1] & 0xC0) == 0x80) len--;
                if (len > 0) len--;
                buf[len] = '\0';
            } else if (k.type == KEY_CHAR && k.ch >= 0x20) {
                char ch[8]; cp_to_utf8(k.ch, ch);
                size_t cl = strlen(ch);
                if ((size_t)len + cl < cap) {
                    memcpy(buf + len, ch, cl + 1);
                    len += (int)cl;
                }
            }
        }
    }
}
