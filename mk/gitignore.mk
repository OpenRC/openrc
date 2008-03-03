# rules to make .gitignore files
# Copyright 2008 Roy Marples <roy@marples.name>

IGNOREFILES+=	${CLEANFILES}

.gitignore:
	for obj in ${IGNOREFILES}; do \
		if ! test -r .gitignore; then \
			echo "$${obj}" > .gitignore || exit $$?; \
		elif ! grep -q "^$${obj}$$" .gitignore; then \
			echo "$${obj}" >> .gitignore || exit $$?; \
		fi; \
	done

gitignore: .gitignore
