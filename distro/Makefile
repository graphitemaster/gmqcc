DROPBOX := dropbox_uploader.sh
UNAME   := $(shell uname -m)
DOWNLOAD:= ../doc/html/download.c
BRANCH  := $(shell git rev-parse --abbrev-ref HEAD)
ifneq ($(shell uname -m), x86_64)
    $(error Cannot build packages without an x86_64 capable CPU)
endif

.NOTPARALLEL: base
.NOTPARALLEL: upload

base:
	@echo "Building Debian packages ..."
	$(MAKE) -C deb/
	$(MAKE) -C deb/ CARCH=i686
	@echo "Building Archlinux packages ..."
	$(MAKE) -C archlinux/this/
	$(MAKE) -C archlinux/this/ CARCH=i686
	@echo "Building Slackware packages ..."
	$(MAKE) -C win32/
	$(MAKE) -C win64
	@echo "Building Windows packages ..."
	$(MAKE) -C slackware/this/
	$(MAKE) -C slackware/this/ CARCH=i686
	@mv deb/*.deb ./
	@mv archlinux/this/*pkg.tar.xz ./
	@mv win32/*.zip ./
	@mv win64/*.zip ./
	@mv slackware/this/*.txz ./

upload:
	@echo "APPKEY:76vh3q42hnvmzm3" > dropbox_config
	@echo "APPSECRET:tmeecht2cmh72xa" >> dropbox_config
	@echo "ACCESS_LEVEL:sandbox" >> dropbox_config
	@echo "OAUTH_ACCESS_TOKEN:w0bxzf0dft8edfq" >> dropbox_config
	@echo "OAUTH_ACCESS_TOKEN_SECRET:9vosx7x8gy4kgjk" >> dropbox_config
	@wget -q "http://raw.github.com/andreafabrizi/Dropbox-Uploader/master/dropbox_uploader.sh"
	@chmod +x dropbox_uploader.sh
	@sed -i -e "s/~\/.dropbox_uploader/.\/dropbox_config/g" $$(basename $(DROPBOX))
	@find . -type f -regex ".*/.*\.\(xz\|deb\|zip\|txz\)" -exec ./$$(basename $(DROPBOX)) upload {} \;
	@rm dropbox_config dropbox_uploader.sh

website:
	$(CC) $(DOWNLOAD) -o html.gen
	@./html.gen ../
	@rm html.gen
	@git stash
	@git checkout gh-pages
	@rm -f ../download.html
	@mv -f download.html ../download.html
	@cd ..; git add download.html; git commit -m 'update download page'; git push origin gh-pages;
	@git checkout $(BRANCH)
	@git stash apply

clean:
	@rm -f *.deb
	@rm -f *.pkg.tar.xz
	@rm -f *.zip
	@rm -f *.gen
	@rm -f *.html
	@rm -f *.txz

all: base upload
