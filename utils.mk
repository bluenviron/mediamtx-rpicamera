ALPINE_IMAGE = alpine:3.22
BULLSEYE_32_IMAGE = balenalib/raspberry-pi:bullseye-run-20240508
BULLSEYE_64_IMAGE = balenalib/raspberrypi3-64:bullseye-run-20240429
BOOKWORM_32_IMAGE = balenalib/raspberry-pi:bookworm-run-20240527
BOOKWORM_64_IMAGE = balenalib/raspberrypi3-64:bookworm-run-20240429

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
