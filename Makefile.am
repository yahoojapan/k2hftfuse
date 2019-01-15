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

SUBDIRS=src docs tests buildutils
EXTRA_DIST=RELEASE_VERSION

CPPCHECK_CMD=	cppcheck
CPPCHECK_OPT=	--quiet \
				--error-exitcode=1 \
				--inline-suppr \
				-j 4 \
				--std=c++03 \
				--xml \
				--enable=warning,style,information,missingInclude
CPPCHECK_IGN=

cppcheck:
	$(CPPCHECK_CMD) $(CPPCHECK_OPT) $(CPPCHECK_IGN) $(SUBDIRS)

#
# VIM modelines
#
# vim:set ts=4 fenc=utf-8:
#
