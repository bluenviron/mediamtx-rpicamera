all: rpicc

#################################################
# text font

TEXT_FONT_URL = https://github.com/IBM/plex/raw/v6.4.2/IBM-Plex-Mono/fonts/complete/ttf/IBMPlexMono-Medium.ttf
TEXT_FONT_SHA256 = 0bede3debdea8488bbb927f8f0650d915073209734a67fe8cd5a3320b572511c

TEXT_FONT_TARGET = text_font.h

text_font.ttf:
	wget -O $@.tmp $(TEXT_FONT_URL)
	H=$$(sha256sum $@.tmp | awk '{ print $$1 }'); [ "$$H" = "$(TEXT_FONT_SHA256)" ] || { echo "hash mismatch; got $$H, expected $(TEXT_FONT_SHA256)"; exit 1; }
	mv $@.tmp $@

$(TEXT_FONT_TARGET): text_font.ttf
	xxd --include $< > $@

#################################################
# rpicc

CFLAGS = \
	-Ofast \
	-Werror \
	-Wall \
	-Wextra \
	-Wno-unused-parameter \
	-Wno-unused-result \
	$$(pkg-config --cflags freetype2)

CXXFLAGS = \
	-Ofast \
	-Werror \
	-Wall \
	-Wextra \
	-Wno-unused-parameter \
	-Wno-unused-result \
	-std=c++17 \
	$$(pkg-config --cflags libcamera)

LDFLAGS = \
	-s \
	-pthread \
	$$(pkg-config --libs freetype2) \
	$$(pkg-config --libs libcamera)

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
	$(TEXT_FONT_TARGET)

%.o: %.c $(DEPENDENCIES)
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.cpp $(DEPENDENCIES)
	$(CXX) $(CXXFLAGS) -c $< -o $@

rpicc: $(OBJS)
	$(CXX) $^ $(LDFLAGS) -o $@
