#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

struct termios orig_termios;

void disableRawMode() { tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios); }

void enableRawMode() {
  tcgetattr(STDIN_FILENO, &orig_termios);
  atexit(disableRawMode);

  struct termios raw = orig_termios;
  /* Struct assignment creates a copy of the entire struct
   *
   *  Important:
   *   struct assignment is a shallow copy
   *
   *   struct Data {
   *     int *ptr;
   *   };
   *
   *   int x = 10;
   *
   *   struct Data b = {&x};
   *   struct Data c = b;
   *
   *   b and c are separate structs
   *   but b.ptr and c.ptr point to the same memory location
   */

  raw.c_iflag &= ~(ICRNL | IXON | BRKINT | INPCK | ISTRIP);
  /*  By default, Ctrl-S and Ctrl-Q are used for software flow control. Ctrl-S
   * stops data from being transmitted to the terminal until you press Ctrl-Q.
   *
   *  IXON comes from termios.h. The I stands for input flag (which it is,
   * unlike the other I flags we've seen so far) and XON comes from the names of
   *  the two control characters that Ctrl-S and Ctrl-Q produce: XOFF to pause
   * transmission and XON to resume transmission
   *
   *
   * When we press Ctrl along with a letter, the terminal generates a control
   * character. Ctrl-A : ascii decimal = 1 : character = SOH Ctrl-B : ascii
   * decimal = 2 : character = STX Ctrl-C : ascii decimal = 3 : character = ETX
   * .
   * .
   * .
   *
   * Ctrl-J generates ASCII 10 (Line feed)
   * Ctrl-M generates ASCII 13 (carriage return)
   *
   * Historically, terminals inherited two different characters for moving to a
   * new line: carriage return (\r) and line feed (\n) By default, the
   * terminal's line discipline has an input flag called ICRNL (Input Carriage
   * Return to NewLine). When enabled, the terminal translates: '\r' into '\n',
   * So when you press Enter, the terminal typically sends \r, but the line
   * discipline converts it to \n.
   *
   * Thats why we disable this with flag ICRNL, so we can recieve the true
   * pressed key without any conversion
   *
   * ICRNL comes from termios.h, the I stands for "input flag". CR stands for
   * carriage return and NL stands for new line
   *
   *
   * It turns out that terminal does a similar translation on the output side.
   * It translates each newline (\n) we print into a carriage return followed by
   * a newline (\r\n). The terminal requires both of these character in order to
   * start a new line of text. The cariage return moves the cursor back to the
   * beginnig of the current line, and the newline moves the cursor down a line,
   * scrolling the screen if necessary.
   *
   * We turn off these output processing feature by turning off the OPOST flag.
   *
   * OPOST comes from termios.h, O means its an output flag.
   */

  raw.c_oflag &= ~(OPOST);
  raw.c_cflag |= (CS8);

  raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN | INPCK);
  /* ICANON flag allows us turn off canonical mode. This means we will finally
   * be reading input byte-by-byte, instead of line by line
   *
   * By default, ctrl-c sends a SIGINT signal to the current process which
   * causes it to terminate and ctrl-z sends a SIGTSTP signal to the current
   * process which causes it to suspend. We are turning off the sending of both
   * these signal using the ISIG flag
   *
   * On some systems, when you type Ctrl-V, the terminal waits for you to type
   * another character and then sends that character literally For example:
   * before we disabled ctrl-C we might've been able to type ctrl-V and then
   * ctrl-C to input a 3 byte. We can turn off this feat using the IEXTEN flag.
   *
   * Turning off IEXTEN also fixes ctrl-o in mac-OS, whose terminal driver is
   * otherwise set to discard that control character
   *
   * IEXTEN comes from termios.h. It is naother flag that starts with I but
   * belongs in the c_lflag field
   */

  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

int main() {
  enableRawMode();

  // checking if IXON is already OFF: yes it is
  // printf("IXON: %s\n", (orig_termios.c_iflag & IXON) ? "ON" : "OFF");

  char c;
  while (read(STDIN_FILENO, &c, 1) == 1 && c != 'q') {
    if (iscntrl(c)) {
      printf("%d\r\n", c);
    } else {
      printf("%d ('%c')\r\n", c, c);
    }
  }

  /* iscntrl tests whether a character is a control character. Control character
   * are nonprintable characters. ASCII Codes 0-31 are all control characters,
   * and 127 is also a control character. ASCII codes 32-126 are all printable.
   */
  return 0;
}
