# This only works for make implementations that always include a .depend if
# it exists. Only GNU make does not do this.

# Copyright 2008 Roy Marples <roy@marples.name>
# All rights reserved. Released under the 2-clause BSD license.

CLEANFILES+=	.depend
IGNOREFILES+=	.depend

.depend: ${SRCS}
	${CC} ${CFLAGS} -MM ${SRCS} > .depend

depend: .depend
