#ifndef EDIT_H
#define EDIT_H

#include "compat.h"

#define SCREEN_INIT_COLS 80
#define SCREEN_INIT_ROWS 25

/* 원본 51f0:07ea / 51f0:090a 커맨드 ID */
enum {
    CMD_FILE_NEW      = 0x0009,
    CMD_FILE_OPEN     = 0x000a,
    CMD_FILE_SAVE     = 0x000b,
    CMD_FILE_SAVEAS   = 0x000c,
    CMD_FILE_PRINT    = 0x0012,
    CMD_FILE_EXIT     = 0x0015,

    CMD_EDIT_CUT      = 0x0056,
    CMD_EDIT_COPY     = 0x0018,
    CMD_EDIT_PASTE    = 0x001a,
    CMD_EDIT_CLEAR    = 0x0057,

    CMD_SEARCH_FIND   = 0x0024,
    CMD_SEARCH_REPEAT = 0x0026,
    CMD_SEARCH_CHANGE = 0x0027,

    CMD_OPT_DISPLAY   = 0x003c,
    CMD_OPT_PATH      = 0x003d,

    CMD_HELP_START    = 0x0046,
    CMD_HELP_KEYS     = 0x0047,
    CMD_HELP_ABOUT    = 0x0048
};

enum {
    KEY_NONE = 0, KEY_CHAR, KEY_ENTER, KEY_ESC, KEY_BS, KEY_TAB, KEY_SHIFT_TAB,
    KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_HOME, KEY_END, KEY_PGUP, KEY_PGDN,
    KEY_INS, KEY_DEL, KEY_F1, KEY_F3,
    KEY_ALT_ONLY, KEY_ALT_CHAR, KEY_CTRL_CHAR,
    KEY_CTRL_INS, KEY_SHIFT_INS, KEY_SHIFT_DEL
};

typedef struct {
    int      type;
    uint32_t ch;
} KeyEvent;

enum { CELL_NORMAL = 0, CELL_LEAD = 1, CELL_TRAIL = 2 };

typedef struct {
    char    utf8[8];
    uint8_t fg;
    uint8_t bg;
    uint8_t kind;
} ScreenCell;

typedef struct {
    int cols;
    int rows;
    ScreenCell *cells;
    ScreenCell *prev;
} ScreenBuf;

typedef struct {
    bool is_utf8;
    bool is_dbcs;
    char codeset[64];
#ifdef _WIN32
    UINT cp;
#else
    iconv_t cd_from_utf8;
    iconv_t cd_to_utf8;
#endif
} CodepageCtx;

typedef struct {
    uint16_t cmd_id;
    bool     is_sep;
    char     hotkey;
    const char *label;
    const char *shortcut;
} MenuItemDef;

typedef struct {
    uint16_t menu_id;
    int      x_pos;
    char     hotkey;
    const char *title;
    int      dropdown_w;
    int      item_count;
    const MenuItemDef *items;
} MenuBarDef;

typedef struct {
    char **lines;
    int    line_cnt;
    int    line_cap;
    int    cur_row;
    int    cur_col;
    int    top_row;
    int    left_col;
    bool   dirty;
    bool   insert_mode;
    char   filename[260];

    char  *clipboard;
    char   find_text[128];
    char   replace_text[128];
    uint8_t fg_color;
    uint8_t bg_color;
    int    tab_stops;
    bool   show_scrollbar;

    bool   running;
    bool   menu_active;
    bool   menu_dropped;
    int    menu_idx;
    int    drop_idx;
} EditorState;

extern ScreenBuf   g_scr;
extern CodepageCtx g_cp;
extern EditorState g_ed;
extern const MenuBarDef EDIT_MENUS[5];

/* platform/term.c */
void     cp_init(void);
size_t   utf8_to_sys(const char *src, char *dst, size_t dst_cap);
void     sys_to_utf8(const char *src, size_t src_len, char *dst, size_t dst_cap);
void     term_init(void);
void     term_cleanup(void);
void     term_flush(int cur_x, int cur_y, bool show_cursor);
KeyEvent term_read_key(void);

/* ui/screen.c */
void scr_init(int cols, int rows);
int  utf8_next_cp(const char *s, uint32_t *cp_out, char ch_out[8]);
void cp_to_utf8(uint32_t cp, char out[8]);
int  cp_cell_width(uint32_t cp);
int  utf8_display_width(const char *s);
void scr_put_cell(int x, int y, const char *utf8_ch, int width, uint8_t fg, uint8_t bg);
int  scr_puts(int x, int y, const char *utf8_str, uint8_t fg, uint8_t bg, int max_w);
void scr_fill(int x, int y, int w, int h, const char *ch, uint8_t fg, uint8_t bg);
void scr_box(int x, int y, int w, int h, bool double_line, uint8_t fg, uint8_t bg);
void scr_shadow(int x, int y, int w, int h);
int  scr_puts_hotkey(int x, int y, const char *utf8_str, bool highlight_hk,
                     uint8_t fg, uint8_t bg, uint8_t hk_fg, int max_w);

/* core/doc.c */
int    line_cp_len(const char *s);
size_t line_byte_offset(const char *s, int cp_idx);
void   ed_clear_doc(void);
void   ed_insert_line(int idx, const char *text);
void   ed_delete_line(int idx);
bool   ed_load_file(const char *path);
bool   ed_save_file(const char *path);
void   ed_handle_edit_key(KeyEvent k);

/* ui/menu.c */
void render_base_screen(int *out_cur_x, int *out_cur_y);
void exec_command(uint16_t cmd);
void handle_key(KeyEvent k);
void render_status_bar(const char *msg, bool show_pos);

/* sdm/dialog.c */
void sdm_run_welcome(void);
bool sdm_run_file_dialog(const char *title, char *path_buf, size_t cap);
bool sdm_run_prompt(const char *title, const char *prompt, char *buf, size_t cap);
/* 하단 프로토타입 */
int  sdm_run_confirm_save(void);   /* 1=예(저장), 0=아니오, -1=취소 */
void sdm_run_display_opt(void);    /* src/sdm/display_opt.c */

#endif /* EDIT_H */
