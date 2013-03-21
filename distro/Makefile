DROPBOX := dropbox_uploader.sh
UNAME   := $(shell uname -m)
ifneq ($(shell uname -m), x86_64)
    $(error Cannot build packages without an x86_64 capable CPU)
endif

.NOTPARALLEL: base
.NOTPARALLEL: upload

base:
	$(MAKE) -C deb/
	$(MAKE) -C deb/ CARCH=i686
	$(MAKE) -C archlinux/this/
	$(MAKE) -C archlinux/this/ CARCH=i686
	@mv deb/*.deb ./
	@mv archlinux/this/*pkg.tar.xz ./

upload:
	@echo "APPKEY:76vh3q42hnvmzm3" > dropbox_config
	@echo "APPSECRET:tmeecht2cmh72xa" >> dropbox_config
	@echo "ACCESS_LEVEL:sandbox" >> dropbox_config
	@echo "OAUTH_ACCESS_TOKEN:w0bxzf0dft8edfq" >> dropbox_config
	@echo "OAUTH_ACCESS_TOKEN_SECRET:9vosx7x8gy4kgjk" >> dropbox_config
	@wget -q "http://raw.github.com/andreafabrizi/Dropbox-Uploader/master/dropbox_uploader.sh"
	@chmod +x dropbox_uploader.sh
	@sed -i -e "s/~\/.dropbox_uploader/.\/dropbox_config/g" $$(basename $(DROPBOX))
	@find . -type f -regex ".*/.*\.\(xz\|deb\)" -exec ./$$(basename $(DROPBOX)) upload {} \;
	@rm dropbox_config dropbox_uploader.sh

clean:
	@rm -f *.deb
	@rm -f *.pkg.tar.xz

all: base upload
