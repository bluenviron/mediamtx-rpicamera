test: test_bullseye_32 test_bullseye_64 test_bookworm_32 test_bookworm_64

define DOCKERFILE_TEST_BULLSEYE_32
FROM --platform=linux/arm/v6 $(BULLSEYE_32_IMAGE) AS build
endef
export DOCKERFILE_TEST_BULLSEYE_32

test_bullseye_32: enable_multiarch
	echo "$$DOCKERFILE_TEST_BULLSEYE_32" | docker build - -t test_bullseye_32
	docker run --rm --platform=linux/arm/v6 -v $(shell pwd):/s -w /s/build/mtxrpicam_32 test_bullseye_32 bash -c "LD_LIBRARY_PATH=. TEST=1 ./mtxrpicam"

define DOCKERFILE_TEST_BULLSEYE_64
FROM --platform=linux/arm64/v8 $(BULLSEYE_64_IMAGE) AS build
endef
export DOCKERFILE_TEST_BULLSEYE_64

test_bullseye_64: enable_multiarch
	echo "$$DOCKERFILE_TEST_BULLSEYE_64" | docker build - -t test_bullseye_64
	docker run --rm --platform=linux/arm64/v8 -v $(shell pwd):/s -w /s/build/mtxrpicam_64 test_bullseye_64 bash -c "LD_LIBRARY_PATH=. TEST=1 ./mtxrpicam"

define DOCKERFILE_TEST_BOOKWORM_32
FROM --platform=linux/arm/v6 $(BOOKWORM_32_IMAGE) AS build
endef
export DOCKERFILE_TEST_BOOKWORM_32

test_bookworm_32: enable_multiarch
	echo "$$DOCKERFILE_TEST_BOOKWORM_32" | docker build - -t test_bookworm_32
	docker run --rm --platform=linux/arm/v6 -v $(shell pwd):/s -w /s/build/mtxrpicam_32 test_bookworm_32 bash -c "LD_LIBRARY_PATH=. TEST=1 ./mtxrpicam"

define DOCKERFILE_TEST_BOOKWORM_64
FROM --platform=linux/arm64/v8 $(BOOKWORM_64_IMAGE) AS build
endef
export DOCKERFILE_TEST_BOOKWORM_64

test_bookworm_64: enable_multiarch
	echo "$$DOCKERFILE_TEST_BOOKWORM_64" | docker build - -t test_bookworm_64
	docker run --rm --platform=linux/arm64/v8 -v $(shell pwd):/s -w /s/build/mtxrpicam_64 test_bookworm_64 bash -c "LD_LIBRARY_PATH=. TEST=1 ./mtxrpicam"
