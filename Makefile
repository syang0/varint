
CXXFLAGS+=-std=c++14 -O3 -g
CPPFLAGS+=-MMD

test: varint
	./varint

varint: $(patsubst %.cpp,%.o,$(wildcard *.cpp)) libsnappy.a
	$(CXX) -o $@ $^ -lz -L. -lsnappy

clean:
	$(RM) *.o *.d varint

format:
	clang-format -i *.cpp

SNAPPY_DIR=./snappy/

$(SNAPPY_DIR)/installDir/lib/libsnappy.a:
	cd $(SNAPPY_DIR) && \
	./autogen.sh && \
	CFLAGS="-O3 -DNDEBUG" CXXFLAGS="-O3 -DNDEBUG" ./configure && \
	make -j10 && \
	make install prefix=$(realpath $(SNAPPY_DIR))/installDir

libsnappy.a: $(SNAPPY_DIR)/installDir/lib/libsnappy.a
	cp $(SNAPPY_DIR)/installDir/lib/libsnappy.a .

-include *.d