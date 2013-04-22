BASEDIR := ../..
HEADER  := $(BASEDIR)/gmqcc.h
MAJOR   := `sed -n -e '/GMQCC_VERSION_MAJOR/{s/.* .* //;p;q;}' $(HEADER)`
MINOR   := `sed -n -e '/GMQCC_VERSION_MINOR/{s/.* .* //;p;q;}' $(HEADER)`
PATCH   := `sed -n -e '/GMQCC_VERSION_PATCH/{s/.* .* //;p;q;}' $(HEADER)`
BINDIR  := gmqcc-$(MAJOR).$(MINOR).$(PATCH)

base:
	$(MAKE) CC=i486-mingw32-gcc UNAME=MINGW32 -C $(BASEDIR) clean
	$(MAKE) CC=i486-mingw32-gcc UNAME=MINGW32 -C $(BASEDIR) DESTDIR=distro/win32/$(BINDIR) PREFIX=/ install
	@zip -r $(BINDIR)-win32.zip $(BINDIR)
	@rm -rf $(BINDIR)
clean:
	$(MAKE) -C $(BASEDIR) clean
	@rm -f *.zip

all: base
