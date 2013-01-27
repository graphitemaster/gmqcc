BASEDIR := ../../../
PREFIX  := /usr
HEADER  := $(BASEDIR)/gmqcc.h
MAJOR   := $(shell sed -n -e '/GMQCC_VERSION_MAJOR/{s/.* .* //;p;q;}' $(HEADER))
MINOR   := $(shell sed -n -e '/GMQCC_VERSION_MINOR/{s/.* .* //;p;q;}' $(HEADER))
PATCH   := $(shell sed -n -e '/GMQCC_VERSION_PATCH/{s/.* .* //;p;q;}' $(HEADER))
PKGREL  := 1
CARCH   := $(shell uname -m)
PKGDIR  := gmqcc-$(MAJOR).$(MINOR).$(PATCH)-$(PKGREL)-$(CARCH)
PKG     := $(PKGDIR).pkg.tar.xz
PKGINFO := $(PKGDIR)/.PKGINFO

base: clean
	$(MAKE) -C $(BASEDIR) DESTDIR=distro/arch/this/$(PKGDIR) PREFIX=$(PREFIX) CFLAGS+=-m64 install
	@echo "pkgname = gmqcc" > $(PKGINFO)
	@echo "pkgver = $(MAJOR).$(MINOR).$(PATCH)-$(PKGREL)" >> $(PKGINFO)
	@echo "pkgdesc = An Improved Quake C Compiler" >> $(PKGINFO)
	@echo "url = https://github.com/graphitemaster/gmqcc.git" >> $(PKGINFO)
	@echo "builddate = `date -u \"+%s\"`" >> $(PKGINFO)
	@echo "packager = Unknown Packager" >> $(PKGINFO)
	@echo "size = `du -sk $(PKGDIR) | awk '{print $$1 * 1024}'`" >> $(PKGINFO)
	@echo "arch = $(CARCH)" >> $(PKGINFO)
	@echo "license = MIT" >> $(PKGINFO)
	@echo "conflict = gmqcc" >> $(PKGINFO)
	@echo "depend = glibc" >> $(PKGINFO)
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
