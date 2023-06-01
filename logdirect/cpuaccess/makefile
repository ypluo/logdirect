CFLAGS := -std=c++11 -msse2 -mavx -mrdrnd -fmax-errors=5 -fopenmp
LINK_LIB_FLAGS :=-pthread -fopenmp

SOURCES := $(wildcard *.cc)
OBJECTS := $(SOURCES:.cc=.o)

all: $(OBJECTS) 
	g++ -o bandwidth bandwidth.o access.o $(LINK_LIB_FLAGS)
	g++ -o latency latency.o access.o $(LINK_LIB_FLAGS)
	g++ -o write write.o $(LINK_LIB_FLAGS)
	@echo "finish compile"

%.o: %.cc
	g++ -I/usr/src/linux-headers-5.14.0+ $(MACRO) $(CFLAGS) -c -o $@ $<

.PONHY: clean
clean:
	@rm *.o bandwidth latency write
