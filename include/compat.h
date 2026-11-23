#ifndef EDIT_COMPAT_H
#define EDIT_COMPAT_H

#ifndef _WIN32
  #define _XOPEN_SOURCE 700
#endif

#include <ctype.h>
#include <locale.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
  #include <direct.h>
  #define compat_getcwd(buf, sz) _getcwd((buf), (int)(sz))
  #define compat_strdup(s)       _strdup(s)
#else
  #include <iconv.h>
  #include <langinfo.h>
  #include <signal.h>
  #include <sys/select.h>
  #include <termios.h>
  #include <unistd.h>
  #define compat_getcwd(buf, sz) getcwd((buf), (sz))
  #define compat_strdup(s)       strdup(s)
#endif

#endif /* EDIT_COMPAT_H */
