#include "edit.h"

int line_cp_len(const char *s) {
    int n = 0;
    for (const char *p = s; *p; ) {
        uint32_t cp; char ch[8];
        int adv = utf8_next_cp(p, &cp, ch);
        if (adv <= 0) break;
        p += adv; n++;
    }
    return n;
}

size_t line_byte_offset(const char *s, int cp_idx) {
    int n = 0;
    const char *p = s;
    while (*p && n < cp_idx) {
        uint32_t cp; char ch[8];
        int adv = utf8_next_cp(p, &cp, ch);
        if (adv <= 0) break;
        p += adv; n++;
    }
    return (size_t)(p - s);
}

void ed_clear_doc(void) {
    for (int i = 0; i < g_ed.line_cnt; ++i) free(g_ed.lines[i]);
    free(g_ed.lines);
    g_ed.line_cap = 64;
    g_ed.lines = (char **)calloc((size_t)g_ed.line_cap, sizeof(char *));
    g_ed.lines[0] = compat_strdup("");
    g_ed.line_cnt = 1;
    g_ed.cur_row = g_ed.cur_col = g_ed.top_row = g_ed.left_col = 0;
    g_ed.dirty = false;
    strcpy(g_ed.filename, "제목없음");
}

void ed_insert_line(int idx, const char *text) {
    if (g_ed.line_cnt >= g_ed.line_cap) {
        g_ed.line_cap *= 2;
        g_ed.lines = (char **)realloc(g_ed.lines, (size_t)g_ed.line_cap * sizeof(char *));
    }
    memmove(&g_ed.lines[idx + 1], &g_ed.lines[idx], (size_t)(g_ed.line_cnt - idx) * sizeof(char *));
    g_ed.lines[idx] = compat_strdup(text ? text : "");
    g_ed.line_cnt++;
}

void ed_delete_line(int idx) {
    if (g_ed.line_cnt <= 1) {
        free(g_ed.lines[0]);
        g_ed.lines[0] = compat_strdup("");
        return;
    }
    free(g_ed.lines[idx]);
    memmove(&g_ed.lines[idx], &g_ed.lines[idx + 1], (size_t)(g_ed.line_cnt - idx - 1) * sizeof(char *));
    g_ed.line_cnt--;
}

bool ed_load_file(const char *path) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return false;
    ed_clear_doc();
    free(g_ed.lines[0]);
    g_ed.line_cnt = 0;

    char buf[4096], u8[8192];
    while (fgets(buf, sizeof(buf), fp)) {
        size_t len = strlen(buf);
        while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r')) buf[--len] = '\0';
        sys_to_utf8(buf, len, u8, sizeof(u8));
        ed_insert_line(g_ed.line_cnt, u8);
    }
    fclose(fp);
    if (g_ed.line_cnt == 0) ed_insert_line(0, "");
    snprintf(g_ed.filename, sizeof(g_ed.filename), "%s", path);
    g_ed.dirty = false;
    return true;
}

bool ed_save_file(const char *path) {
    FILE *fp = fopen(path, "wb");
    if (!fp) return false;
    char out[8192];
    for (int i = 0; i < g_ed.line_cnt; ++i) {
        size_t n = utf8_to_sys(g_ed.lines[i], out, sizeof(out));
        fwrite(out, 1, n, fp);
#ifdef _WIN32
        fwrite("\r\n", 1, 2, fp);
#else
        fwrite("\n", 1, 1, fp);
#endif
    }
    fclose(fp);
    snprintf(g_ed.filename, sizeof(g_ed.filename), "%s", path);
    g_ed.dirty = false;
    return true;
}

void ed_handle_edit_key(KeyEvent k) {
    int clen = line_cp_len(g_ed.lines[g_ed.cur_row]);
    switch (k.type) {
        case KEY_LEFT:
            if (g_ed.cur_col > 0) g_ed.cur_col--;
            break;
        case KEY_RIGHT:
            /* EDIT.COM 규칙: 빈 줄이어도 계속 오른쪽 이동 허용 */
            if (g_ed.cur_col < 255) g_ed.cur_col++;
            break;
        case KEY_UP:
            if (g_ed.cur_row > 0) g_ed.cur_row--;
            break;
        case KEY_DOWN:
            /* 존재하지 않는 줄로는 이동 불가, 열(cur_col)은 유지 */
            if (g_ed.cur_row + 1 < g_ed.line_cnt) g_ed.cur_row++;
            break;
        case KEY_HOME: g_ed.cur_col = 0;    break;
        case KEY_END:  g_ed.cur_col = clen; break; /* 실제 줄 끝으로 복귀 */
        case KEY_ENTER: {
            char *line = g_ed.lines[g_ed.cur_row];
            size_t boff = (g_ed.cur_col < clen) ? line_byte_offset(line, g_ed.cur_col) : strlen(line);
            ed_insert_line(g_ed.cur_row + 1, line + boff);
            line[boff] = '\0';
            g_ed.cur_row++; g_ed.cur_col = 0; g_ed.dirty = true;
            break;
        }
        case KEY_BS:
            if (g_ed.cur_col > clen) {
                g_ed.cur_col--;
            } else if (g_ed.cur_col > 0) {
                char *line = g_ed.lines[g_ed.cur_row];
                size_t b0 = line_byte_offset(line, g_ed.cur_col - 1);
                size_t b1 = line_byte_offset(line, g_ed.cur_col);
                memmove(line + b0, line + b1, strlen(line + b1) + 1);
                g_ed.cur_col--; g_ed.dirty = true;
            } else if (g_ed.cur_row > 0) {
                int prev_len = line_cp_len(g_ed.lines[g_ed.cur_row - 1]);
                char *prev = g_ed.lines[g_ed.cur_row - 1], *curr = g_ed.lines[g_ed.cur_row];
                char *merged = (char *)malloc(strlen(prev) + strlen(curr) + 1);
                strcpy(merged, prev); strcat(merged, curr);
                free(prev); g_ed.lines[g_ed.cur_row - 1] = merged;
                ed_delete_line(g_ed.cur_row);
                g_ed.cur_row--; g_ed.cur_col = prev_len; g_ed.dirty = true;
            }
            break;
        case KEY_TAB: {
            int spaces = g_ed.tab_stops - (g_ed.cur_col % g_ed.tab_stops);
            for (int s = 0; s < spaces; ++s) {
                KeyEvent sp = { KEY_CHAR, ' ' };
                ed_handle_edit_key(sp);
            }
            break;
        }
        case KEY_CHAR: {
            char ch[8]; cp_to_utf8(k.ch, ch);
            size_t cl = strlen(ch);
            char *line = g_ed.lines[g_ed.cur_row];
            /* 줄 끝 너머에서 입력 시 앞부분 공백 자동 채움 */
            if (g_ed.cur_col > clen) {
                int pad = g_ed.cur_col - clen;
                size_t old_len = strlen(line);
                char *nl = (char *)malloc(old_len + (size_t)pad + cl + 1);
                memcpy(nl, line, old_len);
                memset(nl + old_len, ' ', (size_t)pad);
                memcpy(nl + old_len + pad, ch, cl + 1);
                free(line);
                g_ed.lines[g_ed.cur_row] = nl;
            } else {
                size_t b0 = line_byte_offset(line, g_ed.cur_col);
                size_t b1 = (!g_ed.insert_mode && g_ed.cur_col < clen)
                            ? line_byte_offset(line, g_ed.cur_col + 1) : b0;
                size_t tail = strlen(line + b1);
                char *nl = (char *)malloc(b0 + cl + tail + 1);
                memcpy(nl, line, b0);
                memcpy(nl + b0, ch, cl);
                memcpy(nl + b0 + cl, line + b1, tail + 1);
                free(line);
                g_ed.lines[g_ed.cur_row] = nl;
            }
            g_ed.cur_col++; g_ed.dirty = true;
            break;
        }
        default: break;
    }
}
