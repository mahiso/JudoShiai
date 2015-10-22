include common/Makefile.inc

JUDOSHIAIFILE=$(JS_BUILD_DIR)/judoshiai/$(OBJDIR)/judoshiai$(SUFF)
JUDOTIMERFILE=$(JS_BUILD_DIR)/judotimer/$(OBJDIR)/judotimer$(SUFF)
JUDOINFOFILE=$(JS_BUILD_DIR)/judoinfo/$(OBJDIR)/judoinfo$(SUFF)
JUDOWEIGHTFILE=$(JS_BUILD_DIR)/judoweight/$(OBJDIR)/judoweight$(SUFF)
JUDOJUDOGIFILE=$(JS_BUILD_DIR)/judojudogi/$(OBJDIR)/judojudogi$(SUFF)
JUDOPROXYFILE=$(JS_BUILD_DIR)/judoproxy/$(OBJDIR)/judoproxy$(SUFF)

RELDIR=$(RELEASEDIR)/judoshiai
RELFILE=$(RELDIR)/bin/judoshiai$(SUFF)
RUNDIR=$(DEVELDIR)

all:
	make -C common
	make -C judoshiai
	make -C judotimer
	make -C judoinfo
	make -C judoweight
	make -C judojudogi
ifeq ($(JUDOPROXY),YES)
	make -C judoproxy
endif
	make -C doc
	rm -rf $(RELDIR)
	mkdir -p $(RELDIR)/bin
	mkdir -p $(RELDIR)/share/locale/
	mkdir -p $(RELDIR)/share/locale/fi/LC_MESSAGES
	mkdir -p $(RELDIR)/share/locale/sv/LC_MESSAGES
	mkdir -p $(RELDIR)/share/locale/es/LC_MESSAGES
	mkdir -p $(RELDIR)/share/locale/et/LC_MESSAGES
	mkdir -p $(RELDIR)/share/locale/uk/LC_MESSAGES
	mkdir -p $(RELDIR)/share/locale/is/LC_MESSAGES
	mkdir -p $(RELDIR)/share/locale/nb/LC_MESSAGES
	mkdir -p $(RELDIR)/share/locale/pl/LC_MESSAGES
	mkdir -p $(RELDIR)/share/locale/sk/LC_MESSAGES
	mkdir -p $(RELDIR)/share/locale/nl/LC_MESSAGES
	mkdir -p $(RELDIR)/share/locale/cs/LC_MESSAGES
	mkdir -p $(RELDIR)/share/locale/de/LC_MESSAGES
	mkdir -p $(RELDIR)/share/locale/ru/LC_MESSAGES
	mkdir -p $(RELDIR)/share/locale/en_GB/LC_MESSAGES
	mkdir -p $(RELDIR)/lib
	mkdir -p $(RELDIR)/doc
	mkdir -p $(RELDIR)/licenses
	mkdir -p $(RELDIR)/etc
	cp $(JUDOSHIAIFILE) $(RELDIR)/bin/
	cp $(JUDOTIMERFILE) $(RELDIR)/bin/
	cp $(JUDOINFOFILE) $(RELDIR)/bin/
	cp $(JUDOWEIGHTFILE) $(RELDIR)/bin/
	cp $(JUDOJUDOGIFILE) $(RELDIR)/bin/
ifeq ($(JUDOPROXY),YES)
	cp $(JUDOPROXYFILE) $(RELDIR)/bin/
endif
### Windows executable ###
ifeq ($(TGT),WIN32)
ifeq ($(GTKVER),3)
	mkdir -p $(RELDIR)/share/glib-2.0
	cp -r $(RUNDIR)/share/glib-2.0/schemas $(RELDIR)/share/glib-2.0/
endif
	cp $(RUNDIR)/bin/*.dll $(RELDIR)/bin/
	cp $(SOUNDDIR)/bin/*.dll $(RELDIR)/bin/
	cp $(RSVGDIR)/bin/*.dll $(RELDIR)/bin/
	cp $(CURLDIR)/bin/*.dll $(RELDIR)/bin/
ifeq ($(JUDOPROXY),YES)
	cp $(WEBKITDIR)/bin/*.dll $(RELDIR)/bin/
	cp $(SOAPDIR)/bin/*.dll $(RELDIR)/bin/
endif
	cp -r $(RUNDIR)/lib/gtk-$(GTKVER).0 $(RELDIR)/lib/
	cp -r $(RUNDIR)/share/locale/fi $(RELDIR)/share/locale/
	cp -r $(RUNDIR)/share/locale/sv $(RELDIR)/share/locale/
	cp -r $(RUNDIR)/share/locale/es $(RELDIR)/share/locale/
	cp -r $(RUNDIR)/share/locale/et $(RELDIR)/share/locale/
	cp -r $(RUNDIR)/share/locale/uk $(RELDIR)/share/locale/
	cp -r $(RUNDIR)/share/locale/is $(RELDIR)/share/locale/
	cp -r $(RUNDIR)/share/locale/nb $(RELDIR)/share/locale/
	cp -r $(RUNDIR)/share/locale/pl $(RELDIR)/share/locale/
	cp -r $(RUNDIR)/share/locale/sk $(RELDIR)/share/locale/
	cp -r $(RUNDIR)/share/locale/nl $(RELDIR)/share/locale/
	cp -r $(RUNDIR)/share/locale/cs $(RELDIR)/share/locale/
	cp -r $(RUNDIR)/share/locale/de $(RELDIR)/share/locale/
	cp -r $(RUNDIR)/share/locale/ru $(RELDIR)/share/locale/
	cp -r $(RUNDIR)/share/locale/en_GB $(RELDIR)/share/locale/
	cp -r $(RUNDIR)/share/themes $(RELDIR)/share/
	cp -r $(RUNDIR)/etc $(RELDIR)/
### Linux executable ###
else

endif
	cp doc/*.pdf $(RELDIR)/doc/
	cp common/judoshiai-fi_FI.mo $(RELDIR)/share/locale/fi/LC_MESSAGES/judoshiai.mo
	cp common/judoshiai-sv_SE.mo $(RELDIR)/share/locale/sv/LC_MESSAGES/judoshiai.mo
	cp common/judoshiai-es_ES.mo $(RELDIR)/share/locale/es/LC_MESSAGES/judoshiai.mo
	cp common/judoshiai-et_EE.mo $(RELDIR)/share/locale/et/LC_MESSAGES/judoshiai.mo
	cp common/judoshiai-uk_UA.mo $(RELDIR)/share/locale/uk/LC_MESSAGES/judoshiai.mo
	cp common/judoshiai-is_IS.mo $(RELDIR)/share/locale/is/LC_MESSAGES/judoshiai.mo
	cp common/judoshiai-nb_NO.mo $(RELDIR)/share/locale/nb/LC_MESSAGES/judoshiai.mo
	cp common/judoshiai-pl_PL.mo $(RELDIR)/share/locale/pl/LC_MESSAGES/judoshiai.mo
	cp common/judoshiai-sk_SK.mo $(RELDIR)/share/locale/sk/LC_MESSAGES/judoshiai.mo
	cp common/judoshiai-nl_NL.mo $(RELDIR)/share/locale/nl/LC_MESSAGES/judoshiai.mo
	cp common/judoshiai-cs_CZ.mo $(RELDIR)/share/locale/cs/LC_MESSAGES/judoshiai.mo
	cp common/judoshiai-de_DE.mo $(RELDIR)/share/locale/de/LC_MESSAGES/judoshiai.mo
	cp common/judoshiai-ru_RU.mo $(RELDIR)/share/locale/ru/LC_MESSAGES/judoshiai.mo
	cp etc/*.png $(RELDIR)/etc/
	cp etc/*.txt $(RELDIR)/etc/
	cp etc/*.css $(RELDIR)/etc/
	cp etc/*.mp3 $(RELDIR)/etc/
	cp etc/*.shi $(RELDIR)/etc/
	cp etc/*.svg $(RELDIR)/etc/
	cp etc/*.html $(RELDIR)/etc/
	cp etc/*.js $(RELDIR)/etc/
	cp etc/result-texts-*.txt $(RELDIR)/etc/
	cp -r etc/flags-ioc $(RELDIR)/etc/
	cp licenses/* $(RELDIR)/licenses
	cp -r svg $(RELDIR)/
	cp -r custom-examples $(RELDIR)/
	@echo
	@echo "To make a setup executable run" 
	@echo "  make setup"
	@echo
	@echo "To make a Debian package run (Linux only)"
	@echo "  sudo make debian" 

#echo 'gtk-theme-name = "MS-Windows"' >$(RELDIR)/etc/gtk-2.0/gtkrc

setup:
ifeq ($(TGT),WIN32)
	sed "s/AppVerName=.*/AppVerName=Shiai $(SHIAI_VER_NUM)/" etc/judoshiai.iss >judoshiai1.iss
	sed "s/OutputBaseFilename=.*/OutputBaseFilename=judoshiai-setup-$(SHIAI_VER_NUM)/" judoshiai1.iss >judoshiai2.iss
	sed "s,RELDIR,$(RELEASEDIR)," judoshiai2.iss | tr '/' '\\' >judoshiai3.iss
	$(INNOSETUP) judoshiai3.iss
	rm -f judoshiai*.iss
else
	tar -C $(RELEASEDIR) -cf judoshiai.tar judoshiai
	tar -C etc -rf judoshiai.tar install.sh
	gzip judoshiai.tar
	cat etc/header.sh judoshiai.tar.gz >$(RELEASEDIR)/judoshiai-setup-$(SHIAI_VER_NUM).bin
	chmod a+x $(RELEASEDIR)/judoshiai-setup-$(SHIAI_VER_NUM).bin
	rm -f judoshiai.tar.gz
endif

install:
	cp -r $(RELDIR) /usr/lib/
	ln -sf /usr/lib/judoshiai/bin/judoshiai /usr/bin/judoshiai
	ln -sf /usr/lib/judoshiai/bin/judotimer /usr/bin/judotimer
	ln -sf /usr/lib/judoshiai/bin/judoinfo /usr/bin/judoinfo
	ln -sf /usr/lib/judoshiai/bin/judoweight /usr/bin/judoweight
	ln -sf /usr/lib/judoshiai/bin/judojudogi /usr/bin/judojudogi
	ln -sf /usr/lib/judoshiai/bin/judoproxy /usr/bin/judoproxy
	cp gnome/judoshiai.desktop /usr/share/applications/
	cp gnome/judotimer.desktop /usr/share/applications/
	cp gnome/judoinfo.desktop /usr/share/applications/
	cp gnome/judoweight.desktop /usr/share/applications/
	cp gnome/judojudogi.desktop /usr/share/applications/
	cp gnome/judoproxy.desktop /usr/share/applications/
	cp etc/judoshiai.png /usr/share/pixmaps/
	cp etc/judotimer.png /usr/share/pixmaps/
	cp etc/judoinfo.png /usr/share/pixmaps/
	cp etc/judoweight.png /usr/share/pixmaps/
	cp etc/judojudogi.png /usr/share/pixmaps/
	cp etc/judoproxy.png /usr/share/pixmaps/
	cp gnome/judoshiai.mime /usr/share/mime-info/
	cp gnome/judoshiai.keys /usr/share/mime-info/
	cp gnome/judoshiai.applications /usr/share/application-registry/
	cp gnome/judoshiai.packages /usr/lib/mime/packages/judoshiai
	cp gnome/judoshiai.xml /usr/share/mime/packages/
	cp gnome/judoshiai.menu /usr/share/menu/judoshiai

debian:
	cp gnome/*-pak .
	checkinstall -D --install=no --pkgname=judoshiai --pkgversion=$(SHIAI_VER_NUM) \
	--maintainer=oh2ncp@kolumbus.fi --nodoc \
	--requires libao4,libatk1.0-0,libcairo2,libcurl3,libgdk-pixbuf2.0-0,libgtk-3-0,libpango-1.0-0,librsvg2-2
	mv *.deb $(RELDIR)/
	rm description-pak postinstall-pak postremove-pak

clean:
	make -C common clean
	make -C judoshiai clean
	make -C judotimer clean
	make -C judoinfo clean
	make -C judoweight clean
	make -C judojudogi clean
	make -C judoproxy clean
	#make -C doc clean
	rm -rf $(RELEASEDIR)
