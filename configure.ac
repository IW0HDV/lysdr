AC_PREREQ([2.67])
AC_INIT([lysdr], 
        [m4_esyscmd([./git-version-gen .tarball-version])], 
        [BUG-REPORT-ADDRESS])
dnl AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE
dnl kernel style compile messages
dnl m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AC_CONFIG_SRCDIR([lysdr.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

# Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_INSTALL
AC_PROG_LIBTOOL

# checks for platform dependent compiler optimizations
AX_EXT

# Checks for libraries.
AC_CHECK_LIB([m], [main])
AC_CHECK_LIB([rt], [main])
AC_CHECK_LIB([dl], [main])
AC_SEARCH_LIBS([jack_client_open], [jack],[have_jack=yes])
AC_SEARCH_LIBS([pthread_create], [pthread],[have_pthreads=yes])
AM_PATH_GTK_2_0([2.10.0],,AC_MSG_ERROR([Gtk+ 2.10.0 or higher required.]))
AC_SEARCH_LIBS([fftw_malloc], [fftw3],[have_fftw3=yes],AC_MSG_ERROR([libfftw3 required.]))
AC_SEARCH_LIBS([perseus_init], [perseus-sdr],[have_perseus=yes],AC_MSG_ERROR([perseus-sdr required.]))
AC_SEARCH_LIBS([Pa_Initialize], [portaudio],[have_portaudio=yes],AC_MSG_ERROR([portaudio required.]))
AC_SEARCH_LIBS([src_new], [samplerate],[have_samplerate=yes],AC_MSG_ERROR([samplerate required.]))
AC_SEARCH_LIBS([libusb_init], [usb-1.0],[have_libusb10=yes],AC_MSG_ERROR([libusb1.0 required.]))
AC_SEARCH_LIBS([SetHWLO], [fdms1-hw-ctrl],[have_fdms1=yes],AC_MSG_ERROR([fdms1-hw-ctrl required.]),[-ldl -lusb-1.0 -lc -lm])

# Checks for header files.
AC_CHECK_HEADERS([stdlib.h string.h complex.h unistd.h])

if test "x${have_jack}" = xyes; then
    AC_CHECK_HEADERS([jack/jack.h], [], [have_jack=no])
fi

if test "x${have_jack}" = xno; then
    AC_MSG_ERROR([
-----------------------------------
  The libjack library and header file required to build lysdr.
-----------------------------------])

fi



if test "x${have_fftw3}" = xyes; then
    AC_CHECK_HEADERS([fftw3.h], [], [have_fftw3=no])
fi

if test "x${have_fftw3}" = xno; then
    AC_MSG_ERROR([
-----------------------------------
  The fftw3 library and header file required to build lysdr.
-----------------------------------])

fi


# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([memmove pow])


AC_CONFIG_FILES([Makefile])

AC_OUTPUT
