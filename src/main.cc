#include <iostream>
#include <unistd.h>
#include <termios.h>

using namespace std;

void EnableRawMode() {
  struct termios raw;
  tcgetattr(STDIN_FILENO, &raw);

  raw.c_lflag &= ~(ECHO);

  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

int main() {
  EnableRawMode();

  char c;
  while (read(STDIN_FILENO, &c, 1) == 1 && c != 'q');

  return 0;
}
