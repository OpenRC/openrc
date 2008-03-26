# rules to make .gitignore files
# Copyright 2008 Roy Marples <roy@marples.name>
# All rights reserved. Released under the 2-clause BSD license.

IGNOREFILES+=	${CLEANFILES}

.PHONY:		.gitignore

.gitignore:
	@if test -n "${IGNOREFILES}"; then \
		echo "Ignoring ${IGNOREFILES}"; \
		echo ${IGNOREFILES} | tr ' ' '\n' > .gitignore; \
	fi

gitignore: .gitignore
