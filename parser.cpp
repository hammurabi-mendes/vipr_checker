#include <unistd.h>
#include <fcntl.h>

#include "parser.h"

Parser::Parser(char *filename): line{nullptr}, token{nullptr}, eof{false} {
    fd = open(filename, O_RDONLY);

    if(fd == -1) {
        throw runtime_error("Error building parser");
	}

#ifdef LINUX
    if(!posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL)) {
        perror("Error advising for sequential access (proceeding anyway)");
    }
#endif /* LINUX */

    buffer.resize(BUFFER_SIZE + 1);
    buffer[0] = '\0';

    next_line = &buffer[0];

    line = nullptr;
    token = nullptr;

    line_number = 0;
}

Parser::~Parser() {
    close(fd);
}