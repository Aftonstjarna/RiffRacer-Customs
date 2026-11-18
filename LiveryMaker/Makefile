CXX ?= c++
CXXFLAGS ?= -O2 -g
CXXFLAGS += -std=c++17 -Wall -Wextra -Wpedantic -Werror
OUT ?= ../build/livery-maker
LDFLAGS ?=
EXEEXT ?=
APP ?= reriff-livery-maker
LIBS = -lpng -lz -lcrypto
all: $(OUT)/$(APP)$(EXEEXT)
$(OUT)/$(APP)$(EXEEXT): main.cpp core.hpp paint.hpp ../NativeLinux/native_assets.hpp ../NativeLinux/native_common.hpp ../NativeLinux/archive.hpp
	mkdir -p $(OUT)
	$(CXX) $(CXXFLAGS) main.cpp -o $@ $$(pkg-config --cflags --libs gtk+-3.0) $(LIBS) $(LDFLAGS)
$(OUT)/test-livery$(EXEEXT): test.cpp core.hpp paint.hpp ../NativeLinux/native_assets.hpp ../NativeLinux/native_common.hpp ../NativeLinux/archive.hpp
	mkdir -p $(OUT)
	$(CXX) $(CXXFLAGS) test.cpp -o $@ $(LIBS) $(LDFLAGS)
test: $(OUT)/test-livery$(EXEEXT)
	$(OUT)/test-livery$(EXEEXT)
.PHONY: all test
