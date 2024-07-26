#include <unistd.h>
#include <fcntl.h>

#include "file_helper.h"

int FileHelper::open_input(const char *filename) {
    input_fd = open(filename, O_RDONLY);

    if(input_fd == -1) {
        throw runtime_error(format("Error opening {}\n", filename));
	}

#ifdef LINUX
    if(!posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL)) {
        perror("Error advising for sequential access (proceeding anyway)");
    }
#endif /* LINUX */

	return input_fd;
}

void FileHelper::close_input() {
    close(input_fd);
}

int FileHelper::open_output(const char *filename) {
	output_fd = open(filename, O_CREAT | O_TRUNC | O_WRONLY | O_APPEND | 0644, 0644);

	if(output_fd == -1) {
		throw runtime_error(format("Error opening {}\n", filename));

		exit(EXIT_FAILURE);
	}

	output_buffer = new char[FileHelper::OUTPUT_BUFFER_LENGTH];

	return output_fd;
}

void FileHelper::close_output() {
	if(output_buffer != nullptr) {
		flush_data(output_buffer, output_buffer_watermark);

		delete output_buffer;	
	}

	output_buffer = nullptr;
	output_buffer_watermark = 0;
}
	
FileHelper::FileHelper(): input_fd{-1}, output_fd{-1}, output_buffer{nullptr}, output_buffer_watermark{0UL} {
}

FileHelper::~FileHelper() {
}