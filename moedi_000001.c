#include <stdio.h>
#include <unistd.h>
/* termios.h provides an interface to configure how a UNIX terminal handles
 * input and output */
#include <termios.h>

/*
 * By default, our terminal starts in canonical mode, also called cooked mode.
 * In this mode, keyboard input is only sent to our program when the we press
 * enter.
 *
 * Another one is raw mode, where each keypress is sent to the program
 * immediately.
 */

/*
 * Turning off echoing:
 * We can set a terminal's attribute by
 *  - using tcgetattr() to read the current attribute into a struct
 *  - modifying the struct by hand
 *  - passing the modified struct to tcsetattr() to write the new terminal
 * attributes.
 */

void enableRawMode() {
  struct termios raw;
  tcgetattr(STDIN_FILENO, &raw);
  raw.c_lflag &= ~(ECHO);

  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
  /* TCSAFLUSH specifies when to apply terminal settings and what to do with
   * pending input
   *
   *  There are three possible options:
   *   - TCSANOW: Apply changes immediately
   *   - TCSADRAIN: Wait until pending output has been transmitted, then apply
   * changes
   *   - TCSAFLUSH: Wait until pending output has been transmitted, discard
   * unread input, then apply changes
   */

  /* The ECHO feature causes each key we type to be printed on the terminal, so
   * we can see what we're typing */

  /* Modes:
   * - tcflag_t c_iflag: Input mode - controls how incoming characters are
   * processes before the program receives them
   * - tcflag_t c_oflag: Output mode - controls how characters written by the
   * program are processed before reaching the terminal
   * - tcflag_t c_cflag: Control mode - controls terminal hardware and
   * communication characteristics, such as character size and baud rate
   * - tcflag_t c_lflag: Local mode - controls terminal-side behaviour such as
   * line buffering, echoing and signal generation
   */

  /*
   * ECHO is a bitflag, defined as 00000000000000000000000000001000 in binary.
   * We use the bitwise-NOT operator(~) on this value to get
   * 11111111111111111111111111110111. We then bitwise-AND this value with the
   * flag field, which forces the fourth bit in the flags field to become 0, and
   * causes every other bit to retain its current value.
   */
}

int main() {
  enableRawMode();

  char c;
  while (read(STDIN_FILENO, &c, 1) == 1 && c != 'q')
    ;
  return 0;
}
