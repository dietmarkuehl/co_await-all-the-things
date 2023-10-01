NAME     = task-accu
CXXFLAGS = -W -Wall -g -std=c++20 -O2

.PHONY: default clean run

default: build/$(NAME)
	@build/$(NAME)

build/Makefile:
	@mkdir -p build
	cd build; cmake ..

build/$(NAME): $(NAME).cpp build/Makefile
	cd build; $(MAKE)

clean:
	$(RM) -r build
	$(RM) mkerr olderr
