RPI32_IMAGE = balenalib/raspberry-pi:bullseye-run-20240508
RPI64_IMAGE = balenalib/raspberrypi3-64:bullseye-run-20240429

help:
	@echo "usage: make [action]"
	@echo ""
	@echo "available actions:"
	@echo ""
	@echo "  build            build binaries for all platforms"
	@echo "  build32          build binaries for the 64-bit platform"
	@echo "  build64          build binaries for the 32-bit platform"
	@echo "  test32           test binaries for the 32-bit platform"
	@echo "  test64           test binaries for the 64-bit platform"
	@echo ""

build: build32 build64

enable_multiarch:
	docker run --rm --privileged multiarch/qemu-user-static:register --reset --credential yes

define DOCKERFILE_BUILD32
FROM multiarch/qemu-user-static:x86_64-arm AS qemu
FROM $(RPI32_IMAGE) AS build
COPY --from=qemu /usr/bin/qemu-arm-static /usr/bin/qemu-arm-static
RUN apt update && apt install -y --no-install-recommends \
	g++ \
	pkg-config \
	make \
	libcamera-dev \
	libfreetype-dev \
	xxd \
	wget
WORKDIR /s
COPY . .
RUN make -j$$(nproc)
endef
export DOCKERFILE_BUILD32

build32: enable_multiarch
	echo "$$DOCKERFILE_BUILD32" | docker build . -f - -t build32
	docker run --rm -v $(PWD):/o build32 sh -c "mv /s/mtxrpicam_32 /o/"

define DOCKERFILE_BUILD64
FROM multiarch/qemu-user-static:x86_64-aarch64 AS qemu
FROM $(RPI64_IMAGE) AS build
COPY --from=qemu /usr/bin/qemu-aarch64-static /usr/bin/qemu-aarch64-static
RUN apt update && apt install -y --no-install-recommends \
	g++ \
	pkg-config \
	make \
	libcamera-dev \
	libfreetype-dev \
	xxd \
	wget
WORKDIR /s
COPY . .
RUN make -j$$(nproc)
endef
export DOCKERFILE_BUILD64

build64: enable_multiarch
	echo "$$DOCKERFILE_BUILD64" | docker build . -f - -t build64
	docker run --rm -v $(PWD):/o build64 sh -c "mv /s/mtxrpicam_64 /o/"

define DOCKERFILE_TEST32
FROM multiarch/qemu-user-static:x86_64-arm AS qemu
FROM $(RPI32_IMAGE) AS build
COPY --from=qemu /usr/bin/qemu-arm-static /usr/bin/qemu-arm-static
RUN apt update && apt install -y --no-install-recommends libcamera0 libfreetype6 && rm -rf /var/lib/apt/lists/*
endef
export DOCKERFILE_TEST32

test32: enable_multiarch
	echo "$$DOCKERFILE_TEST32" | docker build . -f - -t test32
	docker run --rm --platform=linux/arm/v6 -v $(PWD):/s -w /s test32 bash -c "TEST=1 ./mtxrpicam_32"

define DOCKERFILE_TEST64
FROM multiarch/qemu-user-static:x86_64-aarch64 AS qemu
FROM $(RPI64_IMAGE) AS build
COPY --from=qemu /usr/bin/qemu-aarch64-static /usr/bin/qemu-aarch64-static
RUN apt update && apt install -y --no-install-recommends libcamera0 libfreetype6 && rm -rf /var/lib/apt/lists/*
endef
export DOCKERFILE_TEST64

test64: enable_multiarch
	echo "$$DOCKERFILE_TEST64" | docker build . -f - -t test64
	docker run --rm --platform=linux/arm64/v8 -v $(PWD):/s -w /s test64 bash -c "TEST=1 ./mtxrpicam_64"
