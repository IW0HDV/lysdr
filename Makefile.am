ACLOCAL_AMFLAGS = -I m4

BUILT_SOURCES = $(top_srcdir)/.version
$(top_srcdir)/.version:
	echo $(VERSION) > $@-t && mv $@-t $@
dist-hook:
	echo $(VERSION) > $(distdir)/.tarball-version

EXTRA_DIST = git-version-gen


bin_PROGRAMS = lysdr

lysdr_CPPFLAGS = @GTK_CFLAGS@ -g -O3 -Wall @SIMD_FLAGS@
lysdr_LDADD = @GTK_LIBS@

lysdr_SOURCES =	\
audio_jack.c  config.h  filter.h  lysdr.c  sdr.c  smeter.c  waterfall.c \
audio_jack.h  filter.c  gui.c     lysdr.h  sdr.h  smeter.h  waterfall.h \
perseus.h perseus.c perseus_audio.c perseus_audio.h \
fdms1.h fdms1.c



