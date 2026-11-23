#include "edit.h"

static const MenuItemDef MENU_FILE_ITEMS[] = {
    { CMD_FILE_NEW,    false, 'N', "N/새파일",               "" },
    { CMD_FILE_OPEN,   false, 'O', "O/파일열기...",          "" },
    { CMD_FILE_SAVE,   false, 'S', "S/보관",                 "" },
    { CMD_FILE_SAVEAS, false, 'A', "A/다른이름으로 보관...", "" },
    { 0,               true,   0,  "",                       "" },
    { CMD_FILE_PRINT,  false, 'P', "P/인쇄...",              "" },
    { 0,               true,   0,  "",                       "" },
    { CMD_FILE_EXIT,   false, 'X', "X/종료",                 "" }
};
static const MenuItemDef MENU_EDIT_ITEMS[] = {
    { CMD_EDIT_CUT,    false, 'T', "T/잘라냄",   "Shift+Del" },
    { CMD_EDIT_COPY,   false, 'C', "C/복사",     "Ctrl+Ins"  },
    { CMD_EDIT_PASTE,  false, 'P', "P/붙여넣기", "Shift+Ins" },
    { CMD_EDIT_CLEAR,  false, 'E', "E/지움",     "Del"       }
};
static const MenuItemDef MENU_SEARCH_ITEMS[] = {
    { CMD_SEARCH_FIND,   false, 'F', "F/찾아보기...", ""   },
    { CMD_SEARCH_REPEAT, false, 'R', "R/반복찾기",    "F3" },
    { CMD_SEARCH_CHANGE, false, 'C', "C/교환...",     ""   }
};
static const MenuItemDef MENU_OPT_ITEMS[] = {
    { CMD_OPT_DISPLAY, false, 'D', "D/화면표시...", "" },
    { CMD_OPT_PATH,    false, 'S', "S/경로설정...", "" }
};
static const MenuItemDef MENU_HELP_ITEMS[] = {
    { CMD_HELP_START, false, 'G', "G/시작하기",         "" },
    { CMD_HELP_KEYS,  false, 'K', "K/키보드",           "" },
    { 0,              true,   0,  "",                   "" },
    { CMD_HELP_ABOUT, false, 'A', "A/제품에 관한 정보", "" }
};

const MenuBarDef EDIT_MENUS[5] = {
    { 0,  3, 'F', "F/파일",   24, 8, MENU_FILE_ITEMS   },
    { 1, 11, 'E', "E/편집",   23, 4, MENU_EDIT_ITEMS   },
    { 2, 19, 'S', "S/검색",   18, 3, MENU_SEARCH_ITEMS },
    { 7, 28, 'O', "O/옵션",   17, 2, MENU_OPT_ITEMS    },
    { 8, 71, 'H', "H/도움말", 21, 4, MENU_HELP_ITEMS   }
};

static const char *get_menu_context_help(void) {
    if (!g_ed.menu_dropped) {
        return "한글 MS-DOS 편집기 <F1=도움말>, 메뉴는 ALT 키를 누르십시오";
    }
    uint16_t cmd = EDIT_MENUS[g_ed.menu_idx].items[g_ed.drop_idx].cmd_id;
    switch (cmd) {
        case CMD_FILE_NEW:      return "새 파일을 만듭니다.";
        case CMD_FILE_OPEN:     return "파일을 읽습니다.";
        case CMD_FILE_SAVE:     return "편집중인 파일을 디스크에 저장합니다.";
        case CMD_FILE_SAVEAS:   return "새이름으로 저장합니다.";
        case CMD_FILE_PRINT:    return "문서전체 또는 일부를 인쇄합니다.";
        case CMD_FILE_EXIT:     return "종료후 DOS로 돌아갑니다.";
        case CMD_EDIT_CUT:      return "지정된 내용을 삭제하고 버퍼에 저장합니다.";
        case CMD_EDIT_COPY:     return "지정된 내용을 버퍼로 저장합니다.";
        case CMD_EDIT_PASTE:    return "버퍼의 내용을 커서 위치에 삽입합니다.";
        case CMD_EDIT_CLEAR:    return "지정된 내용을 삭제하나 버퍼로 저장하지는 않습니다.";
        case CMD_SEARCH_FIND:   return "문자열을 검색합니다.";
        case CMD_SEARCH_REPEAT: return "다음 일치하는 것을 찾습니다.";
        case CMD_SEARCH_CHANGE: return "지정된 문자열을 교환합니다.";
        case CMD_OPT_DISPLAY:   return "화면 표시 속성을 변경합니다.";
        case CMD_OPT_PATH:      return "파일을 검색할 경로를 설정합니다.";
        case CMD_HELP_START:    return "한글 MS-DOS 편집기를 사용하는 방법을 보여줍니다.";
        case CMD_HELP_KEYS:     return "키보드를 사용해서 편집하는 방법을 보여줍니다.";
        case CMD_HELP_ABOUT:    return "버전과 저작권자에 관한 정보를 보여줍니다.";
        default:                return "";
    }
}

/* 23행 상태줄 규칙 + 메뉴 단축키 백색 강조 */
void render_status_bar(const char *msg, bool show_pos) {
    int cols = g_scr.cols;
    int st_y = g_scr.rows - 2; /* 23행 */
    scr_fill(0, st_y, cols, 1, " ", 15, 3);
    if (show_pos) {
        scr_puts(1, st_y, msg, 15, 3, cols - 15);
        scr_put_cell(cols - 13, st_y, g_cp.is_utf8 ? "│" : "|", 1, 0, 3);
        char pos_str[32];
        snprintf(pos_str, sizeof(pos_str), " %05d:%03d ", g_ed.cur_row + 1, g_ed.cur_col + 1);
        scr_puts(cols - 12, st_y, pos_str, 0, 3, 12);
    } else {
        scr_puts(1, st_y, msg, 15, 3, cols - 2);
    }
}

void render_base_screen(int *out_cur_x, int *out_cur_y) {
    int cols = g_scr.cols, rows = g_scr.rows;
    uint8_t fg = g_ed.fg_color, bg = g_ed.bg_color;

    /* 0행: 최상위 메뉴 바 */
    scr_fill(0, 0, cols, 1, " ", 0, 7);
    for (int i = 0; i < 5; ++i) {
        int mx = EDIT_MENUS[i].x_pos;
        bool sel = (g_ed.menu_active && g_ed.menu_idx == i);
        uint8_t mfg = sel ? 7 : 0, mbg = sel ? 0 : 7;
        scr_put_cell(mx - 1, 0, " ", 1, mfg, mbg);
        int w = scr_puts_hotkey(mx, 0, EDIT_MENUS[i].title, g_ed.menu_active, mfg, mbg, 15, 0);
        scr_put_cell(mx + w, 0, " ", 1, mfg, mbg);
    }

    /* 1행 ~ 22행 (edit_h = 22): 상단 테두리(1) + 본문 20행(2..21) + 하단 테두리(22) */
    int edit_h = rows - 3;
    scr_fill(0, 1, cols, edit_h, " ", fg, bg);
    scr_box(0, 1, cols, edit_h, false, fg, bg);

    char title_bar[300];
    snprintf(title_bar, sizeof(title_bar), " %s%s ", g_ed.filename, g_ed.dirty ? "*" : "");
    int tw = utf8_display_width(title_bar);
    int tx = (cols - tw) / 2;
    if (tx < 2) tx = 2;
    scr_puts(tx, 1, title_bar, bg, fg, cols - 4);

    int view_rows = edit_h - 2; /* 20행 */
    int view_cols = cols - 2;   /* 78열 */
    if (g_ed.cur_row < g_ed.top_row) g_ed.top_row = g_ed.cur_row;
    if (g_ed.cur_row >= g_ed.top_row + view_rows) g_ed.top_row = g_ed.cur_row - view_rows + 1;

    int cur_scr_x = 1, cur_scr_y = 2;
    for (int vr = 0; vr < view_rows; ++vr) {
        int doc_r = g_ed.top_row + vr;
        if (doc_r >= g_ed.line_cnt) break;
        const char *p = g_ed.lines[doc_r];
        int cp_idx = 0, sx = 1;
        if (doc_r == g_ed.cur_row && g_ed.cur_col == 0) { cur_scr_x = 1; cur_scr_y = 2 + vr; }
        while (*p) {
            uint32_t cp; char ch[8];
            int adv = utf8_next_cp(p, &cp, ch);
            int cw = cp_cell_width(cp);
            if (cp_idx >= g_ed.left_col) {
                if (sx + cw - 1 <= view_cols) scr_put_cell(sx, 2 + vr, ch, cw, fg, bg);
                sx += cw;
            }
            cp_idx++;
            if (doc_r == g_ed.cur_row && cp_idx == g_ed.cur_col) {
                cur_scr_x = (sx <= view_cols) ? sx : view_cols;
                cur_scr_y = 2 + vr;
            }
            p += adv;
        }
        if (doc_r == g_ed.cur_row && g_ed.cur_col > cp_idx) {
            int extra = sx + (g_ed.cur_col - cp_idx);
            cur_scr_x = (extra <= view_cols) ? extra : view_cols;
            cur_scr_y = 2 + vr;
        }
    }

    /* 23행 (rows - 2): EDIT 상태줄 — 메뉴 활성 시 컨텍스트 도움말 표시 */
    int st_y = rows - 2;
    scr_fill(0, st_y, cols, 1, " ", 0, 3);
    /* 23행: EDIT 상태줄 */
    if (g_ed.menu_active) {
        render_status_bar(get_menu_context_help(), false);
    } else {
        render_status_bar("한글 MS-DOS 편집기 <F1=도움말>, 메뉴는 ALT 키를 누르십시오", true);
    }

    /* 24행 (rows - 1): HBIOS / IME 상태줄 (25번째 줄) */
    int ime_y = rows - 1;
    scr_fill(0, ime_y, cols, 1, " ", 7, 0);
    char ime_bar[128];
    snprintf(ime_bar, sizeof(ime_bar), " [영문] [%s] [%s]",
             g_cp.codeset, g_ed.insert_mode ? "삽입" : "수정");
    scr_puts(0, ime_y, ime_bar, 7, 0, cols);

    /* 드롭다운 메뉴 오버레이 */
    if (g_ed.menu_active && g_ed.menu_dropped) {
        const MenuBarDef *m = &EDIT_MENUS[g_ed.menu_idx];
        int dx = m->x_pos - 1, dw = m->dropdown_w, dh = m->item_count + 2;
        if (dx + dw >= cols) dx = cols - dw - 2;
        scr_fill(dx, 1, dw, dh, " ", 0, 7);
        scr_box(dx, 1, dw, dh, false, 0, 7);
        scr_shadow(dx, 1, dw, dh);
        for (int i = 0; i < m->item_count; ++i) {
            const MenuItemDef *it = &m->items[i];
            int iy = 2 + i;
            if (it->is_sep) {
                scr_put_cell(dx, iy, g_cp.is_utf8 ? "├" : "+", 1, 0, 7);
                for (int c = dx + 1; c < dx + dw - 1; ++c)
                    scr_put_cell(c, iy, g_cp.is_utf8 ? "─" : "-", 1, 0, 7);
                scr_put_cell(dx + dw - 1, iy, g_cp.is_utf8 ? "┤" : "+", 1, 0, 7);
            } else {
                bool isel = (g_ed.drop_idx == i);
                uint8_t ifg = isel ? 7 : 0, ibg = isel ? 0 : 7;
                scr_fill(dx + 1, iy, dw - 2, 1, " ", ifg, ibg);
                scr_puts_hotkey(dx + 2, iy, it->label, true, ifg, ibg, 15, dw - 4);
                if (it->shortcut[0]) {
                    int slen = (int)strlen(it->shortcut);
                    scr_puts(dx + dw - 2 - slen, iy, it->shortcut, ifg, ibg, slen);
                }
            }
        }
    }
    *out_cur_x = cur_scr_x;
    *out_cur_y = cur_scr_y;
}

void exec_command(uint16_t cmd) {
    char path_buf[260] = {0};
    switch (cmd) {
        case CMD_FILE_NEW:  ed_clear_doc(); break;
        case CMD_FILE_OPEN:
            if (sdm_run_file_dialog(" 파일 열기 ", path_buf, sizeof(path_buf)) && path_buf[0])
                ed_load_file(path_buf);
            break;
        case CMD_FILE_SAVE:
            if (strcmp(g_ed.filename, "제목없음") != 0) { ed_save_file(g_ed.filename); break; }
            /* fallthrough */
        case CMD_FILE_SAVEAS:
            if (strcmp(g_ed.filename, "제목없음") != 0)
                snprintf(path_buf, sizeof(path_buf), "%s", g_ed.filename);
            if (sdm_run_file_dialog(" 다른 이름으로 보관 ", path_buf, sizeof(path_buf)) && path_buf[0])
                ed_save_file(path_buf);
            break;
        case CMD_FILE_EXIT: g_ed.running = false; break;
        case CMD_EDIT_COPY:
            free(g_ed.clipboard);
            g_ed.clipboard = compat_strdup(g_ed.lines[g_ed.cur_row]);
            break;
        case CMD_EDIT_CUT:
            free(g_ed.clipboard);
            g_ed.clipboard = compat_strdup(g_ed.lines[g_ed.cur_row]);
            ed_delete_line(g_ed.cur_row);
            if (g_ed.cur_row >= g_ed.line_cnt) g_ed.cur_row = g_ed.line_cnt - 1;
            g_ed.cur_col = 0; g_ed.dirty = true;
            break;
        case CMD_EDIT_PASTE:
            if (g_ed.clipboard) {
                char *line = g_ed.lines[g_ed.cur_row];
                size_t boff = line_byte_offset(line, g_ed.cur_col);
                size_t clen = strlen(g_ed.clipboard), llen = strlen(line);
                char *nl = (char *)malloc(llen + clen + 1);
                memcpy(nl, line, boff);
                memcpy(nl + boff, g_ed.clipboard, clen);
                memcpy(nl + boff + clen, line + boff, llen - boff + 1);
                free(line);
                g_ed.lines[g_ed.cur_row] = nl;
                g_ed.cur_col += line_cp_len(g_ed.clipboard);
                g_ed.dirty = true;
            }
            break;
        case CMD_EDIT_CLEAR: {
            char *line = g_ed.lines[g_ed.cur_row];
            if (g_ed.cur_col < line_cp_len(line)) {
                size_t b0 = line_byte_offset(line, g_ed.cur_col);
                size_t b1 = line_byte_offset(line, g_ed.cur_col + 1);
                memmove(line + b0, line + b1, strlen(line + b1) + 1);
                g_ed.dirty = true;
            }
            break;
        }
        case CMD_SEARCH_FIND:
        case CMD_SEARCH_REPEAT:
            if (cmd == CMD_SEARCH_FIND || !g_ed.find_text[0]) {
                if (!sdm_run_prompt(" 찾아보기 ", "검색대상(F):", g_ed.find_text, sizeof(g_ed.find_text)))
                    break;
            }
            if (g_ed.find_text[0]) {
                for (int off = 0; off < g_ed.line_cnt; ++off) {
                    int r = (g_ed.cur_row + off) % g_ed.line_cnt;
                    const char *start = g_ed.lines[r];
                    if (off == 0) {
                        size_t b = line_byte_offset(start, g_ed.cur_col + 1);
                        if (b <= strlen(start)) start += b;
                    }
                    const char *hit = strstr(start, g_ed.find_text);
                    if (hit) { g_ed.cur_row = r; g_ed.cur_col = (int)(hit - g_ed.lines[r]); break; }
                }
            }
            break;
        case CMD_SEARCH_CHANGE:
            if (sdm_run_prompt(" 교환 ", "교환전(F):", g_ed.find_text, sizeof(g_ed.find_text)) && g_ed.find_text[0]) {
                if (sdm_run_prompt(" 교환 ", "교환후(T):", g_ed.replace_text, sizeof(g_ed.replace_text))) {
                    char *line = g_ed.lines[g_ed.cur_row], *hit = strstr(line, g_ed.find_text);
                    if (hit) {
                        size_t pre = (size_t)(hit - line), fl = strlen(g_ed.find_text);
                        size_t rl = strlen(g_ed.replace_text), post = strlen(hit + fl);
                        char *nl = (char *)malloc(pre + rl + post + 1);
                        memcpy(nl, line, pre); memcpy(nl + pre, g_ed.replace_text, rl);
                        memcpy(nl + pre + rl, hit + fl, post + 1);
                        free(line); g_ed.lines[g_ed.cur_row] = nl; g_ed.dirty = true;
                    }
                }
            }
            break;
        case CMD_HELP_START:
        case CMD_HELP_ABOUT: sdm_run_welcome(); break;
        case CMD_OPT_DISPLAY: sdm_run_display_opt(); break;
        default: break;
    }
}

static void move_drop_idx(int delta) {
    const MenuBarDef *m = &EDIT_MENUS[g_ed.menu_idx];
    for (int step = 0; step < m->item_count; ++step) {
        g_ed.drop_idx = (g_ed.drop_idx + delta + m->item_count) % m->item_count;
        if (!m->items[g_ed.drop_idx].is_sep) break;
    }
}

void handle_key(KeyEvent k) {
    if (k.type == KEY_NONE) return;

    if (k.type == KEY_ALT_ONLY) {
        g_ed.menu_active = !g_ed.menu_active;
        g_ed.menu_dropped = false;
        g_ed.menu_idx = 0; g_ed.drop_idx = 0;
        return;
    }

    /* [수정 #3] 드롭다운이 열린 상태에서 Alt를 누른 채 S/X 등을 눌러도 항목 단축키 우선 실행 */
    if (g_ed.menu_active && g_ed.menu_dropped && k.type == KEY_ALT_CHAR) {
        const MenuBarDef *m = &EDIT_MENUS[g_ed.menu_idx];
        for (int i = 0; i < m->item_count; ++i) {
            if (!m->items[i].is_sep && m->items[i].hotkey == (char)k.ch) {
                uint16_t cmd = m->items[i].cmd_id;
                g_ed.menu_active = false; g_ed.menu_dropped = false;
                exec_command(cmd);
                return;
            }
        }
    }

    if (k.type == KEY_ALT_CHAR) {
        for (int i = 0; i < 5; ++i) {
            if (EDIT_MENUS[i].hotkey == (char)k.ch) {
                g_ed.menu_active = true; g_ed.menu_dropped = true;
                g_ed.menu_idx = i; g_ed.drop_idx = 0;
                return;
            }
        }
    }

    if (g_ed.menu_active) {
        if (k.type == KEY_ESC) { g_ed.menu_active = false; g_ed.menu_dropped = false; return; }
        if (k.type == KEY_LEFT)  { g_ed.menu_idx = (g_ed.menu_idx + 4) % 5; g_ed.drop_idx = 0; return; }
        if (k.type == KEY_RIGHT) { g_ed.menu_idx = (g_ed.menu_idx + 1) % 5; g_ed.drop_idx = 0; return; }
        if (k.type == KEY_DOWN) {
            if (!g_ed.menu_dropped) { g_ed.menu_dropped = true; g_ed.drop_idx = 0; }
            else move_drop_idx(+1);
            return;
        }
        if (k.type == KEY_UP && g_ed.menu_dropped) { move_drop_idx(-1); return; }
        if (k.type == KEY_ENTER) {
            if (!g_ed.menu_dropped) { g_ed.menu_dropped = true; g_ed.drop_idx = 0; }
            else {
                uint16_t cmd = EDIT_MENUS[g_ed.menu_idx].items[g_ed.drop_idx].cmd_id;
                g_ed.menu_active = false; g_ed.menu_dropped = false;
                exec_command(cmd);
            }
            return;
        }
        if (k.type == KEY_CHAR && k.ch < 0x80) {
            char up = (char)toupper((unsigned char)k.ch);
            if (!g_ed.menu_dropped) {
                for (int i = 0; i < 5; ++i) {
                    if (EDIT_MENUS[i].hotkey == up) {
                        g_ed.menu_idx = i; g_ed.menu_dropped = true; g_ed.drop_idx = 0; return;
                    }
                }
            } else {
                const MenuBarDef *m = &EDIT_MENUS[g_ed.menu_idx];
                for (int i = 0; i < m->item_count; ++i) {
                    if (!m->items[i].is_sep && m->items[i].hotkey == up) {
                        uint16_t cmd = m->items[i].cmd_id;
                        g_ed.menu_active = false; g_ed.menu_dropped = false;
                        exec_command(cmd);
                        return;
                    }
                }
            }
        }
        return;
    }

    if (k.type == KEY_F1)        { exec_command(CMD_HELP_START);    return; }
    if (k.type == KEY_F3)        { exec_command(CMD_SEARCH_REPEAT); return; }
    if (k.type == KEY_CTRL_INS)  { exec_command(CMD_EDIT_COPY);     return; }
    if (k.type == KEY_SHIFT_INS) { exec_command(CMD_EDIT_PASTE);    return; }
    if (k.type == KEY_SHIFT_DEL) { exec_command(CMD_EDIT_CUT);      return; }
    if (k.type == KEY_DEL)       { exec_command(CMD_EDIT_CLEAR);    return; }
    if (k.type == KEY_CTRL_CHAR && k.ch == 'L') { exec_command(CMD_SEARCH_REPEAT); return; }
    if (k.type == KEY_INS)       { g_ed.insert_mode = !g_ed.insert_mode; return; }

    ed_handle_edit_key(k);
}
