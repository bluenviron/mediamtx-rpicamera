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
   pkg-config \
   make \
   libcamera-dev \
   libfreetype-dev \
   xxd \
   wget
   ```

3. Build:

   ```sh
   make -j$(nproc)
   ```

   This will produce `mtxrpicam_32` or `mtxrpicam_64` (depending on the architecture).

## Cross-compile

1. You can be on any machine you like

2. Install Docker and `make`

3. Build for every supported architecture:

   ```
   make -f utils.mk build
   ```

   This will produce `mtxrpicam_32` and `mtxrpicam_64`.

## Installation

1. Download MediaMTX source code and open a terminal in it

2. Run `go generate ./...`

3. Copy `mtxrpicam_32` and/or `mtxrpicam_64` inside `internal/staticsources/rpicamera/`

4. Compile MediaMTX

   ```sh
   go build .
   ```
