enable_multiarch:
	docker run --privileged --rm tonistiigi/binfmt --install arm64,arm

############################################################################

define DOCKERFILE_BASE_BULLSEYE_32
FROM --platform=linux/arm/v7 $(DEBIAN_BULLSEYE_IMAGE)

# even though the base image is arm v7,
# Raspbian libraries and compilers provide arm v6 compatibility.

RUN apt update \
	&& apt install -y wget gpg \
	&& wget -O- https://archive.raspbian.org/raspbian.public.key | gpg --dearmor -o /etc/apt/trusted.gpg.d/raspbian.gpg \
	&& echo "deb http://archive.raspbian.org/raspbian bullseye main rpi firmware" > /etc/apt/sources.list \
	&& wget -O- https://archive.raspberrypi.org/debian/raspberrypi.gpg.key | gpg --dearmor -o /etc/apt/trusted.gpg.d/raspberrypi.gpg \
	&& echo "deb http://archive.raspberrypi.org/debian bullseye main" > /etc/apt/sources.list.d/raspi.list \
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
FROM --platform=linux/arm64 $(DEBIAN_BULLSEYE_IMAGE)

RUN apt update \
	&& apt install -y wget gpg \
	&& wget -O- https://archive.raspberrypi.org/debian/raspberrypi.gpg.key | gpg --dearmor -o /etc/apt/trusted.gpg.d/raspberrypi.gpg \
	&& echo "deb http://archive.raspberrypi.org/debian bullseye main" > /etc/apt/sources.list.d/raspi.list \
	&& rm -rf /var/lib/apt/lists/*
endef
export DOCKERFILE_BASE_BULLSEYE_64

base_bullseye_64: enable_multiarch
	echo "$$DOCKERFILE_BASE_BULLSEYE_64" | docker build - -t base_bullseye_64

############################################################################

define DOCKERFILE_BASE_BOOKWORM_32
FROM --platform=linux/arm/v7 $(DEBIAN_BOOKWORM_IMAGE)

# even though the base image is arm v7,
# Raspbian libraries and compilers provide arm v6 compatibility.

RUN apt update \
	&& apt install -y wget gpg \
	&& wget -O- https://archive.raspbian.org/raspbian.public.key | gpg --dearmor -o /etc/apt/trusted.gpg.d/raspbian.gpg \
	&& echo "deb http://archive.raspbian.org/raspbian bookworm main rpi firmware" > /etc/apt/sources.list \
	&& wget -O- https://archive.raspberrypi.org/debian/raspberrypi.gpg.key | gpg --dearmor -o /etc/apt/trusted.gpg.d/raspberrypi.gpg \
	&& echo "deb http://archive.raspberrypi.org/debian bookworm main" > /etc/apt/sources.list.d/raspi.list \
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
FROM --platform=linux/arm64 $(DEBIAN_BOOKWORM_IMAGE)

RUN apt update \
	&& apt install -y wget gpg \
	&& wget -O- https://archive.raspberrypi.org/debian/raspberrypi.gpg.key | gpg --dearmor -o /etc/apt/trusted.gpg.d/raspberrypi.gpg \
	&& echo "deb http://archive.raspberrypi.org/debian bookworm main" > /etc/apt/sources.list.d/raspi.list \
	&& rm -rf /var/lib/apt/lists/*
endef
export DOCKERFILE_BASE_BOOKWORM_64

base_bookworm_64: enable_multiarch
	echo "$$DOCKERFILE_BASE_BOOKWORM_64" | docker build - -t base_bookworm_64

############################################################################

define DOCKERFILE_BASE_TRIXIE_32
FROM --platform=linux/arm/v7 $(DEBIAN_TRIXIE_IMAGE)

# even though the base image is arm v7,
# Raspbian libraries and compilers provide arm v6 compatibility.

RUN apt update \
	&& apt install -y wget gpg \
	&& wget -O /raspbian-archive-keyring.deb \
	https://archive.raspbian.org/raspbian/pool/main/r/raspbian-archive-keyring/raspbian-archive-keyring_20120528.4_all.deb \
	&& dpkg -i /raspbian-archive-keyring.deb \
	&& rm /raspbian-archive-keyring.deb \
	&& wget -O /etc/apt/sources.list.d/raspbian.sources \
	https://github.com/RPi-Distro/pi-gen/raw/358f9785089fa9fce397b7c36de2d90a9ae9a50e/stage0/00-configure-apt/files/raspbian.sources \
	&& sed -i "s/RELEASE/trixie/g" /etc/apt/sources.list.d/raspbian.sources \
	&& wget -O /usr/share/keyrings/raspberrypi-archive-keyring.pgp \
	https://github.com/RPi-Distro/pi-gen/raw/358f9785089fa9fce397b7c36de2d90a9ae9a50e/stage0/00-configure-apt/files/raspberrypi-archive-keyring.pgp \
	&& wget -O /etc/apt/sources.list.d/raspi.sources \
	https://github.com/RPi-Distro/pi-gen/raw/358f9785089fa9fce397b7c36de2d90a9ae9a50e/stage0/00-configure-apt/files/raspi.sources \
	&& sed -i "s/RELEASE/trixie/g" /etc/apt/sources.list.d/raspi.sources \
	&& rm -rf /var/lib/apt/lists/*

RUN apt update && apt install --reinstall -y \
	libc6 \
	libstdc++6 \
	&& rm -rf /var/lib/apt/lists/*
endef
export DOCKERFILE_BASE_TRIXIE_32

base_trixie_32: enable_multiarch
	echo "$$DOCKERFILE_BASE_TRIXIE_32" | docker build - -t base_trixie_32

define DOCKERFILE_BASE_TRIXIE_64
FROM --platform=linux/arm64 $(DEBIAN_TRIXIE_IMAGE)

RUN apt update \
	&& apt install -y wget gpg \
	&& wget -O /usr/share/keyrings/raspberrypi-archive-keyring.pgp \
	https://github.com/RPi-Distro/pi-gen/raw/358f9785089fa9fce397b7c36de2d90a9ae9a50e/stage0/00-configure-apt/files/raspberrypi-archive-keyring.pgp \
	&& wget -O /etc/apt/sources.list.d/raspi.sources \
	https://github.com/RPi-Distro/pi-gen/raw/358f9785089fa9fce397b7c36de2d90a9ae9a50e/stage0/00-configure-apt/files/raspi.sources \
	&& sed -i "s/RELEASE/trixie/g" /etc/apt/sources.list.d/raspi.sources \
	&& rm -rf /var/lib/apt/lists/*
endef
export DOCKERFILE_BASE_TRIXIE_64

base_trixie_64: enable_multiarch
	echo "$$DOCKERFILE_BASE_TRIXIE_64" | docker build - -t base_trixie_64
