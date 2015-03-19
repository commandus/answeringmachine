# answeringmachine
AC_PREREQ([2.63])
AC_INIT([answeringmachine], [0.1], [support@commandus.com])
AC_CONFIG_FILES([Makefile])
AM_INIT_AUTOMAKE([1.11 foreign no-define])
AC_CONFIG_SRCDIR([answeringmachine.cpp])
AC_CONFIG_HEADERS([config.h])

m4_include([m4/ax_cxx_compile_stdcxx_11.m4])

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL

# C++ 11
AX_CXX_COMPILE_STDCXX_11

AX_PTHREAD()

# Checks for headers
AC_CHECK_HEADERS([argtable2.h], [], AC_MSG_ERROR([Could not find argtable2.h. Try $ ./configure CFLAGS='-Iyour-argtable2-include-path]))

# Checks for libraries
AC_CHECK_LIB([pthread], [main])
AC_CHECK_LIB([rt], [main])
AC_CHECK_LIB(argtable2, arg_parse, [], AC_MSG_ERROR([Could not find libargtable2. Try $ ./configure LDFLAGS='-Lyour-argtable2-lib-path']))
AC_CHECK_LIB(pj-x86_64-unknown-linux-gnu, main, [], AC_MSG_ERROR([Could not find pj util. Try $ ./configure LDFLAGS='-Lyour-lib-path']))
AC_CHECK_LIB(pjlib-util-x86_64-unknown-linux-gnu, main, [], AC_MSG_ERROR([Could not find pj util. Try $ ./configure LDFLAGS='-Lyour-lib-path']))
AC_CHECK_LIB(pjsip-x86_64-unknown-linux-gnu, main, [], AC_MSG_ERROR([Could not find pj sip. Try $ ./configure LDFLAGS='-Lyour-lib-path']))
AC_CHECK_LIB(srtp-x86_64-unknown-linux-gnu, main, [], AC_MSG_ERROR([Could not find srtp. Try $ ./configure LDFLAGS='-Lyour-lib-path']))
AC_CHECK_LIB(resample-x86_64-unknown-linux-gnu, main, [], AC_MSG_ERROR([Could not find pj resample. Try $ ./configure LDFLAGS='-Lyour-lib-path']))
AC_CHECK_LIB(portaudio-x86_64-unknown-linux-gnu, main, [], AC_MSG_ERROR([Could not find port audio. Try $ ./configure LDFLAGS='-Lyour-lib-path']))
AC_CHECK_LIB(pjmedia-audiodev-x86_64-unknown-linux-gnu, main, [], AC_MSG_ERROR([Could not find pj audiodev. Try $ ./configure LDFLAGS='-Lyour-lib-path']))
AC_CHECK_LIB(pjmedia-x86_64-unknown-linux-gnu, main, [], AC_MSG_ERROR([Could not find pj media. Try $ ./configure LDFLAGS='-Lyour-lib-path']))
AC_CHECK_LIB(pjsip-ua-x86_64-unknown-linux-gnu, main, [], AC_MSG_ERROR([Could not find pj ua. Try $ ./configure LDFLAGS='-Lyour-lib-path']))
AC_CHECK_LIB(pjsip-simple-x86_64-unknown-linux-gnu, main, [], AC_MSG_ERROR([Could not find pj simple. Try $ ./configure LDFLAGS='-Lyour-lib-path']))



# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_HEADER_STDBOOL
AC_TYPE_INT64_T
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_MKTIME
AC_CHECK_FUNCS([gethostbyname inet_ntoa memset strtol])

AC_OUTPUT
