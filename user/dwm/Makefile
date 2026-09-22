PROGNAME=dwm
OUT=$(PROGNAME)

override SRCFILES := $(shell find -L src -type f 2>/dev/null | LC_ALL=C sort)
override CFILES := $(filter %.c,$(SRCFILES))
override CFILES := $(filter %.c,$(SRCFILES))
override CXXFILES := $(filter %.cpp,$(SRCFILES))
override OBJ := $(addprefix obj-$(ARCH)/,$(CFILES:.c=.c.o) $(CXXFILES:.cpp=.cpp.o))
CC=x86_64-ah5kos-gcc
CFLAGS= -Wall -Wextra -Wswitch-enum -g
CXX=x86_64-ah5kos-g++
CPPFLAGS_CXX= -Wall -Wextra -Wswitch-enum -g

all: $(OUT)

$(OUT): $(OBJ)
	@mkdir -p "$(dir $@)"
	@$(CXX) $(OBJ) -g -lm -o $@
	@echo " $<"

obj-$(ARCH)/%.c.o: %.c
	@mkdir -p "$(dir $@)"
	@$(CC) $(CFLAGS) -c $< -o $@
	@echo " $<"

obj-$(ARCH)/%.cpp.o: %.cpp
	@mkdir -p "$(dir $@)"
	@$(CXX) $(CXXFLAGS) $(CPPFLAGS_CXX) -c $< -o $@
	@echo " $<"

cpsysroot:
	@cp $(OUT) ../../sysroot/programs/$(OUT)
clean:
	@rm -rf $(OUT)
	@rm -rf obj-$(ARCH)