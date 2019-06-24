
CXXFLAGS+=-std=c++14 -O3 -g
CPPFLAGS+=-MMD

test: varint
	./varint

varint: $(patsubst %.cpp,%.o,$(wildcard *.cpp))
	$(CXX) -o $@ $^ -lz

clean:
	$(RM) *.o *.d varint

format:
	clang-format -i *.cpp

-include *.d
