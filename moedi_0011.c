#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

struct termios orig_termios;

void die(const char *s) {
  perror(s);
  exit(1);
}

void disableRawMode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1)
    die("tcsetattr");
}

void enableRawMode() {
  if (tcgetattr(STDIN_FILENO, &orig_termios) == -1)
    die("tcgetattr");

  atexit(disableRawMode);

  struct termios raw = orig_termios;

  raw.c_iflag &= ~(ICRNL | IXON | BRKINT | INPCK | ISTRIP);
  raw.c_oflag &= ~(OPOST);
  raw.c_cflag |= (CS8);
  raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN | INPCK);

  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;
  /*
   * VMIN and VTIME control how read() behaves when reading input from a
   * terminal in noncanonical mode(raw mode).
   *
   * 1. VMIN - Minimum number of bytes
   *  It specifies the minimum number of bytes that must be available before
   * read() returns
   *
   * 2. VTIME - Timeout
   *  It specifies a timeout in units of 100ms (0.1 seconds)
   *
   * VMIN and VTIME come from termios.h, they are indexes into the c_cc field,
   * which stands for "control characters", an array of bytes that control
   * various terminal settings.
   *
   * When we run the program, we can see how often read() times out. If you
   * don't supply any input, read() returns without setting the c variable,
   * which retains its 0 value and so we see 0s getting printed out. If we type
   * really fast, we can see that read() returns right away after each keypress,
   * so its not like we can only read one keypress every tenth of a second.
   */

  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

int main() {
  enableRawMode();

  while (1) {
    char c = '\0';
    if (read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN)
      die("read");

    if (iscntrl(c)) {
      printf("%d\r\n", c);
    } else {
      printf("%d ('%c')\r\n", c, c);
    }

    if (c == 'q')
      break;
  }

  return 0;
}
