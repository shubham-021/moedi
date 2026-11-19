#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

/*
 * ASCII has a special range preserved for control characters:
 *   - Values 0-31: Control characters
 *   - Values 32-126: Printable characters
 *
 * For example:
 *   Ctrl-A: 1: 00000001
 *   Ctrl-B: 2: 00000010
 *   Ctrl-C: 3: 00000011
 *   .
 *   .
 *   .
 *   Ctrl-Q: 17: 00010001
 *   .
 *   .
 *   Ctrl-Z: 26: 00011010
 *
 * Corresponding lowercase letters:
 *   a: 97: 01100001
 *   b: 98: 01100010
 *   c: 99: 01100011
 *   .
 *   .
 *   .
 *   q: 113: 01110001
 *   .
 *   .
 *   z: 122: 01111010
 *
 * The lower five bits of each lowercase letter are identical to the
 * corresponding Ctrl character. The upper three bits are different.
 *
 *   0x1f: 31: 00011111
 *
 * Bitwise AND follows one simple rule:
 *   - 1 & 1 = 1
 *   - Everything else produces 0
 *
 * The upper three bits are cleared and the lower five bits are preserved.
 *
 * Therefore Bitwise AND of control characters with 0x1f yields the
 * corresponding lowercase character.
 */
#define CTRL_KEY(k) ((k) & 0x1f)

/*
 * Global struct to keep track of our editor state
 */
struct editorConfig {
  int screenrows;
  int screencols;
  struct termios orig_termios;
};

/* Our global variable containing our editor state is named E */
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
 * ioctl: input/ output control
 * It's a system call used to perform operations on file descriptors that dont
 * fit into the usual read() and write() operations. for example:
 *  - Get terminal window dimensions
 *  - Configure certain terminal properties
 *  - Control device-specific operations
 *
 * The arguments are:
 *  - fd: file descriptor of the target
 *  - request: What operation you want to perform
 *  - ....: Additional argument required by that operation
 *
 *  STDOUT_FILENO: When you run a terminal application, its standard output is
 * generally connected to the terminal where it is running TIOCGWINSZ: TIO:
 * terminal I/O, CG: Get, WINSZ: Window size it tells the kernel that we want to
 * retrieve the terminal's dimensions
 *
 *  struct winsize {
 *    unsigned short ws_row;
 *    unsigned short ws_col;
 *    unsigned short ws_xpixel;
 *    unsigned short ws_ypixel;
 *  }
 */
int getWindowSize(int *rows, int *cols) {
  struct winsize ws;

  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
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
  /*
   * first byte: \x1b: escape character: 27(decimal)
   * other three bytes: [2J
   *
   * We are writing an escape sequence to the terminal. Escape sequences always
   * start with an escape character(27) followed by [ character. Escape sequence
   * instruct the terminal to do various text formatting tasks, such as coloring
   * text, moving the cursor around and clearing parts of the screen
   *
   * We are using the J command to clear the scrren. Escape sequence commands
   * take arguments, which come before the command. In this case the argument is
   * 2, which says to clear the entire screen. <esc>[1J would clear the screen
   * up to where the cursor is, and <esc>[0J would clear the screen from the
   * cursor up to the end of the screen. Also, 0 is the default argument for J,
   * so just <esc>[J by itself would also clear the screen from the cursor to
   * the end
   *
   * http://vt100.net/docs/vt100-ug/chapter3.html
   *
   * <esc>[2J command left the cursor at the bottom of the screen, <esc>[H will
   * resposition it at the top-left corner
   */
  write(STDOUT_FILENO, "\x1b[2J", 4);

  /*
   * This escape sequence is only 3 bytes long, and uses the H command to
   * position the cursor. The H command actually takes two arguments: the row
   * number and the column number at which to position the cursor. So if we have
   * an 80x24 size terminal and we want the cursor in the center of the screen,
   * we could use the command <esc>[12;40H. (Multiple arguments are separated by
   * a ; character). The default arguments for H both happen to be 1, so we can
   * leave both arguments out and it will position the cursor at the first row
   * and first coloumn, as if we had sent the <esc>[1;1H command. (Rows and
   * columns are numbered starting at 1 not 0)
   */
  write(STDOUT_FILENO, "\x1b[H", 3);

  editorDrawRows();

  write(STDOUT_FILENO, "\x1b[H", 3);
}

void editorProcessKeypress() {
  char c = editorReadKey();

  switch (c) {
  /*
   * Mapping Ctrl-Q to quit operation
   */
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
