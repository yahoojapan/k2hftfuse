#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.
#
#
# k2hftfuse for file transaction by FUSE-based file system
#
# Copyright 2015 Yahoo! JAPAN corporation.
#
# k2hftfuse is file transaction system on FUSE file system with
# K2HASH and K2HASH TRANSACTION PLUGIN, CHMPX.
#
# For the full copyright and license information, please view
# the LICENSE file that was distributed with this source code.
#
# AUTHOR:   Takeshi Nakatani
# CREATE:   Tue Sep 1 2015
# REVISION:
#

AC_PREREQ([2.63])
AC_INIT(k2hftfuse, m4_esyscmd([tr -d '\n' < $(pwd)/RELEASE_VERSION]))
AM_INIT_AUTOMAKE()
LT_INIT()

AC_CONFIG_SRCDIR([config.h.in])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CXX
AC_PROG_AWK
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_RANLIB
AC_PROG_LIBTOOL

# Checks for header files.
AC_CHECK_HEADERS([stdlib.h stdio.h string.h fcntl.h syslog.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT64_T
AC_TYPE_MODE_T
AC_TYPE_UID_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_MMAP
AC_FUNC_REALLOC
AC_FUNC_FORK
AC_CHECK_FUNCS([memset strcasecmp strncasecmp dup2 realpath strchr clock_gettime fdatasync gethostname mkdir regcomp strdup strrchr strstr mkfifo])
AC_CHECK_HEADERS([limits.h])
AC_CHECK_MEMBERS([struct stat.st_blksize])
AC_CHECK_MEMBERS([struct stat.st_rdev])
AC_STRUCT_ST_BLOCKS

AC_CONFIG_MACRO_DIR([m4])

AC_CONFIG_FILES([Makefile
		docs/Makefile
		src/Makefile
		tests/Makefile])

# fullock
if test "x${CHECK_LIBFULLOCK}" != "xno"; then
	PKG_CHECK_MODULES([fullock], [libfullock >= 1.0.24], [have_fullock=yes], [have_fullock=no])
	if test "x${have_fullock}" = "xno"; then
		AC_MSG_ERROR([not found libfullock-dev package.])
	fi
fi

# k2hash
if test "x${CHECK_K2HASH}" != "xno"; then
	PKG_CHECK_MODULES([k2hash], [libk2hash >= 1.0.56], [have_k2hash=yes], [have_k2hash=no])
	if test "x${have_k2hash}" = "xno"; then
		AC_MSG_ERROR([not found k2hash package.])
	fi
fi

# chmpx
if test "x${CHECK_CHMPX}" != "xno"; then
	PKG_CHECK_MODULES([chmpx], [libchmpx >= 1.0.52], [have_chmpx=yes], [have_chmpx=no])
	if test "x${have_chmpx}" = "xno"; then
		AC_MSG_ERROR([not found chmpx package.])
	fi
fi

# fuse
if test "x${CHECK_FUSE}" != "xno"; then
	PKG_CHECK_MODULES([fuse], [fuse >= 2.9.2], [have_fuse=yes], [have_fuse=no])
	if test "x${have_fuse}" = "xno"; then
		AC_MSG_ERROR([not found fuse package.])
	fi
fi

#
# C99 designated initializers
# (now not use this)
#
# AC_MSG_CHECKING([for working C99 designated initializers])
# AC_CACHE_VAL(	ac_cv_c99_designated_init,
# 		AC_COMPILE_IFELSE(
# 			[AC_LANG_PROGRAM([struct s { int a; int b; };], [[ struct s ss = { .b = 5, .a = 6 }; ]])],
# 			[ac_cv_c99_designated_init=yes],
# 			[ac_cv_c99_designated_init=no])
# 		)
# AC_MSG_RESULT($ac_cv_c99_designated_init)
# if test $ac_cv_c99_designated_init = no; then
# 	CFLAGS="-DNOT_HAVE_C99_DESIGNED_INIT $CFLAGS"
# 	CXXFLAGS="-DNOT_HAVE_C99_DESIGNED_INIT $CXXFLAGS"
# fi

# CFLAGS/CXXFLAGS
CFLAGS="-Wall $CFLAGS"
CXXFLAGS="-Wall $CXXFLAGS"

# LIB_VERSION_INFO for libtool library version info
LIB_VERSION_INFO=`$(pwd)/make_valiables.sh -lib_version_info`
AC_SUBST([LIB_VERSION_INFO])

AC_OUTPUT

#
# VIM modelines
#
# vim:set ts=4 fenc=utf-8:
#
