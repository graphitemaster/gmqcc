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
	$(MAKE) -C $(BASEDIR) DESTDIR=distro/deb/$(DEBDIR)/ PREFIX=$(PREFIX) install
	@install -d -m755 $(DEBDIR)/DEBIAN
	@cp       control $(DEBDIR)/DEBIAN/

	@tar czf data.tar.gz $(DEBDIR)/[a-z]*
	@tar czf control.tar.gz $(DEBDIR)/DEBIAN/*
	@echo 2.0 > debian-binary
	@ar r $(DEB) debian-binary control.tar.gz data.tar.gz
	@rm -rf $(DEBDIR) debian-binary control.tar.gz data.tar.gz

clean:
	@rm $(DEB)


all: base
