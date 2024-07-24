#ifndef LINEAR_ALLOCATOR_H
#define LINEAR_ALLOCATOR_H

#include <vector>

using std::vector;

template<typename T>
class LinearAllocator {
    constexpr static size_t BUFFER_SIZE = 1024 * 1024 * 1024;

private:
    vector<char *> buffers;

	char *current_buffer;
    size_t current_buffer_watermark;

public:
    LinearAllocator() {
		add_buffer();
    }

    ~LinearAllocator() {
        cleanup();
    }

    inline void cleanup() noexcept {
		for(auto &buffer: buffers) {
			delete buffer;
		}
    }

    inline void add_buffer() noexcept {
		char *new_buffer = new char[BUFFER_SIZE];

		buffers.emplace_back(new_buffer);

		current_buffer = new_buffer;
		current_buffer_watermark = 0;
    }

    inline T *allocate(size_t quantity) noexcept {
		char *old = current_buffer + current_buffer_watermark;

		current_buffer_watermark += (quantity * sizeof(T));

		if(current_buffer_watermark >= BUFFER_SIZE) {
			add_buffer();

			old = current_buffer;
		}

		return (T *) old;
	}
};

#endif /* LINEAR_ALLOCATOR_H */