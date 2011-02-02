# Generate .depend
# Copyright (c) 2008 Roy Marples <roy@marples.name>

CLEANFILES+=	.depend
IGNOREFILES+=	.depend

.depend: ${SRCS}
	rm -f .depend
	${CC} ${CPPFLAGS} -MM ${SRCS} > .depend

depend: .depend extra_depend

-include .depend
