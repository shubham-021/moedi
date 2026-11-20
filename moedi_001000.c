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

int getCursorPosition(int *rows, int *cols) {
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

  /* Parsing for rows and cols */
  if (buf[0] != '\x1b' || buf[1] != '[')
    return -1;
  if (sscanf(&buf[2], "%d;%d", rows, cols) != 2)
    return -1;

  return 0;
}
/*
 * sscanf() is used to read formatted data from a string, just like scanf reads
 * formatted data from standard input. The important distinction is:
 *   - scanf: reads from stdin
 *   - sscanf: reads from a string already present in memory
 *
 * These belong to a family of formatted input/output functions provided by the
 * standard I/O library (stdio.h)
 *
 * 1. The printf family - formatted output
 *  - printf: prints formatted output to stdout
 *  - fprintf: prints formatted output to a specified stream
 *  - sprintf: writes formatted output into a string
 *  - snprintf: writes formatted output into a string with a size limit
 *
 * 2. The scanf family - formatted input
 *  - scanf: reads formatted input from stdin
 *  - fscanf: reads formatted input from a specified stream
 *  - sscanf: reads formatted input from a string
 *
 * 3. File I/O functions:
 *  - fopen: opens a file
 *  - fclose: closes a file
 *  - fread: reads binary data
 *  - fwrite: writes binary data
 *  - fgetc: reads one character
 *  - fputc: writes one character
 *  - fgets: reads a line/string
 *  - fputs: writes a string
 *  - feof: checks whether EOF indicator is set
 *  - ferror: checks whether a stream error occurred
 *
 *  for ex:
 *   FILE *fp = fopen("test.txt", "r");
 *
 *   FILE * represents a C standard I/O stream. It is not the same thing as file
 * descriptor.
 *
 * 4. String manipulation functions:
 *  - strlen: gets string length
 *  - strcmp: compares two strings
 *  - strncmp: comapres first N characters
 *  - strcpy: copies a string
 *  - strncpy: copies up to N characters
 *  - strcat: concatenates strings
 *  - strchr: finds a character
 *  - strstr: finds a substring
 *  - strtok: splits a string into tokens
 *  - memcpy: copies memory
 *  - memmove: copies potentially overlapping memory
 *  - memset: fill memory with a byte
 *  - memcmp: compares memory
 */

int getWindowSize(int *rows, int *cols) {
  struct winsize ws;

  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12)
      return -1;
    return getCursorPosition(rows, cols);
  } else {
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
  }
}

/*
 * The last line of the screen doesnt seem to have a tilde. Thats because we are
 * writing '~\r\n' everytime, as a result the last loop write a tilde and then a
 * newline
 */
/*
void editorDrawRows() {
  int y;
  for (y = 0; y < E.screenrows; y++) {
    write(STDOUT_FILENO, "~\r\n", 3);
  }
}
*/

void editorDrawRows() {
  int y;
  for (y = 0; y < E.screenrows; y++) {
    write(STDOUT_FILENO, "~", 1);
    if (y < E.screenrows - 1) {
      write(STDOUT_FILENO, "\r\n", 2);
    }
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

/*
 * Not a good idea to make a whole bunch of small write()'s everytime we refresh
 * the screen. It would be better to do one big write(), to make sure the whole
 * screen updates at once. Otherwise there could be small unpredictable pauses
 * between write()'s, which would cause an annoying flicker effect.
 *
 * We want to replace all our write() calls with code that appends the string to
 * a buffer and then write() this buffer out at the end. C doesnt have dynamic
 * strings, so we'll create our own dynamic string type that supports one
 * operation: append
 */
