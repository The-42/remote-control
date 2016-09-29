XSLTPROC_STYLESHEET = http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl
XSLTPROC_FLAGS = \
	--stringparam funcsynopsis.style ansi

edit = $(SED) 's|[@]PACKAGE[@]|$(PACKAGE)|' $< > $@

%.xml: %.xml.in
	$(AM_V_GEN)$(edit)

BUILT_SOURCES = \
	browser.conf.xml \
	remote-control.conf.xml

%.5: %.xml
	$(AM_V_GEN)$(XSLTPROC) -o $@ $(XSLTPROC_FLAGS) $(XSLTPROC_STYLESHEET) $<

man_MANS = \
	browser.conf.5 \
	remote-control.conf.5

EXTRA_DIST = \
	browser.conf.xml.in \
	Doxyfile.in \
	remote-control.conf.xml.in

CLEANFILES = \
	$(BUILT_SOURCES) \
	$(man_MANS)
