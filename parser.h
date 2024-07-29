#ifndef PARSER_H
#define PARSER_H

#include <cstring>
#include <vector>

#include <format>

#include <stdexcept>

#include "basic_types.h"
#include "file_helper.h"
#include "LinearAllocator.hpp"

using std::string;
using std::vector;

using std::format;

using std::runtime_error;

class Parser {
	constexpr static long BUFFER_SIZE = 16384;

	// File descriptor
	int fd;

	// Data buffer: gets constantly resized down when tokenization consumes all data
	vector<char> buffer;

	// Points to the first byte of unconsumed data (or nullptr if a new line needs to be sourced)
	char *next_line;

	// Points to the last returned line
	char *line;

	// Points to the last returned token
	char *token;

	// Current line number from the input file
	unsigned long line_number;

	// End-of-file (EOF) flag
	bool eof;

	// Allocates all permanent strings in a linear buffer, reducing calls to malloc()
	LinearAllocator<char> linear_allocator;

	// File helper that provides some useful input/output functions
	FileHelper file_helper;

public:
	Parser(char *filename);
	virtual ~Parser();

	//////////////////////
	// Memory functions //
	//////////////////////

	inline char *get_stable_string(char *token) {
		char *stable_token = linear_allocator.allocate(strlen(token) + 1) ;

		strcpy(stable_token, token);

		return stable_token;
	}

	////////////////////////////
	// Core parsing functions //
	////////////////////////////

	template<typename T>
	using conversion_function = T(*)(const char *, char **, int);

	template<typename T, conversion_function<T> F>
	inline T convert(char *token, int line_number) {
		char *leftover;
		T converted;

		errno = 0;
		converted = F(token, &leftover, 10);

		if(errno != 0) {
			throw runtime_error(format("Error in line {}: {}\n", line_number, strerror(errno)));
		}

		if(*leftover != '\0') {
			throw runtime_error(format("Error in line {}: leftover bytes in token ({})\n", line_number, leftover));
		}

		return converted;
	} 

	inline long parse_long(char *token) {
		return convert<long, strtol>(token, line_number);
	}

	inline unsigned long parse_unsigned_long(char *token) {
		return convert<unsigned long, strtoul>(token, line_number);
	}

	inline Number parse_number(char *token) {
		if(strchr(token, '/') == nullptr) {
			return Number(get_stable_string(token));
		}
		else {
			char *fraction_token = token;

			char *numerator = strsep(&fraction_token, "/");
			char *numerator_stable = get_stable_string(numerator);
			char *denominator = strsep(&fraction_token, "/");
			char *denominator_stable = get_stable_string(denominator);

			if(fraction_token != nullptr) {
				throw runtime_error(format("Error in line {}: leftover bytes in token ({})\n", line_number, fraction_token));
			}

			return Number(numerator_stable, denominator_stable);
		}
	}

	inline Number parse_number_or_infinity(char *token) {
		if(strstr(token, "inf") == nullptr) {
			return parse_number(token);
		}
		else {
			if(strcmp(token, "inf") == 0) {
				Number number("inf");
				number.is_positive_infinity = true;

				return number;
			}

			if(strcmp(token, "-inf") == 0) {
				Number number("-inf");
				number.is_negative_infinity = true;

				return number;
			}

			throw runtime_error(format("Error in line {}: extraneous bytes in token ({})\n", line_number, token));
		}
	}

	inline long get_long() {
		char *token = get_token();

		if(!token) {
			throw runtime_error(format("Error in line {}: expected signed integral value\n", line_number));
		}

		return parse_long(token);
	}

	inline unsigned long get_unsigned_long() {
		char *token = get_token();

		if(!token) {
			throw runtime_error(format("Error in line {}: expected unsigned integral value\n", line_number));
		}

		return parse_unsigned_long(token);
	}

	inline Number get_number() {
		char *token = get_token();

		if(!token) {
			throw runtime_error(format("Error in line {}: expected numeric value\n", line_number));
		}

		return parse_number(token);
	}

	inline Number get_number_or_infinity() {
		char *token = get_token();

		if(!token) {
			throw runtime_error(format("Error in line {}: expected numeric value\n", line_number));
		}

		return parse_number_or_infinity(token);
	}

	///////////////////////////
	// Tokenization functions //
	///////////////////////////

	inline char *get_line() {
		line = strsep(&next_line, "\n");

		// If we have some leftover bytes
		while(next_line == nullptr && !eof) {
			int leftover = strlen(line);

			// If the leftover bytes were not in the beginning of the buffer,
			// move it to the beginning
			if(leftover != 0 && line != &buffer[0]) {
				memmove(&buffer[0], line, leftover);
			}

			// Adds the space for the new chunk
			buffer.resize(leftover + BUFFER_SIZE + 1);

			// Reads the new chunk into the larger buffer
			char *chunk = &buffer[leftover];
			int bytes_read;

			if((bytes_read = read(fd, chunk, BUFFER_SIZE)) <= 0) {
				eof = true;
			}

			chunk[bytes_read] = '\0';

			// Try again
			next_line = &buffer[0];
			line = strsep(&next_line, "\n");
		}

		// If EOF, considers the leftover bytes as a line of its own
		if(next_line == nullptr && eof) {
			if(line != nullptr && strlen(line) != 0) {
				return line;
			}

			return nullptr;;
		}

		line_number++;

		return line;
	}

	inline char *get_token() {
		// If there's no line to get token, obtain one
		if(line == nullptr) {
			line = get_line();

			// If the end of file is reached, there is no token
			if(line == nullptr) {
				return nullptr;
			}
		}

		while(true) {
			token = strsep(&line, " \t\n");

			while(token[0] != '\0' && isspace(token[0])) {
				token++;
			}

			if(token[0] != '\0') {
				break;
			}

			// If there's no line to get token, obtain one
			if(line == nullptr) {
				line = get_line();

				// If the end of file is reached, there is no token
				if(line == nullptr) {
					return nullptr;
				}
			}
		}

		return token;
	}

	/////////////////////////////
	// Getter/setter functions //
	/////////////////////////////

	inline unsigned long get_line_number() {
		return line_number;
	}
};

#endif /* PARSER_H */