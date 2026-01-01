enable_multiarch:
	docker run --privileged --rm tonistiigi/binfmt --install arm64,arm

define DOCKERFILE_BASE_BULLSEYE_32
FROM --platform=linux/arm/v7 $(BULLSEYE_IMAGE)

# even though the base image is arm v7,
# Raspbian libraries and compilers provide arm v6 compatibility.

RUN apt update \
	&& apt install -y wget gpg \
	&& echo "deb http://archive.raspbian.org/raspbian bullseye main rpi firmware" > /etc/apt/sources.list \
	&& echo "deb http://archive.raspberrypi.org/debian bullseye main" > /etc/apt/sources.list.d/raspi.list \
	&& wget -O- https://archive.raspbian.org/raspbian.public.key | gpg --dearmor -o /etc/apt/trusted.gpg.d/raspbian.gpg \
	&& wget -O- https://archive.raspberrypi.org/debian/raspberrypi.gpg.key | gpg --dearmor -o /etc/apt/trusted.gpg.d/raspberrypi.gpg \
	&& rm -rf /var/lib/apt/lists/*

RUN apt update && apt install --reinstall -y \
	libc6 \
	libstdc++6 \
	&& rm -rf /var/lib/apt/lists/*
endef
export DOCKERFILE_BASE_BULLSEYE_32

base_bullseye_32: enable_multiarch
	echo "$$DOCKERFILE_BASE_BULLSEYE_32" | docker build - -t base_bullseye_32

define DOCKERFILE_BASE_BULLSEYE_64
FROM --platform=linux/arm64 $(BULLSEYE_IMAGE)

RUN apt update \
	&& apt install -y wget gpg \
	&& echo "deb http://archive.raspberrypi.org/debian bullseye main" > /etc/apt/sources.list.d/raspi.list \
	&& wget -O- https://archive.raspberrypi.org/debian/raspberrypi.gpg.key | gpg --dearmor -o /etc/apt/trusted.gpg.d/raspberrypi.gpg \
	&& rm -rf /var/lib/apt/lists/*
endef
export DOCKERFILE_BASE_BULLSEYE_64

base_bullseye_64: enable_multiarch
	echo "$$DOCKERFILE_BASE_BULLSEYE_64" | docker build - -t base_bullseye_64

define DOCKERFILE_BASE_BOOKWORM_32
FROM --platform=linux/arm/v7 $(BOOKWORM_IMAGE)

# even though the base image is arm v7,
# Raspbian libraries and compilers provide arm v6 compatibility.

RUN apt update \
	&& apt install -y wget gpg \
	&& echo "deb http://archive.raspbian.org/raspbian bookworm main rpi firmware" > /etc/apt/sources.list \
	&& echo "deb http://archive.raspberrypi.org/debian bookworm main" > /etc/apt/sources.list.d/raspi.list \
	&& wget -O- https://archive.raspbian.org/raspbian.public.key | gpg --dearmor -o /etc/apt/trusted.gpg.d/raspbian.gpg \
	&& wget -O- https://archive.raspberrypi.org/debian/raspberrypi.gpg.key | gpg --dearmor -o /etc/apt/trusted.gpg.d/raspberrypi.gpg \
	&& rm -rf /var/lib/apt/lists/*

RUN apt update && apt install --reinstall -y \
	libc6 \
	libstdc++6 \
	&& rm -rf /var/lib/apt/lists/*
endef
export DOCKERFILE_BASE_BOOKWORM_32

base_bookworm_32: enable_multiarch
	echo "$$DOCKERFILE_BASE_BOOKWORM_32" | docker build - -t base_bookworm_32

define DOCKERFILE_BASE_BOOKWORM_64
FROM --platform=linux/arm64 $(BOOKWORM_IMAGE)

RUN apt update \
	&& apt install -y wget gpg \
	&& echo "deb http://archive.raspberrypi.org/debian bookworm main" > /etc/apt/sources.list.d/raspi.list \
	&& wget -O- https://archive.raspberrypi.org/debian/raspberrypi.gpg.key | gpg --dearmor -o /etc/apt/trusted.gpg.d/raspberrypi.gpg \
	&& rm -rf /var/lib/apt/lists/*
endef
export DOCKERFILE_BASE_BOOKWORM_64

base_bookworm_64: enable_multiarch
	echo "$$DOCKERFILE_BASE_BOOKWORM_64" | docker build - -t base_bookworm_64
