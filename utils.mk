ALPINE_IMAGE = alpine:3.24
BINFMT_IMAGE = tonistiigi/binfmt:qemu-v10.0.4
DEBIAN_BOOKWORM_IMAGE = debian:bookworm-slim
DEBIAN_TRIXIE_IMAGE = debian:trixie-slim
NODE_IMAGE = node:24-alpine3.24

help:
	@echo "usage: make [action]"
	@echo ""
	@echo "available actions:"
	@echo ""
	@echo "  format             format code"
	@echo "  lint               run linter"
	@echo "  build              build binaries for all architectures"
	@echo "  build_32           build binaries for 32-bit architecture"
	@echo "  build_64           build binaries for 64-bit architecture"
	@echo "  test               run tests on all supported OSes and architectures"
	@echo ""

include scripts/*.mk
