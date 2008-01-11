_MAN3_SH=	ls -1 *.3
_MAN3!=		${_MAN3_SH}
MAN3=		${_MAN3}$(shell ${_MAN3_SH})
_MAN8_SH=	ls -1 *.8
_MAN8!=		${_MAN8_SH}
MAN8=		${_MAN8}$(shell ${_MAN8_SH})

# Handy macro to create symlinks
# This does rely on correctly formatting our manpages!
MAKE_LINKS=	suffix=$${man\#*.}; \
		prefix=$${man%%.*}; \
		for link in `sed -e 's/ ,//g' \
		-n -e '/^\.Sh NAME$$/,/\.Sh/ s/\.Nm //p' $${man}`; do \
		if test "$${link}" != "$${prefix}" ; then \
		ln -sf $${man} \
		${DESTDIR}/${MANDIR}$${suffix}/$${link}.$${suffix} ; \
		fi; \
		done;

MK=		../mk
include ${MK}/sys.mk

all:

install:
	${INSTALL} -d ${DESTDIR}/${MANDIR}3
	for man in ${MAN3}; do \
		${INSTALL} -m ${MANMODE} "$$man" ${DESTDIR}/${MANDIR}3 || exit $$?; \
		${MAKE_LINKS} \
	done	
	${INSTALL} -d ${DESTDIR}/${MANDIR}8
	for man in ${MAN8}; do \
		${INSTALL} -m ${MANMODE} "$$man" ${DESTDIR}/${MANDIR}8 || exit $$?; \
		${MAKE_LINKS} \
	done

clean:
