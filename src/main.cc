/*** includes ***/

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

using namespace std;

/*** defines aa ***/

#define KILO_VERSION "0.0.1"

#define CTRL_KEY(k) ((k) & 0x1f)

enum editorKey {
  ARROW_LEFT = 1000,
  ARROW_RIGHT,
  ARROW_UP,
  ARROW_DOWN,
  PAGE_UP,
  PAGE_DOWN
};

/*** data ***/

struct termios orig_termios;

struct editorConfig {
  int cx, cy;
  int screenrows;
  int screencols;
  struct termios orig_termios;
};

struct editorConfig E;

/*** terminal ***/

void Die(const char* s) {
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);

  perror(s);
  exit(1);
}

void DisableRawMode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
    Die("tcsetattr");
}

void EnableRawMode() {
  if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1)
    Die("tcgetattr");
  atexit(DisableRawMode);

  struct termios raw = E.orig_termios;

  // IXON: Ctrl-S and Ctrl-Q
  // ICANON: canonical mode
  // ISIG: Ctrl-C and Ctrl-Z signals
  // IEXTEN: Ctrl-V
  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  raw.c_oflag &= ~(OPOST);
  raw.c_cflag |= (CS8);
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;

  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
    Die("tcsetattr");
}

int EditorReadKey() {
  int nread;
  char c;
  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN)
      Die("read");
  }

  if (c == '\x1b') {
    char seq[3];
    if (read(STDIN_FILENO, &seq[0], 1) != 1)
      return '\x1b';
    if (read(STDIN_FILENO, &seq[1], 1) != 1)
      return '\x1b';
    if (seq[0] == '[') {
      if (seq[1] >= '0' && seq[1] <= '9') {
        if (read(STDIN_FILENO, &seq[2], 1) != 1)
          return '\x1b';
        if (seq[2] == '~') {
          switch (seq[1]) {
            case '5':
              return PAGE_UP;
            case '6':
              return PAGE_DOWN;
          }
        }
      } else {
        switch (seq[1]) {
          case 'A':
            return ARROW_UP;
          case 'B':
            return ARROW_DOWN;
          case 'C':
            return ARROW_RIGHT;
          case 'D':
            return ARROW_LEFT;
        }
      }
    }
    return '\x1b';
  } else {
    return c;
  }
}

int GetCursorPosition(int* rows, int* cols) {
  char buf[32];
  unsigned int i = 0;
  if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4)
    return -1;
  while (i < sizeof(buf) - 1) {
    if (read(STDIN_FILENO, &buf[i], 1) != 1)
      break;
    if (buf[i] == 'R')
      break;
    i++;
  }
  buf[i] = '\0';
  printf("\r\n&buf[1]: '%s'\r\n", &buf[1]);
  EditorReadKey();

  if (buf[0] != '\x1b' || buf[1] != '[')
    return -1;
  if (sscanf(&buf[2], "%d;%d", rows, cols) != 2)
    return -1;
  return 0;
}

int GetWindowSize(int* rows, int* cols) {
  struct winsize ws;
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12)
      return -1;

    return GetCursorPosition(rows, cols);
  } else {
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
  }
}

/*** append buffer ***/

struct abuf {
  char* b;
  int len;
};

#define ABUF_INIT {NULL, 0}

void AbAppend(struct abuf* ab, const char* s, int len) {
  char* new_ = static_cast<char*>(realloc(ab->b, ab->len + len));
  if (new_ == NULL)
    return;
  memcpy(&new_[ab->len], s, len);
  ab->b = new_;
  ab->len += len;
}

void AbFree(struct abuf* ab) {
  free(ab->b);
}

/*** input ***/

void EditorMoveCursor(int key) {
  switch (key) {
    case ARROW_LEFT:
      if (E.cx != 0) {
        E.cx--;
      }
      break;
    case ARROW_RIGHT:
      if (E.cx != E.screencols - 1) {
        E.cx++;
      }
      break;
    case ARROW_UP:
      if (E.cy != 0) {
        E.cy--;
      }
      break;
    case ARROW_DOWN:
      if (E.cy != E.screenrows - 1) {
        E.cy++;
      }
      break;
  }
}

void EditorProcessKeypress() {
  int c = EditorReadKey();

  switch (c) {
    case CTRL_KEY('q'):
      write(STDOUT_FILENO, "\x1b[2J", 4);
      write(STDOUT_FILENO, "\x1b[H", 3);
      exit(0);
      break;

    case PAGE_UP:
    case PAGE_DOWN: {
      int times = E.screenrows;
      while (times--)
        EditorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
    } break;

    case ARROW_UP:
    case ARROW_DOWN:
    case ARROW_LEFT:
    case ARROW_RIGHT:
      EditorMoveCursor(c);
      break;
  }
}

/*** output ***/

void EditorDrawRows(struct abuf* ab) {
  int y;
  for (y = 0; y < E.screenrows; y++) {
    if (y == E.screenrows / 3) {
      char welcome[80];
      int welcomelen = snprintf(welcome, sizeof(welcome),
                                "Kilo editor -- version %s", KILO_VERSION);
      if (welcomelen > E.screencols)
        welcomelen = E.screencols;

      int padding = (E.screencols - welcomelen) / 2;
      if (padding) {
        AbAppend(ab, "~", 1);
        padding--;
      }
      while (padding--)
        AbAppend(ab, " ", 1);

      AbAppend(ab, welcome, welcomelen);
    } else {
      AbAppend(ab, "~", 1);
    }

    AbAppend(ab, "\x1b[K", 3);
    if (y < E.screenrows - 1) {
      AbAppend(ab, "\r\n", 2);
    }
  }
}

void EditorRefreshScreen() {
  struct abuf ab = ABUF_INIT;

  AbAppend(&ab, "\x1b[?25l", 6);
  // AbAppend(&ab, "\x1b[2J", 4);
  AbAppend(&ab, "\x1b[H", 3);

  EditorDrawRows(&ab);

  char buf[32];
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy + 1, E.cx + 1);
  AbAppend(&ab, buf, strlen(buf));

  AbAppend(&ab, "\x1b[H", 3);
  AbAppend(&ab, "\x1b[?25h", 6);

  write(STDOUT_FILENO, ab.b, ab.len);
  AbFree(&ab);
}

/*** init ***/

void InitEditor() {
  E.cx = 0;
  E.cy = 0;

  if (GetWindowSize(&E.screenrows, &E.screencols) == -1)
    Die("getWindowSize");
}

int main() {
  EnableRawMode();
  InitEditor();

  while (1) {
    EditorRefreshScreen();
    EditorProcessKeypress();
  }

  return 0;
}
