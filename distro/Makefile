UNAME := $(shell uname -m)
ifneq ($(shell uname -m), x86_64)
    $(error Cannot build packages without an x86_64 capable CPU)
endif

base:
	$(MAKE) -C deb/
	$(MAKE) -C deb/ CARCH=i686
	$(MAKE) -C archlinux/this/
	$(MAKE) -C archlinux/this/ CARCH=i686
	@mv deb/*.deb ./
	@mv archlinux/this/*pkg.tar.xz ./

clean:
	@rm -f *.deb
	@rm -f *.pkg.tar.xz

all: base
