define DOCKERFILE_FORMAT
FROM $(ALPINE_IMAGE)
RUN apk add --no-cache clang21-extra-tools
endef
export DOCKERFILE_FORMAT

define DOCKERFILE_PRETTIER
FROM $(NODE_IMAGE)
RUN yarn global add prettier@3.6.2
endef
export DOCKERFILE_PRETTIER

format-c:
	echo "$$DOCKERFILE_FORMAT" | docker build . -f - -t format
	docker run --rm -v $(shell pwd):/s -w /s format sh -c "/usr/lib/llvm21/bin/clang-format -i *.cpp *.c *.h"

format-other:
	echo "$$DOCKERFILE_PRETTIER" | docker build . -f - -t temp
	docker run --rm -v "$(shell pwd)/:/s" -w /s temp \
	sh -c "prettier --write ."

format: format-c format-other
