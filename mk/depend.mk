# Generate .depend
# Copyright (c) 2008 Roy Marples <roy@marples.name>

CLEANFILES+=	.depend
IGNOREFILES+=	.depend

ifeq ($(notdir $(firstword ${CC})),qcc)
    # qcc needs a special prefix to get the dependency build to work
    DEP_CFLAGS=-Wp,-MM -E
else
    DEP_CFLAGS=-MM    
endif

.depend: ${SRCS}
	rm -f .depend
	${CC} ${LOCAL_CPPFLAGS} ${CPPFLAGS} ${DEP_CFLAGS} ${SRCS} > .depend

depend: .depend extra_depend

-include .depend
