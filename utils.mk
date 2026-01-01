ALPINE_IMAGE = alpine:3.22
DEBIAN_BULLSEYE_IMAGE = debian:bullseye-slim
DEBIAN_BOOKWORM_IMAGE = debian:bookworm-slim
DEBIAN_TRIXIE_IMAGE = debian:trixie-slim

help:
	@echo "usage: make [action]"
	@echo ""
	@echo "available actions:"
	@echo ""
	@echo "  format             format code"
	@echo "  build              build binaries for all architectures"
	@echo "  build_32           build binaries for 32-bit architectures"
	@echo "  build_64           build binaries for 64-bit architectures"
	@echo "  test               run tests on all supported OSes and architectures"
	@echo ""

include scripts/*.mk
