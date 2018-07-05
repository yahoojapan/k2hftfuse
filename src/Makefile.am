#
# k2hftfuse for file transaction by FUSE-based file system
#
# Copyright 2015 Yahoo Japan Corporation.
#
# k2hftfuse is file transaction system on FUSE file system with
# K2HASH and K2HASH TRANSACTION PLUGIN, CHMPX.
#
# For the full copyright and license information, please view
# the license file that was distributed with this source code.
#
# AUTHOR:   Takeshi Nakatani
# CREATE:   Tue Sep 1 2015
# REVISION:
#

pkginclude_HEADERS	=	k2hftcommon.h \
						k2hftdbg.h \
						k2hftutil.h \
						k2hftstructure.h
pkgincludedir		=	$(includedir)/k2hftfuse

bin_PROGRAMS		=	k2hftfuse \
						k2hftfusesvr

k2hftfuse_SOURCES	=	k2hftcommon.h \
						k2hftdbg.h \
						k2hftutil.h \
						k2hftstructure.h \
						k2hftinfo.h \
						k2hftman.h \
						k2hftwbuf.h \
						k2hftiniparser.h \
						k2hftplugin.h \
						k2hftfdcache.h \
						k2hftfuse.cc \
						k2hftdbg.cc \
						k2hftutil.cc \
						k2hftinfo.cc \
						k2hftman.cc \
						k2hftwbuf.cc \
						k2hftiniparser.cc \
						k2hftplugin.cc \
						k2hftfdcache.cc \
						k2hftfuseversion.cc

k2hftfuse_LDADD		=	$(fullock_LIBS) \
						$(k2hash_LIBS) \
						$(chmpx_LIBS) \
						$(fuse_LIBS) \
						-lpthread -lyaml

k2hftfusesvr_SOURCES =	k2hftcommon.h \
						k2hftdbg.h \
						k2hftutil.h \
						k2hftstructure.h \
						k2hftiniparser.h \
						k2hftsvrinfo.h \
						k2hftplugin.h \
						k2hftfdcache.h \
						k2hftfusesvr.cc \
						k2hftdbg.cc \
						k2hftutil.cc \
						k2hftiniparser.cc \
						k2hftsvrinfo.cc \
						k2hftplugin.cc \
						k2hftfdcache.cc \
						k2hftfusesvrversion.cc

k2hftfusesvr_LDADD	=	$(fullock_LIBS) \
						$(k2hash_LIBS) \
						$(chmpx_LIBS) \
						-lyaml -lpthread

ACLOCAL_AMFLAGS		=	-I m4
AM_CFLAGS			=	-D_FILE_OFFSET_BITS=64 -DFUSE_USE_VERSION=26 $(fullock_CFLAGS) $(k2hash_CFLAGS) $(chmpx_CFLAGS) $(fuse_CFLAGS)
AM_CPPFLAGS			=	-D_FILE_OFFSET_BITS=64 -DFUSE_USE_VERSION=26 $(fullock_CFLAGS) $(k2hash_CFLAGS) $(chmpx_CFLAGS) $(fuse_CFLAGS)

### version(commit hash)
.PHONY: k2hftfuseversion

k2hftfuseversion.cc: k2hftfuseversion
	@../buildutils/make_commit_hash_source.sh k2hftfuseversion.cc k2hftfuse_commit_hash

.PHONY: k2hftfusesvrversion

k2hftfusesvrversion.cc: k2hftfusesvrversion
	@../buildutils/make_commit_hash_source.sh k2hftfusesvrversion.cc k2hftfusesvr_commit_hash

#
# VIM modelines
#
# vim:set ts=4 fenc=utf-8:
#
