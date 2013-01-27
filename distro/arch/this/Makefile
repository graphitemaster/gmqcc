BASEDIR := ../../../
PREFIX  := /usr
HEADER  := $(BASEDIR)/gmqcc.h
MAJOR   := `sed -n -e '/GMQCC_VERSION_MAJOR/{s/.* .* //;p;q;}' $(HEADER)`
MINOR   := `sed -n -e '/GMQCC_VERSION_MINOR/{s/.* .* //;p;q;}' $(HEADER)`
PATCH   := `sed -n -e '/GMQCC_VERSION_PATCH/{s/.* .* //;p;q;}' $(HEADER)`
PKGDIR  := gmqcc-$(MAJOR).$(MINOR).$(PATCH)
PKG     := $(PKGDIR).tar.xz
PKGINFO := $(PKGDIR)/.PKGINFO

base:
	$(MAKE) -C $(BASEDIR) DESTDIR=distro/arch/this/$(PKGDIR) PREFIX=$(PREFIX) install
	@echo "pkgname = gmqcc" > $(PKGINFO)
	@echo "pkgver = $(MAJOR).$(MINOR).$(PATCH)" >> $(PKGINFO)
	@echo "pkgdesc = An Improved Quake C Compiler" >> $(PKGINFO)
	@echo "url = https://github.com/graphitemaster/gmqcc.git" >> $(PKGINFO)
	@echo "builddate = `date -u \"+%s\"`" >> $(PKGINFO)
	@echo "packager = Unknown Packager" >> $(PKGINFO)
	@echo "size = $(shell du -shb $(PKGDIR) | awk '{s+=$$1} END {print s}')" >> $(PKGINFO)
	@echo "arch = x86_64" >> $(PKGINFO)
	@echo "license = MIT" >> $(PKGINFO)
	@echo "conflict = gmqcc" >> $(PKGINFO)
	@echo "depend = glibc" >> $(PKGINFO)
	@echo "provides = gmqcc= $(MAJOR).$(MINOR).$(PATCH)" >> $(PKGINFO)
	@echo "makepkgopt = strip" >> $(PKGINFO)
	@echo "makepkgopt = docs" >> $(PKGINFO)
	@echo "makepkgopt = libtool" >> $(PKGINFO)
	@echo "makepkgopt = emptydirs" >> $(PKGINFO)
	@echo "makepkgopt = zipman" >> $(PKGINFO)
	@echo "makepkgopt = purge" >> $(PKGINFO)
	@echo "makepkgopt = !upx" >> $(PKGINFO)
	@tar -cJvf $(PKG) -C $(PKGDIR)/ .PKGINFO usr/
	@rm -rf $(PKGDIR)


clean:
	@rm -f  $(PKG)


all: base
