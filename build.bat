@echo off
cl /std:c11 /O2 /W3 /Iinclude src\main.c src\platform\term.c src\core\doc.c src\ui\screen.c src\ui\menu.c src\sdm\dialog.c /Fe:edit_tui.exe
