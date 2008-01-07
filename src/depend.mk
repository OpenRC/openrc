# This only works for make implementations that always include a .depend if
# it exists. Only GNU make does not do this.

.depend: ${SCRIPTS} ${SRCS}
	$(CC) $(CFLAGS) -MM ${SRCS} > .depend

depend: .depend

clean-depend:
	rm -f .depend
