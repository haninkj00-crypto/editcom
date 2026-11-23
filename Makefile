CC      := gcc
CFLAGS  := -std=c11 -O2 -Wall -Wextra -Iinclude
SRCS    := src/main.c src/platform/term.c src/core/doc.c \
           src/ui/screen.c src/ui/menu.c src/sdm/dialog.c \
           src/sdm/display_opt.c
TARGET  := edit_tui

all: $(TARGET)

$(TARGET): $(SRCS) include/compat.h include/edit.h
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET)

clean:
	rm -f $(TARGET) $(TARGET).exe
