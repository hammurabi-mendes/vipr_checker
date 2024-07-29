#ifndef FILE_HELPER_H

#include <unistd.h>
#include <fcntl.h>

#include <stdexcept>
#include <format>

#include <string>
#include <cstring>

using std::runtime_error;
using std::format;

using std::string;

struct FileHelper {
	// Space for output buffer
	constexpr static size_t OUTPUT_BUFFER_LENGTH = 64 * 1024 * 1024;

	int input_fd;
	int output_fd;

	char *output_buffer;
	size_t output_buffer_watermark;

	int open_input(const char *filename);
	void close_input();

	int open_output(const char *filename);
	void close_output();

	inline void flush_data(const char *buffer, size_t ntowrite) {
		size_t nwritten = 0;

		ssize_t result;

		while(ntowrite > 0) {
			result = write(output_fd, buffer + nwritten, ntowrite);

			if(result == -1) {
				fprintf(stderr, "Cannot write to client socket no. %d\n", output_fd);

				exit(EXIT_FAILURE);
			}

			nwritten += result;
			ntowrite -= result;
		}
	}

	inline void write_output(const char *message) {
		size_t message_size = strlen(message);

		size_t remaining = OUTPUT_BUFFER_LENGTH - output_buffer_watermark;

		if(message_size > remaining) {
			if(message_size >= OUTPUT_BUFFER_LENGTH) {
				flush_data(output_buffer, output_buffer_watermark);
				flush_data(message, message_size);

				output_buffer_watermark = 0;
			}
			else {
				memcpy(output_buffer + output_buffer_watermark, message, remaining);
				flush_data(output_buffer, OUTPUT_BUFFER_LENGTH);

				memcpy(output_buffer, message + remaining, message_size - remaining);

				output_buffer_watermark = (message_size - remaining);
			}
		}
		else {
			memcpy(output_buffer + output_buffer_watermark, message, message_size);

			output_buffer_watermark += message_size;
		}
	}
	
	FileHelper();
	~FileHelper();
};

#endif /* FILE_HELPER_H */