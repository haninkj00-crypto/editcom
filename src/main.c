/*
 * =========================================================================
 * 한글 MS-DOS 6.2 EDIT.COM (/EDCOM) 복각 프로젝트 — 모듈 목차 및 구현 현황
 * =========================================================================
 * [목차 / 구현 트래커]
 *   [x] 1. platform/term.c   : 80x25 고정 초기화, VGA 16색 RGB(#0000AA 파랑),
 *                              Windows chcp / Linux CODESET 자동 추종, 키 입력
 *   [x] 2. ui/screen.c       : 80x25 셀 버퍼, DBCS 2셀 경계 보호(41b7:72a4), 상자
 *   [x] 3. ui/menu.c         : EDIT 메뉴 5종(51f0:07ea), Alt 유지 연속 단축키
 *                              (Alt+F->S->F->X), 전역 단축키(51f0:090a)
 *   [x] 4. core/doc.c        : 줄 버퍼, 파일 열기/보관(인코딩 변환), 기본 편집
 *   [x] 5. sdm/dialog.c      : DOS식 모달 포커스(Tab/Shift+Tab/화살표),
 *                              환영 창(5025:0059), 경로 표시 열기/보관 창, 찾기/교환
 *   [ ] 6. sdm/file_list.c   : 열기/보관 창 내부 디렉터리/파일 리스트박스 탐색
 *   [ ] 7. sdm/display_opt.c : 화면표시 옵션 창(5086:0000 - 문자색/바탕색/탭)
 *   [ ] 8. core/selection.c  : 본문 블록 선택(Shift+방향키) 및 부분 복사/잘라내기
 *   [ ] 9. core/res_loader.c : QBASIC_UNPACKED.EXE 바이너리 직접 리소스 로더
 * =========================================================================
 */

#include "edit.h"

ScreenBuf   g_scr;
CodepageCtx g_cp;
EditorState g_ed;

int main(int argc, char **argv) {
    cp_init();
    memset(&g_ed, 0, sizeof(g_ed));
    g_ed.fg_color    = 7; /* CGA 밝은 회색 */
    g_ed.bg_color    = 1; /* CGA 파랑 (#0000AA) */
    g_ed.tab_stops   = 8;
    g_ed.insert_mode = true;
    g_ed.running     = true;
    ed_clear_doc();

    term_init();
    atexit(term_cleanup);

    if (argc > 1) {
        if (!ed_load_file(argv[1])) {
            snprintf(g_ed.filename, sizeof(g_ed.filename), "%s", argv[1]);
        }
    } else {
        sdm_run_welcome();
    }

    while (g_ed.running) {
        int cx, cy;
        render_base_screen(&cx, &cy);
        /* 메뉴 활성 중에는 본문 커서 숨김 (DOS식 포커스 규칙) */
        term_flush(cx, cy, !g_ed.menu_active);
        KeyEvent k = term_read_key();
        handle_key(k);
    }
    return 0;
}
