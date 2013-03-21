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
