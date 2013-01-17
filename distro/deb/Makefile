BASEDIR := ../..
PREFIX  := usr
BINDIR  := $(PREFIX)/bin
DATADIR := $(PREFIX)/share
MANDIR  := $(DATADIR)/man
HEADER  := $(BASEDIR)/gmqcc.h
MAJOR   := `sed -n -e '/GMQCC_VERSION_MAJOR/{s/.* .* //;p;q;}' $(HEADER)`
MINOR   := `sed -n -e '/GMQCC_VERSION_MINOR/{s/.* .* //;p;q;}' $(HEADER)`
PATCH   := `sed -n -e '/GMQCC_VERSION_PATCH/{s/.* .* //;p;q;}' $(HEADER)`
DEBDIR  := gmqcc-$(MAJOR).$(MINOR).$(PATCH)
DEB     := $(DEBDIR).deb

base:
	@mkdir     $(DEBDIR)
	@mkdir     $(DEBDIR)/$(PREFIX)
	@mkdir -p  $(DEBDIR)/$(BINDIR)
	@mkdir -p  $(DEBDIR)/$(DATADIR)
	@mkdir -p  $(DEBDIR)/$(MANDIR)
	@mkdir -p  $(DEBDIR)/$(MANDIR)/man1
	@mkdir -p  $(DEBDIR)/DEBIAN

	@cp        $(BASEDIR)/doc/gmqcc.1 $(DEBDIR)/$(MANDIR)/man1/
	@cp        $(BASEDIR)/doc/qcvm.1  $(DEBDIR)/$(MANDIR)/man1/
	@cp        $(BASEDIR)/gmqcc       $(DEBDIR)/$(BINDIR)/
	@cp        $(BASEDIR)/qcvm        $(DEBDIR)/$(BINDIR)/
	@cp        control                $(DEBDIR)/DEBIAN/

	@chmod -R 755 $(DEBDIR)

	@tar czf data.tar.gz $(DEBDIR)/[a-z]*
	@tar czf control.tar.gz $(DEBDIR)/DEBIAN/*
	@echo 2.0 > debian-binary
	@ar r $(DEB) debian-binary control.tar.gz data.tar.gz
	@rm -rf $(DEBDIR) debian-binary control.tar.gz data.tar.gz

clean:
	@rm $(DEB)


all: base
