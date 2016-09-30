ACLOCAL_AMFLAGS = -I m4

SUBDIRS = \
	src \
	bin \
	data \
	doc \
	tests

DISTCHECK_CONFIGURE_FLAGS = \
	--with-gtk=3.0

MOSTLYCLEANFILES = \
	$(DX_CLEANFILES)

if HAVE_DOXYGEN
@DX_RULES@

all: doxygen-run
endif
