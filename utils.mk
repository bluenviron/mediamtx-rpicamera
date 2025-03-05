BULLSEYE_32_IMAGE = balenalib/raspberry-pi:bullseye-run-20240508
BULLSEYE_64_IMAGE = balenalib/raspberrypi3-64:bullseye-run-20240429
BOOKWORM_32_IMAGE = balenalib/raspberry-pi:bookworm-run-20240527
BOOKWORM_64_IMAGE = balenalib/raspberrypi3-64:bookworm-run-20240429

help:
	@echo "usage: make [action]"
	@echo ""
	@echo "available actions:"
	@echo ""
	@echo "  build              build binaries for all architectures"
	@echo "  build_32           build binaries for 32-bit architectures"
	@echo "  build_64           build binaries for 64-bit architectures"
	@echo "  test_bullseye_32   test binaries on Raspberry Pi OS Bullseye 32-bit"
	@echo "  test_bullseye_64   test binaries on Raspberry Pi OS Bullseye 64-bit"
	@echo "  test_bookworm_32   test binaries on Raspberry Pi OS Bookworm 32-bit"
	@echo "  test_bookworm_64   test binaries on Raspberry Pi OS Bookworm 64-bit"
	@echo ""

build: build_32 build_64

enable_multiarch:
	docker run --privileged --rm tonistiigi/binfmt --install arm64,arm

define DOCKERFILE_BUILD_32
FROM --platform=linux/arm/v6 $(BULLSEYE_32_IMAGE) AS build
RUN apt update && apt install -y --no-install-recommends \
	g++ \
	xxd \
	wget \
	git \
	cmake \
	meson \
	pkg-config \
	python3-jinja2 \
	python3-yaml \
	python3-ply
WORKDIR /s
COPY . .
RUN meson setup build && DESTDIR=./prefix ninja -C build install
endef
export DOCKERFILE_BUILD_32

build_32: enable_multiarch
	echo "$$DOCKERFILE_BUILD_32" | docker build . -f - -t build32
	mkdir -p build
	docker run --rm -v $(shell pwd):/o build32 sh -c "rm -rf /o/build/mtxrpicam_32 && mv /s/build/mtxrpicam_32 /o/build/"

define DOCKERFILE_BUILD_64
FROM --platform=linux/arm64/v8 $(BULLSEYE_64_IMAGE) AS build
RUN apt update && apt install -y --no-install-recommends \
	g++ \
	make \
	xxd \
	wget \
	git \
	cmake \
	meson \
	patch \
	pkg-config \
	python3-jinja2 \
	python3-yaml \
	python3-ply
WORKDIR /s
COPY . .
RUN meson setup build && DESTDIR=./prefix ninja -C build install
endef
export DOCKERFILE_BUILD_64

build_64: enable_multiarch
	echo "$$DOCKERFILE_BUILD_64" | docker build . -f - -t build64
	mkdir -p build
	docker run --rm -v $(shell pwd):/o build64 sh -c "rm -rf /o/build/mtxrpicam_64 && mv /s/build/mtxrpicam_64 /o/build/"

define DOCKERFILE_TEST_BULLSEYE_32
FROM --platform=linux/arm/v6 $(BULLSEYE_32_IMAGE) AS build
endef
export DOCKERFILE_TEST_BULLSEYE_32

test_bullseye_32: enable_multiarch
	echo "$$DOCKERFILE_TEST_BULLSEYE_32" | docker build . -f - -t test_bullseye_32
	docker run --rm --platform=linux/arm/v6 -v $(shell pwd):/s -w /s/build/mtxrpicam_32 test_bullseye_32 bash -c "LD_LIBRARY_PATH=. TEST=1 ./mtxrpicam"

define DOCKERFILE_TEST_BULLSEYE_64
FROM --platform=linux/arm64/v8 $(BULLSEYE_64_IMAGE) AS build
endef
export DOCKERFILE_TEST_BULLSEYE_64

test_bullseye_64: enable_multiarch
	echo "$$DOCKERFILE_TEST_BULLSEYE_64" | docker build . -f - -t test_bullseye_64
	docker run --rm --platform=linux/arm64/v8 -v $(shell pwd):/s -w /s/build/mtxrpicam_64 test_bullseye_64 bash -c "LD_LIBRARY_PATH=. TEST=1 ./mtxrpicam"

define DOCKERFILE_TEST_BOOKWORM_32
FROM --platform=linux/arm/v6 $(BOOKWORM_32_IMAGE) AS build
endef
export DOCKERFILE_TEST_BOOKWORM_32

test_bookworm_32: enable_multiarch
	echo "$$DOCKERFILE_TEST_BOOKWORM_32" | docker build . -f - -t test_bookworm_32
	docker run --rm --platform=linux/arm/v6 -v $(shell pwd):/s -w /s/build/mtxrpicam_32 test_bookworm_32 bash -c "LD_LIBRARY_PATH=. TEST=1 ./mtxrpicam"

define DOCKERFILE_TEST_BOOKWORM_64
FROM --platform=linux/arm64/v8 $(BOOKWORM_64_IMAGE) AS build
endef
export DOCKERFILE_TEST_BOOKWORM_64

test_bookworm_64: enable_multiarch
	echo "$$DOCKERFILE_TEST_BOOKWORM_64" | docker build . -f - -t test_bookworm_64
	docker run --rm --platform=linux/arm64/v8 -v $(shell pwd):/s -w /s/build/mtxrpicam_64 test_bookworm_64 bash -c "LD_LIBRARY_PATH=. TEST=1 ./mtxrpicam"
