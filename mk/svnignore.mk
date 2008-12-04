# rules to make svn ignore files 
# Copyright 2008 Roy Marples <roy@marples.name>
# All rights reserved. Released under the 2-clause BSD license.

IGNOREFILES+=	${CLEANFILES}

ignore:
	@if test -n "${IGNOREFILES}"; then \
		echo "Ignoring ${IGNOREFILES}"; \
		files="$$(echo ${IGNOREFILES} | tr ' ' '\n')"; \
		eval svn propset svn:ignore \'"$${files}"\' .; \
	fi
