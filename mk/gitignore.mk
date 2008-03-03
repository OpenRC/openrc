# rules to make .gitignore files
# Copyright 2008 Roy Marples <roy@marples.name>

IGNOREFILES+=	${CLEANFILES}

.PHONY:		.gitignore

.gitignore:
	@if test -n "${IGNOREFILES}"; then \
		echo "Ignoring ${IGNOREFILES}"; \
		echo ${IGNOREFILES} | tr ' ' '\n' > .gitignore; \
	fi

gitignore: .gitignore
