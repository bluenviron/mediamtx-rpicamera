test: test_bullseye_32 test_bullseye_64 test_bookworm_32 test_bookworm_64

test_bullseye_32: base_bullseye_32
	docker run --rm --platform=linux/arm/v7 -v $(shell pwd):/s:ro -w /s/build/mtxrpicam_32 base_bullseye_32 bash -c "LD_LIBRARY_PATH=. TEST=1 ./mtxrpicam"

test_bullseye_64: base_bullseye_64
	docker run --rm --platform=linux/arm64 -v $(shell pwd):/s:ro -w /s/build/mtxrpicam_64 base_bullseye_64 bash -c "LD_LIBRARY_PATH=. TEST=1 ./mtxrpicam"

test_bookworm_32: base_bookworm_32
	docker run --rm --platform=linux/arm/v7 -v $(shell pwd):/s:ro -w /s/build/mtxrpicam_32 base_bookworm_32 bash -c "LD_LIBRARY_PATH=. TEST=1 ./mtxrpicam"

test_bookworm_64: base_bookworm_64
	docker run --rm --platform=linux/arm64 -v $(shell pwd):/s:ro -w /s/build/mtxrpicam_64 base_bookworm_64 bash -c "LD_LIBRARY_PATH=. TEST=1 ./mtxrpicam"
