ALPINE_IMAGE = alpine:3.20
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
	@echo ""

build: build32 build64

define DOCKERFILE_BUILD32
FROM $(RPI32_IMAGE) AS build
RUN ["cross-build-start"]
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
FROM $(ALPINE_IMAGE)
COPY --from=build /s/rpicc /s/rpicc
endef
export DOCKERFILE_BUILD32

define DOCKERFILE_BUILD64
FROM $(RPI64_IMAGE) AS build
RUN ["cross-build-start"]
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
FROM $(ALPINE_IMAGE)
COPY --from=build /s/rpicc /s/rpicc
endef
export DOCKERFILE_BUILD64

build32:
	echo "$$DOCKERFILE_BUILD32" | docker build . -f - -t build32
	docker run --rm -v $(PWD):/o build32 sh -c "mv /s/rpicc /o/rpicc_32"

build64:
	echo "$$DOCKERFILE_BUILD64" | docker build . -f - -t build64
	docker run --rm -v $(PWD):/o build64 sh -c "mv /s/rpicc /o/rpicc_64"
