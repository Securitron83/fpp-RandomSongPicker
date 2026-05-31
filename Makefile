PLUGIN_NAME := fpp-RandomSongPicker
SONAME      := lib$(PLUGIN_NAME).so

# FPP source tree — override with: make FPP_SRC=/path/to/fpp/src
FPP_SRC ?= /opt/fpp/src

CXX      ?= g++
CXXFLAGS  = -std=gnu++20 -O2 -fPIC -Wall -Wextra
CXXFLAGS += -I$(FPP_SRC) -DNOPCH
CXXFLAGS += $(shell pkg-config --cflags jsoncpp 2>/dev/null || echo "-I/usr/include/jsoncpp")
CXXFLAGS += -Wno-unused-parameter -Wno-reorder

LDFLAGS   = -shared -fPIC
LDFLAGS  += -Wl,-rpath,'$$ORIGIN/../../../../src'
LDFLAGS  += -Wl,-rpath,$(FPP_SRC)
LDLIBS    = -L$(FPP_SRC) -lfpp -ljsoncpp

SRCS := src/RandomSongPickerPlugin.cpp
OBJS := $(SRCS:.cpp=.o)

.PHONY: all install clean

all: $(SONAME)

$(SONAME): $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

INSTALL_DIR ?= /home/fpp/media/plugins/$(PLUGIN_NAME)

install: $(SONAME)
	install -m 755 $(SONAME) $(INSTALL_DIR)/

clean:
	rm -f $(OBJS) $(SONAME)
