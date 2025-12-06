# mediamtx-rpicamera

Raspberry Pi Camera component for [MediaMTX](https://github.com/bluenviron/mediamtx).

This is a C-based executable that pulls the Raspberry Camera video feed, encodes it and makes the compressed video available to the server, while listening for incoming commands.

This is embedded into all MediaMTX releases and shouldn't be usually downloaded unless you want to recompile it.

## Compile

1. You must be on a Raspberry Pi, running Raspberry Pi OS Bullseye.

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

1. You must be on a Raspberry Pi, running Raspberry Pi OS Bullseye. The external libcamera must have a version &le; 0.50.

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

Check MediaMTX documentation for instructions on how to use the compiled component.

## License

All the code in this repository is released under the [MIT License](LICENSE). Compiled binaries include or make use of some third-party dependencies:

* libcamera, released under the [MIT license](https://git.libcamera.org/libcamera/libcamera.git/tree/LICENSES)
* freetype, released under the [FreeType license](https://github.com/freetype/freetype/blob/master/LICENSE.TXT)
* libjpeg-turbo, released under a [Modified BSD 3-clause License](https://github.com/libjpeg-turbo/libjpeg-turbo?tab=License-1-ov-file#the-modified-3-clause-bsd-license)
* openh264, released under the [BSD 2-clause License](https://github.com/cisco/openh264/blob/master/LICENSE)
