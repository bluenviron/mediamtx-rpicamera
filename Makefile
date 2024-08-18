ifeq ($(shell gcc -dumpmachine),aarch64-linux-gnu)
  OUT_DIR = mtxrpicam_64
else
  OUT_DIR = mtxrpicam_32
endif

all: \
	$(OUT_DIR)/exe \
	$(OUT_DIR)/ipa_conf \
	$(OUT_DIR)/ipa_module \
	$(OUT_DIR)/libcamera.so.9.9 \
	$(OUT_DIR)/libcamera-base.so.9.9

folder:
	mkdir -p $(OUT_DIR)

#################################################
# openssl

OPENSSL_REPO = https://github.com/openssl/openssl
OPENSSL_TAG = openssl-3.3.1

OPENSSL_TARGET = prefix/lib/libcrypto.a

deps/openssl:
	git clone -b $(OPENSSL_TAG) --depth=1 $(OPENSSL_REPO) $@

$(OPENSSL_TARGET): deps/openssl
	cd $< \
	&& ./Configure \
	--prefix=$(PWD)/prefix \
	no-shared \
	no-threads \
	no-quic \
	no-uplink \
	no-tests \
	&& make -j$$(nproc) \
	&& make install_sw

#################################################
# libcamera

LIBCAMERA_REPO = https://github.com/raspberrypi/libcamera
LIBCAMERA_TAG = v0.3.0+rpt20240617

LIBCAMERA_TARGET = prefix/lib/libcamera.so

deps/libcamera:
	git clone -b $(LIBCAMERA_TAG) --depth=1 $(LIBCAMERA_REPO) $@

$(LIBCAMERA_TARGET): deps/libcamera $(OPENSSL_TARGET)
	cd $< \
	&& echo "0.3.0+mediamtx" > .tarball-version \
	&& patch -p1 < ../../libcamera.patch \
	&& PKG_CONFIG_PATH=$(PWD)/prefix/lib/pkgconfig \
	meson setup build \
	--prefix=$(PWD)/prefix \
	--buildtype=release \
	-Dwrap_mode=forcefallback \
	-Dlc-compliance=disabled \
	-Dipas=rpi/vc4 \
	-Dpipelines=rpi/vc4 \
	-Dcam=disabled \
	-Ddocumentation=disabled \
	-Dgstreamer=disabled \
	-Dpycamera=disabled \
	-Dqcam=disabled \
	-Dtracing=disabled \
	-Dudev=disabled \
	&& ninja -C build install

$(OUT_DIR)/ipa_conf: folder $(LIBCAMERA_TARGET)
	cp -r prefix/share/libcamera/ipa $@

$(OUT_DIR)/ipa_module: folder $(LIBCAMERA_TARGET)
	cp -r prefix/lib/libcamera $@

$(OUT_DIR)/libcamera.so.9.9: folder $(LIBCAMERA_TARGET)
	cp prefix/lib/libcamera.so.9.9 $@

$(OUT_DIR)/libcamera-base.so.9.9: folder $(LIBCAMERA_TARGET)
	cp prefix/lib/libcamera-base.so.9.9 $@

#################################################
# libfreetype

FREETYPE_REPO = https://github.com/freetype/freetype
FREETYPE_BRANCH = VER-2-11-1

FREETYPE_TARGET = prefix/lib/libfreetype.a

deps/freetype:
	git clone -b $(FREETYPE_BRANCH) --depth=1 $(FREETYPE_REPO) $@

$(FREETYPE_TARGET): deps/freetype
	cd $< \
	&& cmake -B build \
	-DCMAKE_INSTALL_PREFIX:PATH=$(PWD)/prefix \
	-D CMAKE_BUILD_TYPE=Release \
	-D BUILD_SHARED_LIBS=false \
	-D FT_DISABLE_ZLIB=TRUE \
	-D FT_DISABLE_BZIP2=TRUE \
	-D FT_DISABLE_PNG=TRUE \
	-D FT_DISABLE_HARFBUZZ=TRUE \
	-D FT_DISABLE_BROTLI=TRUE \
	&& make -C build -j$$(nproc) \
	&& make -C build install

#################################################
# text font

TEXT_FONT_URL = https://github.com/IBM/plex/raw/v6.4.2/IBM-Plex-Mono/fonts/complete/ttf/IBMPlexMono-Medium.ttf
TEXT_FONT_SHA256 = 0bede3debdea8488bbb927f8f0650d915073209734a67fe8cd5a3320b572511c

TEXT_FONT_TARGET = deps/text_font.h

deps/text_font.ttf:
	mkdir -p deps
	wget -O $@.tmp $(TEXT_FONT_URL)
	H=$$(sha256sum $@.tmp | awk '{ print $$1 }'); [ "$$H" = "$(TEXT_FONT_SHA256)" ] || { echo "hash mismatch; got $$H, expected $(TEXT_FONT_SHA256)"; exit 1; }
	mv $@.tmp $@

$(TEXT_FONT_TARGET): deps/text_font.ttf
	xxd --include $< > $@

#################################################
# exe

CFLAGS = \
	-Ofast \
	-Werror \
	-Wall \
	-Wextra \
	-Wno-unused-parameter \
	-Wno-unused-result \
	$$(PKG_CONFIG_PATH=$(PWD)/prefix/lib/pkgconfig pkg-config --cflags freetype2)

CXXFLAGS = \
	-Ofast \
	-Werror \
	-Wall \
	-Wextra \
	-Wno-unused-parameter \
	-Wno-unused-result \
	-std=c++17 \
	$$(PKG_CONFIG_PATH=$(PWD)/prefix/lib/pkgconfig pkg-config --cflags libcamera)

LDFLAGS = \
	-s \
	-pthread \
	$$(PKG_CONFIG_PATH=$(PWD)/prefix/lib/pkgconfig pkg-config --libs freetype2) \
	$$(PKG_CONFIG_PATH=$(PWD)/prefix/lib/pkgconfig pkg-config --libs libcamera)

OBJS = \
	base64.o \
	camera.o \
	encoder.o \
	main.o \
	parameters.o \
	pipe.o \
	sensor_mode.o \
	text.o \
	window.o

DEPENDENCIES = \
	$(LIBCAMERA_TARGET) \
	$(FREETYPE_TARGET) \
	$(TEXT_FONT_TARGET)

%.o: %.c $(DEPENDENCIES)
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.cpp $(DEPENDENCIES)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OUT_DIR)/exe: $(OBJS)
	mkdir -p $(OUT_DIR)
	$(CXX) $^ $(LDFLAGS) -o $@
