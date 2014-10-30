CXX=arm-unknown-linux-gnueabi-c++
ASIO_FLAGS=-DASIO_STANDALONE -DASIO_DISABLE_THREADS

mpeg-test: mpeg-test.cpp
	$(CXX) -std=c++11 $(ASIO_FLAGS) $^ -o $@

