define DOCKERFILE_LINT
FROM $(ALPINE_IMAGE)
RUN apk add --no-cache clang21-extra-tools
endef
export DOCKERFILE_LINT

lint-c:
	echo "$$DOCKERFILE_LINT" | docker build . -f - -t lint
	docker run --rm -v $(shell pwd):/s -w /s lint sh -c "/usr/lib/llvm21/bin/clang-format --dry-run --Werror *.cpp *.c *.h"

lint-other:
	echo "$$DOCKERFILE_PRETTIER" | docker build . -f - -t temp
	docker run --rm -v "$(shell pwd)/:/s" -w /s temp \
	sh -c "prettier --check ."

lint: lint-c lint-other
