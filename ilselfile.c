// ------------------------------------------------------------------------------
// ILPER 1.40 for Linux
// Copyright (c) 2008-2009  J-F Garnier
// Copyright (c) 2011-2012  Ch. Gottheimer
// Copyright (c) 2026       J.M. Vansteene
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// ------------------------------------------------------------------------------
//                                            
// ilselfile.c   HP-IL select LIF File
// 2026: created for linux by JM Vansteene
//       from rover source file (https://github.com/lecram/rover)
// ------------------------------------------------------------------------------

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE       700
#endif
#define _XOPEN_SOURCE_EXTENDED
#define _FILE_OFFSET_BITS   64

#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <wchar.h>
#include <wctype.h>
#include <string.h>
#include <sys/types.h>  /* pid_t, ... */
#include <stdio.h>
#include <limits.h>     /* PATH_MAX */
#include <locale.h>     /* setlocale(), LC_ALL */
#include <unistd.h>     /* chdir(), getcwd(), read(), close(), ... */
#include <dirent.h>     /* DIR, struct dirent, opendir(), ... */
#include <sys/stat.h>
#include <fcntl.h>      /* open() */
#include <errno.h>
#include <stdarg.h>
#include <ncurses.h>
#include "ilper.h"

/* CTRL+X: "^X"
   ALT+X: "M-X" */
#define RVK_QUIT        "q"
#define RVK_HELP        "?"
#define RVK_DOWN        "j"
#define RVK_UP          "k"
#define RVK_JUMP_DOWN   "J"
#define RVK_JUMP_UP     "K"
#define RVK_JUMP_TOP    "g"
#define RVK_JUMP_BOTTOM "G"
#define RVK_CD_DOWN     "l"
#define RVK_CD_UP       "h"
#define RVK_HOME        "H"
#define RVK_TARGET      "t"
#define RVK_COPY_PATH   "y"
#define RVK_PASTE_PATH  "p"
#define RVK_REFRESH     "r"
#define RVK_SHELL       "^M"
#define RVK_VIEW        " "
#define RVK_EDIT        "e"
#define RVK_OPEN        "o"
#define RVK_SEARCH      "/"
#define RVK_TG_FILES    "f"
#define RVK_TG_DIRS     "d"
#define RVK_TG_HIDDEN   "s"
#define RVK_NEW_FILE    "n"
#define RVK_NEW_DIR     "N"
#define RVK_RENAME      "R"
#define RVK_TG_EXEC     "E"
#define RVK_DELETE      "D"
#define RVK_TG_MARK     "m"
#define RVK_INVMARK     "M"
#define RVK_MARKALL     "a"
#define RVK_MARK_DELETE "X"
#define RVK_MARK_COPY   "C"
#define RVK_MARK_MOVE   "V"

/* Colors available: DEFAULT, RED, GREEN, YELLOW, BLUE, CYAN, MAGENTA, WHITE, BLACK. */
#define RVC_CWD         GREEN
#define RVC_STATUS      CYAN
#define RVC_BORDER      BLUE
#define RVC_SCROLLBAR   CYAN
#define RVC_LINK        CYAN
#define RVC_HIDDEN      YELLOW
#define RVC_EXEC        GREEN
#define RVC_REG         DEFAULT
#define RVC_DIR         DEFAULT
#define RVC_CHR         MAGENTA
#define RVC_BLK         MAGENTA
#define RVC_FIFO        BLUE
#define RVC_SOCK        MAGENTA
#define RVC_PROMPT      DEFAULT
#define RVC_TABNUM      DEFAULT
#define RVC_MARKS       YELLOW

/* Special symbols used by the TUI. See <curses.h> for available constants. */
#define RVS_SCROLLBAR   ACS_CKBOARD
#define RVS_MARK        ACS_DIAMOND

/* Listing view flags. */
#define SHOW_FILES      0x01u
#define SHOW_DIRS       0x02u
#define SHOW_HIDDEN     0x04u

/* Number of entries to jump on RVK_JUMP_DOWN and RVK_JUMP_UP. */
#define RV_JUMP         10

/* Default listing view flags.
   May include SHOW_FILES, SHOW_DIRS and SHOW_HIDDEN. */
#define RV_FLAGS        SHOW_FILES | SHOW_DIRS

/* Listing view parameters. */
#define HEIGHTWIN  	(LINES-7)
#define HEIGHT		(LINES-10)
#define STARTY		5
#define WIDTH		(COLS-2)
#define STARTX		1
#define STATUSPOS   	(COLS-20)

/* Information associated to each entry in listing. */
typedef struct Row {
  char *name;
  off_t size;
  mode_t mode;
  int islink;
  int marked;
} Row;

/* Global files struct. */
static struct Files {
  WINDOW *window;
  int nfiles;
  Row *rows;
  
  int scroll;
  int esel;
  uint8_t flags;
  char cwd[PATH_MAX];
} files;

/* Macros for accessing global state. */
#define ENAME(I)    files.rows[I].name
#define ESIZE(I)    files.rows[I].size
#define EMODE(I)    files.rows[I].mode
#define ISLINK(I)   files.rows[I].islink
#define MARKED(I)   files.rows[I].marked
#define SCROLL      files.scroll
#define ESEL        files.esel
#define FLAGS       files.flags
#define CWD         files.cwd

/* Helpers. */
#define MIN(A, B)   ((A) < (B) ? (A) : (B))
#define MAX(A, B)   ((A) > (B) ? (A) : (B))
#define ISDIR(E)    (strchr((E), '/') != NULL)

/* String buffers. */
#define BUFLEN  PATH_MAX
static char BUF1[BUFLEN];
static char BUF2[BUFLEN];
static wchar_t WBUF[BUFLEN];
static char   strselfile[] = "Select LIF File";

typedef enum EditStat {CONTINUE, CONFIRM, CANCEL} EditStat;

static void update_view();

/* Show a message on the status bar. */
static void message(Color color, char *fmt, ...)
{
    int len, pos;
    va_list args;

    va_start(args, fmt);
    vsnprintf(BUF1, MIN(BUFLEN, STATUSPOS), fmt, args);
    va_end(args);
    len = strlen(BUF1);
    pos = (STATUSPOS - len) / 2;
    attr_on(A_BOLD, NULL);
    color_set(color, NULL);
    mvwaddstr(files.window, HEIGHTWIN - 1, pos, BUF1);
    color_set(DEFAULT, NULL);
    attr_off(A_BOLD, NULL);
    //    wattron(files.window, A_BOLD + color);
    //    mvaddstr(LINES - 1, pos, BUF1);
    //    wattroff(files.window, A_BOLD + color);
}

/* Clear message area, leaving only status info. */
static void clear_message()
{
  mvwhline(files.window, HEIGHTWIN - 1, 0, ' ', STATUSPOS);
}

/* Comparison used to sort listing entries. */
static int rowcmp(const void *a, const void *b)
{
    int isdir1, isdir2, cmpdir;
    const Row *r1 = a;
    const Row *r2 = b;
    isdir1 = S_ISDIR(r1->mode);
    isdir2 = S_ISDIR(r2->mode);
    cmpdir = isdir2 - isdir1;
    return cmpdir ? cmpdir : strcoll(r1->name, r2->name);
}

/* Get all entries in current working directory. */
static int ls(Row **rowsp, uint8_t flags)
{
    DIR *dp;
    struct dirent *ep;
    struct stat statbuf;
    Row *rows;
    int i, n;

    if(!(dp = opendir("."))) return -1;
    n = -2; /* We don't want the entries "." and "..". */
    while (readdir(dp)) n++;
    if (n == 0) {
        closedir(dp);
        return 0;
    }
    rewinddir(dp);
    rows = malloc(n * sizeof *rows);
    i = 0;
    while ((ep = readdir(dp))) {
        if (!strcmp(ep->d_name, ".") || !strcmp(ep->d_name, ".."))
            continue;
        if (!(flags & SHOW_HIDDEN) && ep->d_name[0] == '.')
            continue;
        lstat(ep->d_name, &statbuf);
        rows[i].islink = S_ISLNK(statbuf.st_mode);
        stat(ep->d_name, &statbuf);
        if (S_ISDIR(statbuf.st_mode)) {
            if (flags & SHOW_DIRS) {
                rows[i].name = malloc(strlen(ep->d_name) + 2);
                strcpy(rows[i].name, ep->d_name);
                if (!rows[i].islink)
                    strcat(rows[i].name, "/");
                rows[i].mode = statbuf.st_mode;
                i++;
            }
        } else if (flags & SHOW_FILES) {
            rows[i].name = malloc(strlen(ep->d_name) + 1);
            strcpy(rows[i].name, ep->d_name);
            rows[i].size = statbuf.st_size;
            rows[i].mode = statbuf.st_mode;
            i++;
        }
    }
    n = i; /* Ignore unused space in array caused by filters. */
    qsort(rows, n, sizeof (*rows), rowcmp);
    closedir(dp);
    *rowsp = rows;
    return n;
}

static void free_rows(Row **rowsp, int nfiles)
{
    int i;

    for (i = 0; i < nfiles; i++)
        free((*rowsp)[i].name);
    free(*rowsp);
    *rowsp = NULL;
}

/* Change working directory to the path in CWD. */
static void cd(int reset)
{
  message(CYAN, "Loading \"%s\"...", CWD);
  wrefresh(files.window);

  if (chdir(CWD) == -1) {
    getcwd(CWD, PATH_MAX-1);
    if (CWD[strlen(CWD)-1] != '/')
      strcat(CWD, "/");
    goto done;
  }

  if (reset) ESEL = SCROLL = 0;
  if (files.nfiles)
    free_rows(&files.rows, files.nfiles);
  files.nfiles = ls (&files.rows, FLAGS);

 done:
  clear_message();
  update_view();
}

/* Select a target entry, if it is present. */
static void try_to_sel(const char *target)
{
    ESEL = 0;
    if (!ISDIR(target))
        while ((ESEL+1) < files.nfiles && S_ISDIR(EMODE(ESEL)))
            ESEL++;
    while ((ESEL+1) < files.nfiles && strcoll(ENAME(ESEL), target) < 0)
        ESEL++;
}

/* Update the listing view. */
static void update_view()
{
    int i, j;
    int ishidden;

    wcolor_set ( files.window, RVC_BORDER, NULL );
    wborder   ( files.window, 0, 0, 0, 0, 0, 0, 0, 0 );
    mvwprintw ( files.window, 0, (WIDTH - strlen (strselfile)) / 2, "%s", strselfile);
    mvwhline  ( files.window, HEIGHT+1, 1,       boxc[2], WIDTH-2 );
    mvwaddch  ( files.window, HEIGHT+1, 0,       boxc[6] );
    mvwaddch  ( files.window, HEIGHT+1, WIDTH-1, boxc[7] );
    mvwhline  ( files.window, HEIGHT+2,  0,       ' ', WIDTH);

    wcolor_set(files.window, RVC_CWD, NULL);
    mbstowcs(WBUF, CWD, PATH_MAX);
    mvwaddnwstr(files.window, HEIGHT+2, 1, WBUF, WIDTH - 3);

    /* Selection might not be visible, due to cursor wrapping or window
       shrinking. In that case, the scroll must be moved to make it visible. */
    if (files.nfiles > HEIGHT) {
        SCROLL = MAX(MIN(SCROLL, ESEL), ESEL - HEIGHT + 1);
        SCROLL = MIN(MAX(SCROLL, 0), files.nfiles - HEIGHT);
    } else
        SCROLL = 0;

    //    marking = !strcmp(CWD, files.marks.dirpath);
    for (i = 0, j = SCROLL; i < HEIGHT && j < files.nfiles; i++, j++) {
        ishidden = ENAME(j)[0] == '.';
	if (j == ESEL)
            wattr_on(files.window, A_REVERSE, NULL);
        if (ISLINK(j))
            wcolor_set(files.window, RVC_LINK, NULL);
        else if (ishidden)
            wcolor_set(files.window, RVC_HIDDEN, NULL);
        else if (S_ISREG(EMODE(j))) {
            if (EMODE(j) & (S_IXUSR | S_IXGRP | S_IXOTH))
                wcolor_set(files.window, RVC_EXEC, NULL);
            else
                wcolor_set(files.window, RVC_REG, NULL);
        } else if (S_ISDIR(EMODE(j)))
            wcolor_set(files.window, RVC_DIR, NULL);
        else if (S_ISCHR(EMODE(j)))
            wcolor_set(files.window, RVC_CHR, NULL);
        else if (S_ISBLK(EMODE(j)))
            wcolor_set(files.window, RVC_BLK, NULL);
        else if (S_ISFIFO(EMODE(j)))
            wcolor_set(files.window, RVC_FIFO, NULL);
        else if (S_ISSOCK(EMODE(j)))
            wcolor_set(files.window, RVC_SOCK, NULL);
        if (S_ISDIR(EMODE(j))) {
            mbstowcs(WBUF, ENAME(j), PATH_MAX);
            if (ISLINK(j))
                wcscat(WBUF, L"/");
        } else {
            char *suffix, *suffixes = "BKMGTPEZY";
            off_t human_size = ESIZE(j) * 10;
            int length = mbstowcs(WBUF, ENAME(j), PATH_MAX);
            int namecols = wcswidth(WBUF, length);
            for (suffix = suffixes; human_size >= 10240; suffix++)
                human_size = (human_size + 512) / 1024;
            if (*suffix == 'B')
                swprintf(WBUF + length, PATH_MAX - length, L"%*d %c",
                         (int) (WIDTH - namecols - 6),
                         (int) human_size / 10, *suffix);
            else
                swprintf(WBUF + length, PATH_MAX - length, L"%*d.%d %c",
                         (int) (WIDTH - namecols - 8),
                         (int) human_size / 10, (int) human_size % 10, *suffix);
        }
        mvwhline(files.window, i + 1, 1, ' ', WIDTH - 2);
        mvwaddnwstr(files.window, i + 1, 2, WBUF, WIDTH - 4);
	mvwaddch(files.window, i + 1, 1, ' ');
        if (j == ESEL)
            wattr_off(files.window, A_REVERSE, NULL);
    }
    for (; i < HEIGHT; i++)
      mvwhline(files.window, i + 1, 1, ' ', COLS - 2);

    /* right scroll bar */
    if (files.nfiles > HEIGHT) {
      int center, height;
      center = (SCROLL + HEIGHT / 2) * HEIGHT / files.nfiles;
      height = (HEIGHT-1) * HEIGHT / files.nfiles;
      if (!height) height = 1;
      wcolor_set(files.window, RVC_SCROLLBAR, NULL);
      mvwvline(files.window, center-height/2+1, WIDTH-1, RVS_SCROLLBAR, height);
    }
    
    BUF1[0] = FLAGS & SHOW_FILES  ? 'F' : ' ';
    BUF1[1] = FLAGS & SHOW_DIRS   ? 'D' : ' ';
    BUF1[2] = FLAGS & SHOW_HIDDEN ? 'H' : ' ';
    if (!files.nfiles)
        strcpy(BUF2, "0/0");
    else
        snprintf(BUF2, BUFLEN, "%d/%d", ESEL + 1, files.nfiles);
    snprintf(BUF1+3, BUFLEN-3, "%12s", BUF2);
    wcolor_set(files.window, RVC_STATUS, NULL);
    mvwaddstr(files.window, HEIGHTWIN - 1, STATUSPOS, BUF1);
    wrefresh(files.window);
}

void select_file (WINDOW *w)
{
  int ch;
  const char *key;

  files.nfiles = 0;
  SCROLL = ESEL = 0;
  files.flags = RV_FLAGS;
  strcpy(files.cwd, strwd);
  files.window = subwin(w, HEIGHTWIN, WIDTH, STARTY, STARTX);

  cd(1);
  wrefresh (files.window);

  nodelay (stdscr, FALSE);
  notimeout(stdscr, FALSE);
  while (1) {
    ch = getch();
    key = keyname(ch);
    clear_message();

    if (!strcmp(key, RVK_QUIT)) break;
    else if (ch == 0x1b) break;
    else if (ch == 0x0A) {
      if (!S_ISDIR(EMODE(ESEL))) {
	strcpy (strwd, files.cwd);
	strcpy (strca, ENAME(ESEL));
      }
      break;
    }
    else if (ch == KEY_DOWN) {
      if (!files.nfiles) continue;
      ESEL = MIN(ESEL + 1, files.nfiles - 1);
      update_view();
    } else if (ch == KEY_UP) {
      if (!files.nfiles) continue;
      ESEL = MAX(ESEL - 1, 0);
      update_view();
    }
    else if (!strcmp(key, RVK_JUMP_UP)) {
      if (!files.nfiles) continue;
      ESEL = MAX(ESEL - RV_JUMP, 0);
      SCROLL = MAX(SCROLL - RV_JUMP, 0);
      update_view();
    } else if (!strcmp(key, RVK_JUMP_TOP)) {
      if (!files.nfiles) continue;
      ESEL = 0;
      update_view();
    } else if (!strcmp(key, RVK_JUMP_BOTTOM)) {
      if (!files.nfiles) continue;
      ESEL = files.nfiles - 1;
      update_view();
    } else if (ch == KEY_RIGHT) {
      if (!files.nfiles || !S_ISDIR(EMODE(ESEL))) continue;
      if (chdir(ENAME(ESEL)) == -1) {
	message(RED, "Cannot access \"%s\".", ENAME(ESEL));
	continue;
      }
      strcat(CWD, ENAME(ESEL));
      cd(1);
    } else if (ch == KEY_LEFT) {
      char *dirname, first;
      if (!strcmp(CWD, "/")) continue;
      CWD[strlen(CWD) - 1] = '\0';
      dirname = strrchr(CWD, '/') + 1;
      first = dirname[0];
      dirname[0] = '\0';
      cd(1);
      dirname[0] = first;
      dirname[strlen(dirname)] = '/';
      try_to_sel(dirname);
      dirname[0] = '\0';
      if (files.nfiles > HEIGHT)
	SCROLL = ESEL - HEIGHT / 2;
      update_view();
    } else if (!strcmp(key, RVK_HOME)) {
      strcpy(CWD, getenv("HOME"));
      if (CWD[strlen(CWD) - 1] != '/')
	strcat(CWD, "/");
      cd(1);
    }
  }
  werase (files.window); // to clear the screen image
  delwin (files.window);
  if (files.nfiles) {
    free_rows(&files.rows, files.nfiles);
  }
}
