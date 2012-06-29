
# Process this file with autoconf to produce a configure script.
AC_PREREQ(2.64)
AC_INIT([laser_stimulation],[0.1])
AC_COPYRIGHT([Copyright (C) 2011 Kevin Allen])
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([src/comedi_code.c])
AC_CONFIG_HEADERS([config.h]) 
AC_DEFINE([PACKAGE_COPYRIGHT],["Copyright (C) 2011 Kevin Allen"],[Copyright for the package])
AC_PROG_CC 
AM_PROG_CC_STDC
AM_PROG_CC_C_O
AC_PROG_INSTALL 
AC_SEARCH_LIBS([pthread_create],[pthread])
AC_SEARCH_LIBS([clock_gettime],[rt])
AC_SEARCH_LIBS([sqrt],[m])
AC_SEARCH_LIBS([fftw_execute],[fftw3]) # not working
AC_SEARCH_LIBS([comedi_open],[libcomedi]) # not working
AC_CHECK_HEADERS(stdlib.h string.h sys/time.h unistd.h comedi.h comedilib.h fftw3.h)
AC_FUNC_MALLOC
AC_CHECK_FUNCS([clock_gettime])
AC_CHECK_FUNCS([memset])
AC_CHECK_HEADERS([fcntl.h])
AC_PREREQ
AC_TYPE_OFF_T
AC_CONFIG_FILES([Makefile src/Makefile doc/Makefile])  #generate Makefile from Makefile.in
AC_OUTPUT
