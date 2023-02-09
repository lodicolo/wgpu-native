RUST_BACKTRACE:=1
EXCLUDES:=

GENERATOR_PLATFORM:=
TARGET_TRIPLE:=

FFI_DIR:=ffi
BUILD_DIR:=build
CREATE_BUILD_DIR:=
OUTPUT_DIR:=

WILDCARD_SOURCE:=$(wildcard src/*.rs)

GIT_TAG=$(shell git describe --abbrev=0 --tags)
GIT_TAG_FULL=$(shell git describe --tags)
OS_NAME=

EXTRA_BUILD_ARGS=
TARGET_DIR=target
ifdef TARGET
    EXTRA_BUILD_ARGS=--target $(TARGET)
    TARGET_DIR=target/$(TARGET)
    TARGET_TRIPLE=-DTARGET_TRIPLE=$(TARGET)
endif

ifndef ARCHIVE_NAME
    ARCHIVE_NAME=wgpu-$(TARGET)
endif

ifeq ($(OS),Windows_NT)
    # '-Force' ignores error if folder already exists
    CREATE_BUILD_DIR=powershell -Command md $(BUILD_DIR) -Force
    GENERATOR_PLATFORM=-DCMAKE_GENERATOR_PLATFORM=x64
    OUTPUT_DIR=build/Debug
else
    CREATE_BUILD_DIR=mkdir -p $(BUILD_DIR)
    OUTPUT_DIR=build
endif

ifeq ($(OS),Windows_NT)
    OS_NAME=windows
else
    UNAME_S:=$(shell uname -s)
    ifeq ($(UNAME_S),Linux)
        OS_NAME=linux
    endif
    ifeq ($(UNAME_S),Darwin)
        OS_NAME=macos
    endif
endif

sanitize-target = "$(word 3,$1) $(if $(word 4,$1),$(word 4,$1),"debug")"

.PHONY: \
	all \
	package \
	clean clean-cargo clean-examples \
	check \
	doc \
	test \
	wgpu-native wgpu-native-debug wgpu-native-release \
	helper helper-debug helper-release

all:
	make example-build capture && \
	make example-build compute && \
	make example-build triangle

package: wgpu-native
	mkdir -p dist
	@echo "$(GIT_TAG_FULL)" > dist/commit-sha
	for RELEASE in debug release; do \
		ARCHIVE=$(ARCHIVE_NAME)-$$RELEASE.zip; \
		LIBDIR=$(TARGET_DIR)/$$RELEASE; \
		rm -f dist/$$ARCHIVE; \
		sed 's/webgpu-headers\///' ffi/wgpu.h > wgpu.h ;\
		if [ $(OS_NAME) = windows ]; then \
			7z a -tzip dist/$$ARCHIVE ./$$LIBDIR/wgpu_native.dll ./$$LIBDIR/wgpu_native.lib ./ffi/webgpu-headers/*.h ./wgpu.h ./dist/commit-sha; \
		else \
			zip -j dist/$$ARCHIVE ./$$LIBDIR/libwgpu_native.so ./$$LIBDIR/libwgpu_native.dylib ./$$LIBDIR/libwgpu_native.a ./ffi/webgpu-headers/*.h ./wgpu.h ./dist/commit-sha; \
		fi; \
		rm wgpu.h ;\
	done

clean: clean-cargo clean-examples
	@echo "Cleaned all files"

clean-examples:
	rm -Rf examples/*/build

clean-cargo:
	cargo clean

check:
	cargo check --all

doc:
	cargo doc --all

test:
	cargo test --all

wgpu-native: wgpu-native-debug wgpu-native-release

wgpu-native-debug: Cargo.lock Cargo.toml Makefile $(WILDCARD_SOURCE)
	cargo build $(EXTRA_BUILD_ARGS)

wgpu-native-release: Cargo.lock Cargo.toml Makefile $(WILDCARD_SOURCE)
	cargo build --release $(EXTRA_BUILD_ARGS)

helper: helper-debug helper-release

helper-debug:
	cargo build -p helper $(EXTRA_BUILD_ARGS)

helper-release:
	cargo build -p helper --release $(EXTRA_BUILD_ARGS)
	cargo build --release $(EXTRA_BUILD_ARGS)

example-clean-%:
	@echo "Cleaning example '$(firstword $(subst -, ,$*))'..."
	rm -Rf examples/$(EXAMPLE_NAME)/build

example-build-%:
    CURRENT_EXAMPLE_TARGET_GOAL = "$(call sanitize-target,$(subst -, ,$@))"
    CURRENT_EXAMPLE_TARGET = "$(firstword $(CURRENT_EXAMPLE_TARGET_GOAL))"
    CURRENT_EXAMPLE_BUILD_TYPE = "$(word 2,$(CURRENT_EXAMPLE_TARGET_GOAL))"
example-build-%: example-clean-% wgpu-native helper
	@echo "Building example '$(CURRENT_EXAMPLE_TARGET)' in '$(CURRENT_EXAMPLE_BUILD_TYPE)' mode..."
	cd examples/$(CURRENT_EXAMPLE_TARGET) && \
	$(CREATE_BUILD_DIR) && \
	cd build && \
	cmake ..  -DCMAKE_TOOLCHAIN_FILE=$(CMAKE_TOOLCHAIN_FILE) -DCMAKE_BUILD_TYPE=$(CURRENT_EXAMPLE_BUILD_TYPE) -DCMAKE_EXPORT_COMPILE_COMMANDS=1 $(GENERATOR_PLATFORM) $(TARGET_TRIPLE) && \
	cmake --build .

example-run-%:
    CURRENT_EXAMPLE_TARGET_GOAL = "$(call sanitize-target,$(subst -, ,$@))"
    CURRENT_EXAMPLE_TARGET = "$(firstword $(CURRENT_EXAMPLE_TARGET_GOAL))"
    CURRENT_EXAMPLE_BUILD_TYPE = "$(word 2,$(CURRENT_EXAMPLE_TARGET_GOAL))"
example-run-%: example-build-%
	@echo "Running example '$(firstword $(subst -, ,$*))'..."
	@echo "MAKECMDGOALS=$(MAKECMDGOALS) -- dummy=$(dummy)"
	cd examples/$(CURRENT_EXAMPLE_TARGET) && \
	"$(OUTPUT_DIR)/$(CURRENT_EXAMPLE_TARGET)"
