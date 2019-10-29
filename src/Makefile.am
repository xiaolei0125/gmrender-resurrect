bin_PROGRAMS = gmediarender

gmediarender_SOURCES = main.cpp git-version.h \
	upnp_service.cpp upnp_control.cpp upnp_connmgr.cpp  upnp_transport.cpp \
	upnp_service.h upnp_control.h upnp_connmgr.h  upnp_transport.h \
	song-meta-data.h song-meta-data.cpp \
	variable-container.h variable-container.cpp \
	upnp_device.cpp upnp_device.h \
	upnp_renderer.h upnp_renderer.cpp \
	webserver.cpp webserver.h \
	output.cpp output.h \
	logging.h logging.cpp \
	xmldoc.cpp xmldoc.h \
	xmlescape.cpp xmlescape.h

if HAVE_GST
gmediarender_SOURCES += \
	output_gstreamer.cpp  output_gstreamer.h
endif

main.cpp : git-version.h

git-version.h: .FORCE
	$(AM_V_GEN)(echo "#define GM_COMPILE_VERSION \"$(shell git log -n1 --date=short --format='0.0.8_git%cd_%h' 2>/dev/null || echo -n '0.0.8')\"" > $@-new; \
	cmp -s $@ $@-new || cp $@-new $@; \
	rm $@-new)

.FORCE:

AM_CPPFLAGS = $(GLIB_CFLAGS) $(GST_CFLAGS) $(LIBUPNP_CFLAGS) -DPKG_DATADIR=\"$(datadir)/gmediarender\"
gmediarender_LDADD = $(GLIB_LIBS) $(GST_LIBS) $(LIBUPNP_LIBS)
