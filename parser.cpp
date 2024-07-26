#include <unistd.h>
#include <fcntl.h>

#include "parser.h"

Parser::Parser(char *filename): line{nullptr}, token{nullptr}, eof{false} {
    fd = file_helper.open_input(filename);

    buffer.resize(BUFFER_SIZE + 1);
    buffer[0] = '\0';

    next_line = &buffer[0];

    line = nullptr;
    token = nullptr;

    line_number = 0;
}

Parser::~Parser() {
    file_helper.close_input();
}