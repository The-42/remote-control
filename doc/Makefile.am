XSLTPROC_STYLESHEET = http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl
XSLTPROC_FLAGS = \
	--nonet \
	--stringparam funcsynopsis.style ansi

edit = $(SED) 's|[@]PACKAGE[@]|$(PACKAGE)|' $< > $@

%.xml: %.xml.in
	$(AM_V_GEN)$(edit)

BUILT_SOURCES = remote-control.conf.xml

%.5: %.xml
	$(AM_V_GEN)$(XSLTPROC) -o $@ $(XSLTPROC_FLAGS) $(XSLTPROC_STYLESHEET) $<

man_MANS = \
	remote-control.conf.5
