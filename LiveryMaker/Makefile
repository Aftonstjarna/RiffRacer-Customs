CXX ?= c++
CXXFLAGS ?= -O2 -g
CXXFLAGS += -std=c++17 -Wall -Wextra -Wpedantic -Werror
VERSION := $(shell cat ../VERSION)
CXXFLAGS += -DCUSTOMS_VERSION=\"$(VERSION)\"
OUT ?= ../build/livery-maker
LDFLAGS ?=
EXEEXT ?=
APP ?= reriff-livery-maker
LIBS = -lpng -lz -lcrypto $(shell pkg-config --libs libcurl)
CURLFLAGS = $(shell pkg-config --cflags libcurl)
all: $(OUT)/$(APP)$(EXEEXT)
$(OUT)/$(APP)$(EXEEXT): main.cpp core.hpp paint.hpp config_author.hpp economy.hpp car_preview.hpp car_hud_preview.hpp mod_editor.hpp mod_appearance.hpp mod_names.hpp error_dialog.hpp car_template.hpp surface_snap.hpp attachment_editor.hpp multiplier_style.hpp model_import.hpp model_import_ui.hpp texture_encode.hpp decimate.hpp update_channel.hpp update_ui.hpp ../NativeLinux/native_assets.hpp ../NativeLinux/native_common.hpp ../NativeLinux/archive.hpp
	mkdir -p $(OUT)
	$(CXX) $(CXXFLAGS) $(CURLFLAGS) main.cpp -o $@ $$(pkg-config --cflags --libs gtk+-3.0) $(LIBS) $(LDFLAGS)
$(OUT)/test-livery$(EXEEXT): test.cpp core.hpp paint.hpp ../NativeLinux/native_assets.hpp ../NativeLinux/native_common.hpp ../NativeLinux/archive.hpp
	mkdir -p $(OUT)
	$(CXX) $(CXXFLAGS) test.cpp -o $@ $(LIBS) $(LDFLAGS)
test: $(OUT)/test-livery$(EXEEXT) $(OUT)/test-origin$(EXEEXT) $(OUT)/test-model-import$(EXEEXT) $(OUT)/test-car-hud$(EXEEXT) $(OUT)/test-surface-snap$(EXEEXT) $(OUT)/test-update$(EXEEXT) $(OUT)/test-texture$(EXEEXT) $(OUT)/test-decimate$(EXEEXT)
	$(OUT)/test-livery$(EXEEXT)
	$(OUT)/test-origin$(EXEEXT)
	$(OUT)/test-model-import$(EXEEXT)
	$(OUT)/test-car-hud$(EXEEXT)
	$(OUT)/test-surface-snap$(EXEEXT)
	$(OUT)/test-update$(EXEEXT)
	$(OUT)/test-texture$(EXEEXT)
	$(OUT)/test-decimate$(EXEEXT)
.PHONY: all test

$(OUT)/test-mod-editor$(EXEEXT): test_mod_editor.cpp mod_editor.hpp core.hpp paint.hpp ../NativeLinux/native_assets.hpp
	mkdir -p $(OUT)
	$(CXX) $(CXXFLAGS) test_mod_editor.cpp -o $@ $(LIBS) $(LDFLAGS)

$(OUT)/test-mod-appearance$(EXEEXT): test_mod_appearance.cpp mod_appearance.hpp mod_names.hpp mod_editor.hpp core.hpp paint.hpp
	mkdir -p $(OUT)
	$(CXX) $(CXXFLAGS) test_mod_appearance.cpp -o $@ $(LIBS) $(LDFLAGS)

$(OUT)/test-surface-snap$(EXEEXT): test_surface_snap.cpp surface_snap.hpp mod_editor.hpp core.hpp paint.hpp
	mkdir -p $(OUT)
	$(CXX) $(CXXFLAGS) test_surface_snap.cpp -o $@ $(LIBS) $(LDFLAGS)

$(OUT)/test-model-import$(EXEEXT): test_model_import.cpp model_import.hpp texture_encode.hpp mod_editor.hpp mod_names.hpp mod_appearance.hpp economy.hpp car_template.hpp core.hpp paint.hpp
	mkdir -p $(OUT)
	$(CXX) $(CXXFLAGS) test_model_import.cpp -o $@ $(LIBS) $(LDFLAGS)

$(OUT)/test-multiplier$(EXEEXT): test_multiplier.cpp multiplier_style.hpp mod_editor.hpp core.hpp
	mkdir -p $(OUT)
	$(CXX) $(CXXFLAGS) test_multiplier.cpp -o $@ $(LIBS) $(LDFLAGS)

$(OUT)/test-car-hud$(EXEEXT): test_car_hud.cpp car_hud_preview.hpp car_preview.hpp multiplier_style.hpp mod_appearance.hpp
	mkdir -p $(OUT)
	$(CXX) $(CXXFLAGS) test_car_hud.cpp -o $@ $$(pkg-config --cflags --libs cairo) $(LIBS) $(LDFLAGS)

$(OUT)/test-economy$(EXEEXT): test_economy.cpp economy.hpp config_author.hpp mod_appearance.hpp mod_editor.hpp paint.hpp
	mkdir -p $(OUT)
	$(CXX) $(CXXFLAGS) test_economy.cpp -o $@ $(LIBS) $(LDFLAGS)

$(OUT)/test-origin$(EXEEXT): test_origin.cpp core.hpp ../NativeLinux/native_assets.hpp ../NativeLinux/native_common.hpp ../NativeLinux/archive.hpp
	mkdir -p $(OUT)
	$(CXX) $(CXXFLAGS) test_origin.cpp -o $@ $(LIBS) $(LDFLAGS)

$(OUT)/test-update$(EXEEXT): test_update.cpp update_channel.hpp core.hpp
	mkdir -p $(OUT)
	$(CXX) $(CXXFLAGS) test_update.cpp -o $@ $(LIBS) $(LDFLAGS)

$(OUT)/test-texture$(EXEEXT): test_texture.cpp texture_encode.hpp core.hpp ../NativeLinux/native_assets.hpp
	mkdir -p $(OUT)
	$(CXX) $(CXXFLAGS) test_texture.cpp -o $@ -lpng -lz -lcrypto $(LDFLAGS)

$(OUT)/test-decimate$(EXEEXT): test_decimate.cpp decimate.hpp
	mkdir -p $(OUT)
	$(CXX) $(CXXFLAGS) test_decimate.cpp -o $@ $(LDFLAGS)
