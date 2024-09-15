# mediamtx-rpicamera

Raspberry Pi Camera component for [MediaMTX](https://github.com/bluenviron/mediamtx).

This is a C-based executable that pulls the Raspberry Camera video feed, encodes it and makes the compressed video available to the server, while listening for incoming commands.

This is embedded into all MediaMTX releases and shouldn't normally be downloaded unless you want to recompile it.

## Compile

1. You must be on a Raspberry Pi, running Raspberry Pi OS Bullseye

2. Install build dependencies:

   ```sh
   sudo apt install -y \
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
   ```

3. Build:

   ```sh
   meson setup build && DESTDIR=./prefix ninja -C build install
   ```

   This will produce the `build/mtxrpicam_32` or `build/mtxrpicam_64` folder (depending on the architecture).

## Compile against an external libcamera

1. You must be on a Raspberry Pi, running Raspberry Pi OS Bullseye

2. Install build dependencies:

   ```sh
   sudo apt install -y \
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
   ```

3. Make sure that the development package of your libcamera is installed, otherwise install the default one:

   ```sh
   sudo apt install -y \
   libcamera-dev
   ```

3. Build with `--wrap-mode=default` (that disables embedded libraries):

   ```sh
   meson setup --wrap-mode=default build && DESTDIR=./prefix ninja -C build install
   ```

   This will produce the `build/mtxrpicam_32` or `build/mtxrpicam_64` folder (depending on the architecture).

## Cross-compile

1. You can be on any machine you like

2. Install Docker and `make`

3. Build for every supported architecture:

   ```
   make -f utils.mk build
   ```

   This will produce the `build/mtxrpicam_32` and `build/mtxrpicam_64` folders.

## Install

1. Download MediaMTX source code and open a terminal in it

2. Run `go generate ./...`

3. Copy `build/mtxrpicam_32` and/or `build/mtxrpicam_64` inside `internal/staticsources/rpicamera/`, overriding existing folders

4. Compile MediaMTX

   ```sh
   go build .
   ```

## License

All the code in this repository is released under the [MIT License](LICENSE). Compiled binaries make use of some third-party dependencies:

* libcamera, released under the [MIT license](https://git.libcamera.org/libcamera/libcamera.git/tree/LICENSES)
* freetype, released under the [FreeType license](https://github.com/freetype/freetype/blob/master/LICENSE.TXT)
* x264, released under the [GPL v2](https://gitlab.freedesktop.org/gstreamer/meson-ports/x264/-/blob/meson/COPYING)
