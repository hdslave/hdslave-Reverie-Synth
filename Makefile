# Reverie: a pad and chord synthesizer as a CLAP plug-in for the Akai MPC Sample (64-bit ARM Linux, Cortex-A55).
# Build inside a Linux arm64 image with ./build.sh, which runs `make all test`.
#   make            build/Reverie.clap
#   make test       the tests against the built plug-in (tests/host.cpp) and the sound's fingerprint
#                   (tests/fingerprint.cpp: the output to the bit, against tests/fingerprint.txt)
#   make bench      build/bench: renders busy scenes; tools/bench.sh counts their instructions
#   make sanitize   the host tests against a build with AddressSanitizer and UBSan (any error stops it)
#   make render     build/render: renders progressions and sounds to WAV (tests/render.cpp), for listening
# Each needs build/voicings.inc, the chords every pad plays (tools/voicings.cpp writes it; src/voicings.h reads it).
CXX ?= g++
OUT := build
WARN := -Wall -Wextra -Werror
# The MPC's cores. -ffp-contract=fast lets the compiler fuse multiply-adds; nothing here relies on IEEE errno, traps or
# exceptions.
OPT := -O3 -mcpu=cortex-a55 -ffp-contract=fast -fno-math-errno -fno-trapping-math
BASE := -std=c++17 -fno-exceptions -fno-rtti -fvisibility=hidden -fPIC -Ithird_party/clap/include -Isrc -I$(OUT)
CXXFLAGS := $(BASE) $(OPT)
# Only clap_entry is exported; libstdc++ and libgcc are linked in so the MPC needs nothing beyond libc and libm. -s leaves
# out the symbol table.
LDFLAGS := -shared -s -static-libgcc -static-libstdc++ -Wl,--gc-sections -Wl,-z,now -Wl,-O1 -Wl,--version-script=src/exports.map
PLUGIN_SOURCES := src/reverie.cpp src/screen.cpp
HEADERS := $(wildcard src/*.h src/ui/*.h) $(OUT)/voicings.inc

.PHONY: all test bench clean fingerprint-update sanitize render
all: $(OUT)/Reverie.clap

# Every pad's chord in every progression, voicing and key, voiced here with the plug-in's flags (the search's fused
# multiply-adds decide a few near ties), so the plug-in only reads them.
$(OUT)/voicings.inc: tools/voicings.cpp src/chords.h src/params.h src/progressions.h Makefile
	@mkdir -p $(OUT)
	$(CXX) $(CXXFLAGS) $(WARN) tools/voicings.cpp -o $(OUT)/voicings
	$(OUT)/voicings > $@.tmp
	mv $@.tmp $@

$(OUT)/Reverie.clap: $(PLUGIN_SOURCES) $(HEADERS) src/exports.map Makefile
	@mkdir -p $(OUT)
	$(CXX) $(CXXFLAGS) $(WARN) -ffunction-sections -fdata-sections $(PLUGIN_SOURCES) $(LDFLAGS) -o $@

$(OUT)/host: tests/host.cpp tests/wav.h tests/png.h $(HEADERS) Makefile
	@mkdir -p $(OUT)
	$(CXX) -std=c++17 -O2 $(WARN) -Wno-stringop-overflow -Ithird_party/clap/include -I$(OUT) tests/host.cpp -ldl -o $@

$(OUT)/fingerprint: tests/fingerprint.cpp $(HEADERS) Makefile
	@mkdir -p $(OUT)
	$(CXX) -std=c++17 -O2 $(WARN) -Ithird_party/clap/include tests/fingerprint.cpp -ldl -o $@

$(OUT)/render: tests/render.cpp tests/wav.h $(HEADERS) Makefile
	@mkdir -p $(OUT)
	$(CXX) -std=c++17 -O2 $(WARN) -Ithird_party/clap/include -Isrc tests/render.cpp -ldl -o $@

render: $(OUT)/render $(OUT)/Reverie.clap

test: $(OUT)/Reverie.clap $(OUT)/host $(OUT)/fingerprint
	$(OUT)/host $(OUT)/Reverie.clap $(OUT)
	$(OUT)/fingerprint $(OUT)/Reverie.clap tests/fingerprint.txt > /dev/null

# The sound's hashes after a change meant to change the sound (see tests/fingerprint.cpp).
fingerprint-update: $(OUT)/Reverie.clap $(OUT)/fingerprint
	$(OUT)/fingerprint $(OUT)/Reverie.clap > tests/fingerprint.txt

# -fexpensive-optimizations: the host's own voicing search (checked against the table) fuses multiply-adds as the
# plug-in's build does.
SAN := -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer -g -O1 -fexpensive-optimizations
sanitize: $(HEADERS) $(PLUGIN_SOURCES) tests/host.cpp tests/wav.h tests/png.h Makefile
	@mkdir -p $(OUT)/san
	$(CXX) $(BASE) $(SAN) -mcpu=cortex-a55 -shared $(PLUGIN_SOURCES) -o $(OUT)/san/Reverie.clap
	$(CXX) -std=c++17 $(SAN) $(WARN) -Wno-stringop-overflow -Ithird_party/clap/include -I$(OUT) tests/host.cpp -ldl -o $(OUT)/san/host
	ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1:abort_on_error=1:print_stacktrace=1 \
	    $(OUT)/san/host $(OUT)/san/Reverie.clap $(OUT)/san

$(OUT)/bench: tests/bench.cpp $(PLUGIN_SOURCES) $(HEADERS) Makefile
	@mkdir -p $(OUT)
	$(CXX) $(CXXFLAGS) $(WARN) -g tests/bench.cpp src/screen.cpp -o $@

bench: $(OUT)/bench

clean:
	rm -rf $(OUT)

# SP-16 uses the MPC voicing table and a separate six-encoder screen. Run the
# art generator on the build machine before making either SP-16 target.
SP16_CXX ?= arm-linux-gnueabi-g++
SP16_OPT := -O3 -march=armv7-a -mtune=cortex-a9 -mfpu=neon -mfloat-abi=softfp -ffp-contract=fast -fno-math-errno -fno-trapping-math
SP16_SOURCES := $(PLUGIN_SOURCES) src/screen_sp16.cpp
SP16_FLAGS := -DREVERIE_SP16 -ffunction-sections -fdata-sections
SP16_DEPS := $(SP16_SOURCES) $(HEADERS) $(OUT)/sp16_art.h src/exports.map Makefile

.PHONY: sp16 sp16-preview
sp16: $(OUT)/Reverie-sp16.clap
sp16-preview: $(OUT)/Reverie-sp16-preview.clap

$(OUT)/Reverie-sp16.clap: $(SP16_DEPS)
	$(SP16_CXX) $(BASE) $(SP16_OPT) $(WARN) $(SP16_FLAGS) $(SP16_SOURCES) $(LDFLAGS) -o $@

$(OUT)/Reverie-sp16-preview.clap: $(SP16_DEPS)
	$(CXX) $(CXXFLAGS) $(WARN) $(SP16_FLAGS) $(SP16_SOURCES) $(LDFLAGS) -o $@

$(OUT)/sp16-test: tests/sp16.cpp tests/png.h src/sixteen_screen.h src/presets.h src/params.h Makefile
	$(CXX) -std=c++17 -O2 $(WARN) -Ithird_party/clap/include -Isrc -DREVERIE_SP16 $< -ldl -o $@

$(OUT)/sp16-test-arm: tests/sp16.cpp tests/png.h src/sixteen_screen.h src/presets.h src/params.h Makefile
	$(SP16_CXX) -std=c++17 -O2 $(WARN) -march=armv7-a -Ithird_party/clap/include -Isrc -DREVERIE_SP16 $< -static-libstdc++ -static-libgcc -ldl -o $@
