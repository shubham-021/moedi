#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#define CTRL_KEY(k) ((k) & 0x1f)

struct editorConfig {
  int screenrows;
  int screencols;
  struct termios orig_termios;
};

struct editorConfig E;

void die(const char *s) {
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);
  perror(s);
  exit(1);
}

void disableRawMode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
    die("tcsetattr");
}

void enableRawMode() {
  if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1)
    die("tcgetattr");
  atexit(disableRawMode);

  struct termios raw = E.orig_termios;
  raw.c_iflag &= ~(ICRNL | IXON | BRKINT | INPCK | ISTRIP);
  raw.c_oflag &= ~(OPOST);
  raw.c_cflag |= (CS8);
  raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN | INPCK);
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;

  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

char editorReadKey() {
  int nread;
  char c;

  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN)
      die("read");
  }

  return c;
}

/*
 * ioctl() isnt guaranteed to be able to request the window size on all system,
 * so we are going to provide a fallback method of getting window size
 *
 * The strategy is to position the cursor at the bottom-right of the screen,
 * then use escape sequence that let us query the position of the cursor. That
 * will tell us how many rows and column there must be on the screen
 *
 * There is no simple "move the cursor to the bottom-right corner" command.
 * We are sending two escape sequence one after the other. The C command  moves
 * the cursor to the right and the B command moves the cursor down. The argument
 * says how much to move it right or down by. We use a very large value, 999,
 * which should ensure that the cursor reaches the right and bottom edge of the
 * screen
 *
 * The C and B commands are specifically
 * documented(http://vt100.net/docs/vt100-ug/chapter3.html#CUD) to stop the
 * cursor from going past the edge of the screen. The reason we dont use the
 * <esc>[999;999H command is that the documentation doesnt specify what happens
 * when you try to move the cursor off-screen.
 *
 * We are sticking a `1 ||` at the front of our if condition temporarily, so
 * that we can test this fallback branch we are developing
 *
 * Because we are always returning -1(meaning an error ocurred) from
 * getWindowSize() at this point, we make a call to editorReadKey() so we can
 * observe the result of our escape sequence before the program calls die() and
 * clears the screen. When you run the program, you should see the cursor is
 * positioned at the bottom-right corner of the screen, and then when you press
 * a key you'll see the error message printed by die() after it clears the
 * screen.
 */
int getWindowSize(int *rows, int *cols) {
  struct winsize ws;

  if (1 || ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12)
      return -1;
    editorReadKey();
    return -1;
  } else {
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
  }
}

void editorDrawRows() {
  int y;
  for (y = 0; y < E.screenrows; y++) {
    write(STDOUT_FILENO, "~\r\n", 3);
  }
}

void editorRefreshScreen() {
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);

  editorDrawRows();

  write(STDOUT_FILENO, "\x1b[H", 3);
}

void editorProcessKeypress() {
  char c = editorReadKey();

  switch (c) {
  case CTRL_KEY('q'):
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    exit(0);
    break;
  }
}

void initEditor() {
  if (getWindowSize(&E.screenrows, &E.screencols) == -1)
    die("getWindowSize");
}

int main() {
  enableRawMode();
  initEditor();

  while (1) {
    editorRefreshScreen();
    editorProcessKeypress();
  }

  return 0;
}
