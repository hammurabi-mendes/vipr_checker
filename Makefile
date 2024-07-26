CXX=clang++

ifeq ($(DEBUG), 1)
	CXXFLAGS=-std=c++20 -I. -g -pthread
else
	CXXFLAGS=-std=c++20 -I. -O3 -march=native -fno-strict-aliasing -flto -pthread
endif

# Add or remove -DPARALLEL to generate/process SMT files in parallel
FLAGS=-DPARALLEL
LDFLAGS=

PROGRAMS=vipr_checker
OBJECTS=main.o parser.o certificate.o

all: $(PROGRAMS)

vipr_checker: main.o parser.o certificate.o
	$(CXX) $(CXXFLAGS) $(FLAGS) -o $@ $(OBJECTS) $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(FLAGS) -c $< -o $@

clean:
	rm -f *.o $(PROGRAMS)
