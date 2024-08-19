ifeq ($(shell gcc -dumpmachine),aarch64-linux-gnu)
  OUT_DIR = mtxrpicam_64
else
  OUT_DIR = mtxrpicam_32
endif

PREFIX = $(PWD)/prefix

all: \
	$(OUT_DIR)/exe \
	$(OUT_DIR)/ipa_conf \
	$(OUT_DIR)/ipa_module \
	$(OUT_DIR)/libcamera.so.9.9 \
	$(OUT_DIR)/libcamera-base.so.9.9

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
	--prefix=$(PREFIX) \
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
	&& PKG_CONFIG_PATH=$(PREFIX)/lib/pkgconfig \
	meson setup build \
	--prefix=$(PREFIX) \
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

$(OUT_DIR)/ipa_conf: $(LIBCAMERA_TARGET)
	mkdir -p $(OUT_DIR)
	cp -r $(PREFIX)/share/libcamera/ipa $@

$(OUT_DIR)/ipa_module: $(LIBCAMERA_TARGET)
	mkdir -p $(OUT_DIR)
	cp -r $(PREFIX)/lib/libcamera $@

$(OUT_DIR)/libcamera.so.9.9: $(LIBCAMERA_TARGET)
	mkdir -p $(OUT_DIR)
	cp $(PREFIX)/lib/libcamera.so.9.9 $@

$(OUT_DIR)/libcamera-base.so.9.9: $(LIBCAMERA_TARGET)
	mkdir -p $(OUT_DIR)
	cp $(PREFIX)/lib/libcamera-base.so.9.9 $@

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
	-DCMAKE_INSTALL_PREFIX:PATH=$(PREFIX) \
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
# x264

X264_REPO = https://code.videolan.org/videolan/x264
X264_COMMIT = 31e19f92f00c7003fa115047ce50978bc98c3a0d

X264_TARGET = prefix/lib/libx264.a

deps/x264:
	git clone $(X264_REPO) $@
	cd $@ && git checkout $(X264_COMMIT)

$(X264_TARGET): deps/x264
	cd $< \
	&& ./configure \
    --prefix=$(PREFIX) \
    --enable-static \
    --enable-strip \
    --disable-cli \
    && make -j$(nproc) \
	&& make install

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
	$$(PKG_CONFIG_PATH=$(PREFIX)/lib/pkgconfig pkg-config --cflags freetype2) \
	$$(PKG_CONFIG_PATH=$(PREFIX)/lib/pkgconfig pkg-config --cflags x264)

CXXFLAGS = \
	-Ofast \
	-Werror \
	-Wall \
	-Wextra \
	-Wno-unused-parameter \
	-Wno-unused-result \
	-std=c++17 \
	$$(PKG_CONFIG_PATH=$(PREFIX)/lib/pkgconfig pkg-config --cflags libcamera)

LDFLAGS = \
	-s \
	-pthread \
	$$(PKG_CONFIG_PATH=$(PREFIX)/lib/pkgconfig pkg-config --libs libcamera) \
	$$(PKG_CONFIG_PATH=$(PREFIX)/lib/pkgconfig pkg-config --libs freetype2) \
	$$(PKG_CONFIG_PATH=$(PREFIX)/lib/pkgconfig pkg-config --libs x264)

OBJS = \
	base64.o \
	camera.o \
	encoder.o \
	encoder_v4l.o \
	encoder_x264.o \
	main.o \
	parameters.o \
	pipe.o \
	sensor_mode.o \
	text.o \
	window.o

DEPENDENCIES = \
	$(LIBCAMERA_TARGET) \
	$(FREETYPE_TARGET) \
	$(X264_TARGET) \
	$(TEXT_FONT_TARGET)

%.o: %.c $(DEPENDENCIES)
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.cpp $(DEPENDENCIES)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OUT_DIR)/exe: $(OBJS)
	mkdir -p $(OUT_DIR)
	$(CXX) $^ $(LDFLAGS) -o $@
