CC=$(CROSS_COMPILE)gcc
CXX=$(CROSS_COMPILE)g++

targets: h264-ts-no-audio

all: $(targets)

clean:
	rm -f $(targets)
	rm -f *.o

%: %.cpp
	$(CXX) --std=c++14 -DASIO_STANDALONE -DASIO_DISABLE_THREADS $^ -o $@

.PHONY: all clean
