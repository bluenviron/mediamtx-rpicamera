define DOCKERFILE_FORMAT
FROM $(ALPINE_IMAGE)
RUN apk add --no-cache clang20-extra-tools
endef
export DOCKERFILE_FORMAT

format:
	echo "$$DOCKERFILE_FORMAT" | docker build . -f - -t format
	docker run --rm -v $(shell pwd):/s -w /s format sh -c "clang-format -i *.cpp *.c *.h"
