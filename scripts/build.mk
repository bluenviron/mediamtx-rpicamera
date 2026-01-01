build: build_32 build_64

define DOCKERFILE_BUILD_32
FROM --platform=linux/arm/v7 base_bullseye_32
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

build_32: base_bullseye_32
	echo "$$DOCKERFILE_BUILD_32" | docker build . -f - -t build_32
	mkdir -p build
	docker run --rm -v $(shell pwd):/o build_32 sh -c "rm -rf /o/build/mtxrpicam_32 && mv /s/build/mtxrpicam_32 /o/build/"

define DOCKERFILE_BUILD_64
FROM --platform=linux/arm64 base_bullseye_64
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

build_64: base_bullseye_64
	echo "$$DOCKERFILE_BUILD_64" | docker build . -f - -t build_64
	mkdir -p build
	docker run --rm -v $(shell pwd):/o build_64 sh -c "rm -rf /o/build/mtxrpicam_64 && mv /s/build/mtxrpicam_64 /o/build/"
