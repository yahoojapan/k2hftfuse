#!/bin/sh
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

##############################################################
### Utility to stop all processes
###
force_stop_all_processes()
{
	TG_MOUNT_DIR=$1
	CHMPX_PROCS=`ps ax | grep ' chmpx ' | grep -v grep | awk '{print $1}'`
	K2HFTFUSE_PROCS=`ps ax | grep ' k2hftfuse ' | grep -v grep | awk '{print $1}'`
	K2HFTFUSESVR_PROCS=`ps ax | grep ' k2hftfusesvr ' | grep -v grep | awk '{print $1}'`
	if [ "X${CHMPX_PROCS}" != "X" -o "X${K2HFTFUSE_PROCS}" != "X" -o "X${K2HFTFUSESVR_PROCS}" != "X" ]; then
		kill -HUP ${CHMPX_PROCS} ${K2HFTFUSE_PROCS} ${K2HFTFUSESVR_PROCS} 2> /dev/null
		sleep 1
		kill -9 ${CHMPX_PROCS} ${K2HFTFUSE_PROCS} ${K2HFTFUSESVR_PROCS} 2> /dev/null
	fi
	fusermount -u ${TG_MOUNT_DIR} 2> /dev/null
	sleep 10
}

##############################################################
## programs path
##
K2HFUSE_SCRIPT_DIR=`dirname $0`
K2HFUSE_SCRIPT_DIR=`cd -P ${K2HFUSE_SCRIPT_DIR}; pwd`
if [ "X${SRCTOP}" = "X" ]; then
	SRCTOP=`cd -P ${K2HFUSE_SCRIPT_DIR}/..; pwd`
	K2HFUSE_SRC_DIR=`cd -P ${K2HFUSE_SCRIPT_DIR}/../src; pwd`
else
	K2HFUSE_SCRIPT_DIR=`cd -P ${SRCTOP}/tests; pwd`
	K2HFUSE_SRC_DIR=`cd -P ${SRCTOP}/src; pwd`
fi
if [ "X${OBJDIR}" = "X" ]; then
	TESTPROGDIR=${K2HFUSE_SCRIPT_DIR}
	SRCPROGDIR=${K2HFUSE_SRC_DIR}
else
	TESTPROGDIR=${K2HFUSE_SCRIPT_DIR}/${OBJDIR}
	SRCPROGDIR=${K2HFUSE_SRC_DIR}/${OBJDIR}
fi
MOUNTDIR=`cd -P ~; pwd`
MOUNTDIR=${MOUNTDIR}/k2htpfs
INIFILEDIR=${K2HFUSE_SCRIPT_DIR}

K2HFUSEBIN=${SRCPROGDIR}/k2hftfuse
K2HFUSESVRBIN=${SRCPROGDIR}/k2hftfusesvr
K2HFUSETESTBIN=${TESTPROGDIR}/k2hftfusetest

##############################################################
## Parameters
##############################################################
#
# Check only INI as default
#
DO_INI_CONF=yes
DO_YAML_CONF=no
DO_JSON_CONF=no
DO_JSON_STRING=no
DO_JSON_ENV=no
DEBUGMODE=msg

if [ $# -ne 0 ]; then
	DO_INI_CONF=no
	DO_YAML_CONF=no
	DO_JSON_CONF=no
	DO_JSON_STRING=no
	DO_JSON_ENV=no

	while [ $# -ne 0 ]; do
		if [ "X$1" = "Xhelp" -o "X$1" = "Xh" -o "X$1" = "X-help" -o "X$1" = "X-h" ]; then
			PROGRAM_NAME=`basename $0`
			echo "Usage: ${PROGRAM_NAME} { -help } { ini_conf } { yaml_conf } { json_conf } { json_string } { json_env } { silent | err | wan | msg }"
			echo "       no option means all process doing."
			echo ""
			exit 0
		elif [ "X$1" = "Xini_conf" ]; then
			DO_INI_CONF=yes
		elif [ "X$1" = "Xyaml_conf" ]; then
			DO_YAML_CONF=yes
		elif [ "X$1" = "Xjson_conf" ]; then
			DO_JSON_CONF=yes
		elif [ "X$1" = "Xjson_string" ]; then
			DO_JSON_STRING=yes
		elif [ "X$1" = "Xjson_env" ]; then
			DO_JSON_ENV=yes
		elif [ "X$1" = "Xsilent" ]; then
			DEBUGMODE=$1
		elif [ "X$1" = "Xerr" ]; then
			DEBUGMODE=$1
		elif [ "X$1" = "Xwan" ]; then
			DEBUGMODE=$1
		elif [ "X$1" = "Xmsg" ]; then
			DEBUGMODE=$1
		else
			echo "[ERROR] unknown option $1 specified."
			echo ""
			exit 1
		fi
		shift
	done
fi

#
# Check fusermount/etc and permission
#
# [NOTE]
# In this test, if the FUSE environment is not in place,
# the test can not be carried out. This case returns SUCCESS
# without error.
#
IS_SAFE_TEST=1
if ! command -v fusermount >/dev/null 2>&1; then
	echo ""
	echo "[WARNING] not found fusermount command or not allow permission, then this test is going to fail."
	echo ""
	IS_SAFE_TEST=1
else
	if [ ! -f /dev/fuse ]; then
		echo ""
		echo "[WARNING] not found /dev/fuse or not allow permission, then this test is going to fail."
		echo ""
		IS_SAFE_TEST=1
	else
		if [ ! -f /etc/fuse.conf ]; then
			echo "[MESSAGE] not found /etc/fuse.conf, try to set up by preinst script."

			# setup
			/bin/sh ${SRCTOP}/buildutils/k2hftfuse.preinst install
		fi

		if ! grep user_allow_other /etc/fuse.conf >/dev/null 2>&1; then
			echo "[MESSAGE] not found user_allow_other in /etc/fuse.conf, try to set up by preinst script."
			# setup
			/bin/sh ${SRCTOP}/buildutils/k2hftfuse.preinst install

			# retry
			grep user_allow_other /etc/fuse.conf >/dev/null 2>&1
		fi
		if [ $? -ne 0 ]; then
			echo ""
			echo "[WARNING] not found user_allow_other in /etc/fuse.conf, then this test is going to fail."
			echo ""
			IS_SAFE_TEST=1
		fi
	fi
fi
if [ ${IS_SAFE_TEST} -ne 1 ]; then
	echo ""
	echo "[NOTICE]"
	echo ""
	echo "This test is going to fail, because this environment is not allowed"
	echo "to use FUSE for me."
	echo "But this script returns SUCCESS, if it reason is not using FUSE."
	echo ""
	echo "PLEASE CHECK YOUR FUSE ENVIRONMENT."
	echo ""
fi

##############################################################
## log files
##############################################################
SINGLE_CHMPX_SERVER_LOG=/tmp/test/single_chmpx_server.log
SINGLE_CHMPX_SLAVE_LOG=/tmp/test/single_chmpx_slave.log
SINGLE_K2HFTFUSESVR_LOG=/tmp/test/single_k2hftfusesvr.log
SINGLE_K2HFTFUSE_LOG=/tmp/test/single_k2hftfuse.log
SINGLE_K2HFTFUSE_CHMPX_LOG=/tmp/test/single_k2hftfuse_chmpx.log
SINGLE_K2HFTFUSE_K2HASH_LOG=/tmp/test/single_k2hftfuse_k2hash.log
SINGLE_K2HFTFUSETEST_LOG=/tmp/test/single_k2hftfusetest.log

NEST_TRANS_CHMPX_SERVER_LOG=/tmp/test/nest_trans_chmpx_server.log
NEST_CHMPX_SERVER_LOG=/tmp/test/nest_chmpx_server.log
NEST_TRANS_CHMPX_SLAVE_LOG=/tmp/test/nest_trans_chmpx_slave.log
NEST_CHMPX_SLAVE_LOG=/tmp/test/nest_chmpx_slave.log
NEST_TRANS_K2HFTFUSESVR_LOG=/tmp/test/nest_trans_k2hftfusesvr.log
NEST_K2HFTFUSESVR_LOG=/tmp/test/nest_k2hftfusesvr.log
NEST_K2HFTFUSE_LOG=/tmp/test/nest_k2hftfuse.log
NEST_K2HFTFUSE_CHMPX_LOG=/tmp/test/nest_k2hftfuse_chmpx.log
NEST_K2HFTFUSE_K2HASH_LOG=/tmp/test/nest_k2hftfuse_k2hash.log
NEST_K2HFTFUSETEST_LOG=/tmp/test/nest_k2hftfusetest.log


##############################################################
## COMMON
##############################################################
#
# Json Parameter
#
K2HFTFUSESVR_TEST_TRANS_SERVER_JSON=`grep 'K2HFTFUSESVR_TEST_TRANS_SERVER_JSON=' ${INIFILEDIR}/test_json_string.data 2>/dev/null | sed 's/K2HFTFUSESVR_TEST_TRANS_SERVER_JSON=//g' 2>/dev/null`
K2HFTFUSESVR_TEST_TRANS_SLAVE_JSON=`grep 'K2HFTFUSESVR_TEST_TRANS_SLAVE_JSON=' ${INIFILEDIR}/test_json_string.data 2>/dev/null | sed 's/K2HFTFUSESVR_TEST_TRANS_SLAVE_JSON=//g' 2>/dev/null`
K2HFTFUSE_TEST_SERVER_JSON=`grep 'K2HFTFUSE_TEST_SERVER_JSON=' ${INIFILEDIR}/test_json_string.data 2>/dev/null | sed 's/K2HFTFUSE_TEST_SERVER_JSON=//g' 2>/dev/null`
K2HFTFUSE_TEST_SLAVE_JSON=`grep 'K2HFTFUSE_TEST_SLAVE_JSON=' ${INIFILEDIR}/test_json_string.data 2>/dev/null | sed 's/K2HFTFUSE_TEST_SLAVE_JSON=//g' 2>/dev/null`
K2HFTFUSE_TEST_SLAVE_JSON_FUSE=`grep 'K2HFTFUSE_TEST_SLAVE_JSON=' ${INIFILEDIR}/test_json_string.data 2>/dev/null | sed 's/K2HFTFUSE_TEST_SLAVE_JSON=//g' 2>/dev/null | sed 's/,/\\\\,/g' 2>/dev/null`
K2HFTFUSE_TEST_TRANS_SERVER_JSON=`grep 'K2HFTFUSE_TEST_TRANS_SERVER_JSON=' ${INIFILEDIR}/test_json_string.data 2>/dev/null | sed 's/K2HFTFUSE_TEST_TRANS_SERVER_JSON=//g' 2>/dev/null`

#
# Result
#
TESTSUBRESULT=0

#
# Version
#
${K2HFUSEBIN} -v

#
# Cleaning
#
rm -f /tmp/mnt/k2hftfusesvr/log/unify.log
if [ $? -ne 0 ]; then
	echo "RESULT --> FAILED(COULD NOT REMOVE RESULT FILE)"
	exit 1
fi
fusermount -u ${MOUNTDIR} > /dev/null 2>&1
if [ -d ${MOUNTDIR} ]; then
	rm -rf ${MOUNTDIR}
fi
mkdir -p ${MOUNTDIR}
if [ $? -ne 0 ]; then
	echo "RESULT --> FAILED(COULD NOT MAKE MOUNTPOINT)"
	exit 1
fi

#
# clean old files in /tmp
#
rm -rf /tmp/mnt/k2hftfuse
rm -rf /tmp/mnt/k2hftfusesvr
rm -rf /tmp/test/K2HFUSESVRTEST-*.chmpx
rm -rf /tmp/test/K2HFUSESVRTEST-*.k2h
rm -rf /tmp/test/K2HFUSETEST-*.chmpx
rm -rf /tmp/test/K2HFUSETEST-*.k2h
rm -rf /tmp/k2hftfuse_np_*
rm -rf /tmp/test/nest_*.log
rm -rf /tmp/test/single_*.log

#
# mount point and log directory, etc
#
if [ -d /tmp/test -o -f /tmp/test ]; then
	rm -rf /tmp/test
fi
if [ -d /tmp/mnt -o -f /tmp/mnt ]; then
	rm -rf /tmp/mnt
fi
mkdir /tmp/test
mkdir /tmp/mnt

##############################################################
# Start INI conf file
##############################################################
if [ "X${DO_INI_CONF}" = "Xyes" ]; then
	echo ""
	echo ""
	echo "====== START TEST FOR INI CONF FILE ========================"
	echo ""
	CONF_FILE_EXT=".ini"

	#######################
	# TEST for SINGLE SERVER
	#######################
	#
	# chmpx for server
	#
	echo "------ RUN chmpx on server side ----------------------------"
	chmpx -conf ${INIFILEDIR}/k2hftfuse_test_server${CONF_FILE_EXT} -d ${DEBUGMODE} > ${SINGLE_CHMPX_SERVER_LOG} 2>&1 &
	CHMPXSVRPID=$!
	echo "chmpx on server side pid = ${CHMPXSVRPID}"
	sleep 2

	#
	# k2hftfusesvr server process
	#
	echo "------ RUN server process on server side ------------------"
	${K2HFUSESVRBIN} -conf ${INIFILEDIR}/k2hftfuse_test_server${CONF_FILE_EXT} -d ${DEBUGMODE} > ${SINGLE_K2HFTFUSESVR_LOG} 2>&1 &
	K2HFTFUSESVRPID=$!
	echo "server process pid = ${K2HFTFUSESVRPID}"
	sleep 2

	#
	# chmpx for slave
	#
	echo "------ RUN chmpx on slave side ----------------------------"
	chmpx -conf ${INIFILEDIR}/k2hftfuse_test_slave${CONF_FILE_EXT} -d ${DEBUGMODE} > ${SINGLE_CHMPX_SLAVE_LOG} 2>&1 &
	CHMPXSLVPID=$!
	echo "chmpx on slave side pid = ${CHMPXSLVPID}"
	sleep 2

	#
	# fuse client( without allow_other option)
	#
	echo "------ RUN fuse client ------------------------------------"
	K2HDBGMODE=INFO K2HDBGFILE=${SINGLE_K2HFTFUSE_K2HASH_LOG} CHMDBGMODE=MSG CHMDBGFILE=${SINGLE_K2HFTFUSE_CHMPX_LOG} ${K2HFUSEBIN} ${MOUNTDIR} -o dbglevel=msg,conf=${INIFILEDIR}/k2hftfuse_test_slave${CONF_FILE_EXT},chmpxlog=${SINGLE_K2HFTFUSE_CHMPX_LOG} -f > ${SINGLE_K2HFTFUSE_LOG} 2>&1 &
	K2HFTFUSEPID=$!
	echo "k2hftfuse pid = ${K2HFTFUSEPID}"
	sleep 10

	#
	# check process
	#
	RESULT_RUN_K2HFTFUSE_PROC=true
	ps -p ${K2HFTFUSEPID} > /dev/null 2>&1
	if [ $? != 0 ]; then
		echo "ERROR: could not run k2hftfuse process"
		RESULT_RUN_K2HFTFUSE_PROC=false
	fi

	#
	# test( we need to run this if failed to run k2hftfuse for gcov )
	#
	echo "------ RUN test program(k2hftfusetest) --------------------"
	${K2HFUSETESTBIN} ${MOUNTDIR}/log/nothing2.log 10 MY_TEST_PROGRAM_OUTPUT > ${SINGLE_K2HFTFUSETEST_LOG} 2>&1
	TESTSUBRESULT=$?
	echo "k2hftfusetest finished."
	sleep 2

	#
	# exit all process
	#
	kill -HUP ${K2HFTFUSEPID} > /dev/null 2>&1
	sleep 1
	kill -HUP ${K2HFTFUSESVRPID} > /dev/null 2>&1
	sleep 1
	kill -HUP ${CHMPXSLVPID} > /dev/null 2>&1
	sleep 1
	kill -HUP ${CHMPXSVRPID} > /dev/null 2>&1
	sleep 1
	kill -9 ${K2HFTFUSEPID} ${K2HFTFUSESVRPID} ${CHMPXSLVPID} ${CHMPXSVRPID} > /dev/null 2>&1

	#
	# check line count
	#
	LINES=`wc -l /tmp/mnt/k2hftfusesvr/log/unify.log 2> /dev/null | awk '{print $1}' 2> /dev/null`
	if [ "X$LINES" != "X10" ]; then
		echo "ERROR: unify.log file line count(${LINES}) is not as same as 10."
		TESTSUBRESULT=1
	fi

	#
	# result for single test
	#
	if [ ${TESTSUBRESULT} -ne 0 ]; then
		echo ""
		echo "### FAILED ON SINGLE TEST #########################################################################"
		echo ""
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${SINGLE_CHMPX_SERVER_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${SINGLE_CHMPX_SERVER_LOG}
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${SINGLE_CHMPX_SLAVE_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${SINGLE_CHMPX_SLAVE_LOG}
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${SINGLE_K2HFTFUSESVR_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${SINGLE_K2HFTFUSESVR_LOG}
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${SINGLE_K2HFTFUSE_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${SINGLE_K2HFTFUSE_LOG}
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${SINGLE_K2HFTFUSE_CHMPX_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${SINGLE_K2HFTFUSE_CHMPX_LOG}
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${SINGLE_K2HFTFUSE_K2HASH_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${SINGLE_K2HFTFUSE_K2HASH_LOG}
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${SINGLE_K2HFTFUSETEST_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${SINGLE_K2HFTFUSETEST_LOG}
		echo "---------------------------------------------------------------------------------------------------"

		if [ "X${RESULT_RUN_K2HFTFUSE_PROC}" = "Xtrue" ]; then
			echo ""
			echo "RESULT --> FAILED(ON SINGLE TEST)"
			echo ""
			exit 1
		fi
		if [ ${IS_SAFE_TEST} -eq 0 ]; then
			echo ""
			echo "RESULT --> FAILED(ON SINGLE TEST)"
			echo ""
			exit 1
		fi

		echo ""
		echo "***************************************************************************************************"
		echo "This test is run on CI(docker) and we could not run k2hftfuse process."
		echo "Maybe, you need to set \"--cap-add SYS_ADMIN --device /dev/fuse --security-opt apparmor:unconfined\""
		echo "to your docker."
		echo "And you can find \"fuse: failed to open /dev/fuse: Operation not permitted\" in logs, when this failure"
		echo "reason is permission on docker."
		echo ""
		echo "***** THEN WE SKIP THIS TEST, MUST CHECK TEST ON YOUR LOCAL ENVIRONMENT *****"
		echo ""
		echo "***************************************************************************************************"
		echo ""
	else
		#
		# cleaning
		#
		rm -f /tmp/mnt/k2hftfusesvr/log/unify.log
		if [ $? -ne 0 ]; then
			echo "RESULT --> FAILED(COULD NOT REMOVE RESULT FILE)"
			exit 1
		fi
	fi
	force_stop_all_processes ${MOUNTDIR} > /dev/null 2>&1

	#
	# clean old files in /tmp
	#
	rm -rf /tmp/mnt/k2hftfuse
	rm -rf /tmp/mnt/k2hftfusesvr
	rm -rf /tmp/test/K2HFUSESVRTEST-*.chmpx
	rm -rf /tmp/test/K2HFUSESVRTEST-*.k2h
	rm -rf /tmp/test/K2HFUSETEST-*.chmpx
	rm -rf /tmp/test/K2HFUSETEST-*.k2h
	rm -rf /tmp/k2hftfuse_np_*
	rm -rf /tmp/test/nest_*.log
	rm -rf /tmp/test/single_*.log

	#######################
	# TEST for NEST SERVER
	#######################
	#
	# chmpx for transferred server
	#
	echo "------ RUN transferred chmpx on server side ----------------"
	chmpx -conf ${INIFILEDIR}/k2hftfusesvr_test_trans_server${CONF_FILE_EXT} -d ${DEBUGMODE} > ${NEST_TRANS_CHMPX_SERVER_LOG} 2>&1 &
	CHMPXTRANSSVRPID=$!
	echo "transferred chmpx on server side pid = ${CHMPXTRANSSVRPID}"
	sleep 2

	#
	# k2hftfusesvr transferred server process
	#
	echo "------ RUN transferred server process on server side -------"
	${K2HFUSESVRBIN} -conf ${INIFILEDIR}/k2hftfusesvr_test_trans_server${CONF_FILE_EXT} -d ${DEBUGMODE} > ${NEST_TRANS_K2HFTFUSESVR_LOG} 2>&1 &
	K2HFTFUSESVRTRANSPID=$!
	echo "transferred server process pid = ${K2HFTFUSESVRTRANSPID}"
	sleep 2

	#
	# transferred chmpx for slave
	#
	echo "------ RUN transferred chmpx on slave side -----------------"
	chmpx -conf ${INIFILEDIR}/k2hftfusesvr_test_trans_slave${CONF_FILE_EXT} -d ${DEBUGMODE} > ${NEST_TRANS_CHMPX_SLAVE_LOG} 2>&1 &
	CHMPXTRANSSLVPID=$!
	echo "transferred chmpx on slave side pid = ${CHMPXTRANSSLVPID}"
	sleep 2

	#
	# chmpx for server
	#
	echo "------ RUN chmpx on server side ----------------------------"
	chmpx -conf ${INIFILEDIR}/k2hftfuse_test_trans_server${CONF_FILE_EXT} -d ${DEBUGMODE} > ${NEST_CHMPX_SERVER_LOG} 2>&1 &
	CHMPXSVRPID=$!
	echo "chmpx on server side pid = ${CHMPXSVRPID}"
	sleep 2

	#
	# k2hftfusesvr server process
	#
	echo "------ RUN server process on server side ------------------"
	${K2HFUSESVRBIN} -conf ${INIFILEDIR}/k2hftfuse_test_trans_server${CONF_FILE_EXT} -d ${DEBUGMODE} > ${NEST_K2HFTFUSESVR_LOG} 2>&1 &
	K2HFTFUSESVRPID=$!
	echo "server process pid = ${K2HFTFUSESVRPID}"
	sleep 2

	#
	# chmpx for slave
	#
	echo "------ RUN chmpx on slave side ----------------------------"
	chmpx -conf ${INIFILEDIR}/k2hftfuse_test_slave${CONF_FILE_EXT} -d ${DEBUGMODE} > ${NEST_CHMPX_SLAVE_LOG} 2>&1 &
	CHMPXSLVPID=$!
	echo "chmpx on slave side pid = ${CHMPXSLVPID}"
	sleep 2

	#
	# fuse client ( without allow_other option )
	#
	echo "------ RUN fuse client ------------------------------------"
	K2HDBGMODE=INFO K2HDBGFILE=${NEST_K2HFTFUSE_K2HASH_LOG} CHMDBGMODE=MSG CHMDBGFILE=${NEST_K2HFTFUSE_CHMPX_LOG} ${K2HFUSEBIN} ${MOUNTDIR} -o dbglevel=msg,conf=${INIFILEDIR}/k2hftfuse_test_slave${CONF_FILE_EXT},chmpxlog=${NEST_K2HFTFUSE_CHMPX_LOG} -f > ${NEST_K2HFTFUSE_LOG} 2>&1 &
	K2HFTFUSEPID=$!
	echo "k2hftfuse pid = ${K2HFTFUSEPID}"
	sleep 10

	#
	# check process
	#
	RESULT_RUN_K2HFTFUSE_PROC=true
	ps -p ${K2HFTFUSEPID} > /dev/null 2>&1
	if [ $? != 0 ]; then
		echo "ERROR: could not run k2hftfuse process"
		RESULT_RUN_K2HFTFUSE_PROC=false
	fi

	#
	# test( we need to run this if failed to run k2hftfuse for gcov )
	#
	echo "------ RUN test program(k2hftfusetest) --------------------"
	${K2HFUSETESTBIN} ${MOUNTDIR}/log/nothing2.log 10 MY_TEST_PROGRAM_OUTPUT > ${NEST_K2HFTFUSETEST_LOG} 2>&1
	TESTSUBRESULT=$?
	echo "k2hftfusetest finished."
	sleep 2

	#
	# exit all process
	#
	kill -HUP ${K2HFTFUSEPID} > /dev/null 2>&1
	sleep 1
	kill -HUP ${K2HFTFUSESVRPID} > /dev/null 2>&1
	sleep 1
	kill -HUP ${K2HFTFUSESVRTRANSPID} > /dev/null 2>&1
	sleep 1
	kill -HUP ${CHMPXSLVPID} > /dev/null 2>&1
	sleep 1
	kill -HUP ${CHMPXSVRPID} > /dev/null 2>&1
	sleep 1
	kill -HUP ${CHMPXTRANSSLVPID} > /dev/null 2>&1
	sleep 1
	kill -HUP ${CHMPXTRANSSVRPID} > /dev/null 2>&1
	sleep 1
	kill -9 ${K2HFTFUSEPID} ${K2HFTFUSESVRPID} ${K2HFTFUSESVRTRANSPID} ${CHMPXSLVPID} ${CHMPXSVRPID} ${CHMPXTRANSSLVPID} ${CHMPXTRANSSVRPID} > /dev/null 2>&1

	#
	# check line count
	#
	LINES=`wc -l /tmp/mnt/k2hftfusesvr/log/unify.log 2> /dev/null | awk '{print $1}' 2> /dev/null`
	if [ "X$LINES" != "X10" ]; then
		echo "ERROR: unify.log file line count(${LINES}) is not as same as 10."
		TESTSUBRESULT=1
	fi

	#
	# result for single test
	#
	if [ ${TESTSUBRESULT} -ne 0 ]; then
		echo ""
		echo "### FAILED ON NEST TEST ###########################################################################"
		echo ""
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${NEST_TRANS_CHMPX_SERVER_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${NEST_TRANS_CHMPX_SERVER_LOG}
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${NEST_CHMPX_SERVER_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${NEST_CHMPX_SERVER_LOG}
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${NEST_TRANS_CHMPX_SLAVE_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${NEST_TRANS_CHMPX_SLAVE_LOG}
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${NEST_CHMPX_SLAVE_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${NEST_CHMPX_SLAVE_LOG}
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${NEST_TRANS_K2HFTFUSESVR_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${NEST_TRANS_K2HFTFUSESVR_LOG}
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${NEST_K2HFTFUSESVR_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${NEST_K2HFTFUSESVR_LOG}
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${NEST_K2HFTFUSE_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${NEST_K2HFTFUSE_LOG}
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${NEST_K2HFTFUSE_CHMPX_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${NEST_K2HFTFUSE_CHMPX_LOG}
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${NEST_K2HFTFUSE_K2HASH_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${NEST_K2HFTFUSE_K2HASH_LOG}
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${NEST_K2HFTFUSETEST_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${NEST_K2HFTFUSETEST_LOG}
		echo "---------------------------------------------------------------------------------------------------"

		if [ "X${RESULT_RUN_K2HFTFUSE_PROC}" = "Xtrue" ]; then
			echo ""
			echo "RESULT --> FAILED(ON NEST TEST)"
			echo ""
			exit 1
		fi

		if [ ${IS_SAFE_TEST} -eq 0 ]; then
			echo ""
			echo "RESULT --> FAILED(ON NEST TEST)"
			echo ""
			exit 1
		fi

		echo ""
		echo "***************************************************************************************************"
		echo "This test is run on CI(docker) and we could not run k2hftfuse process."
		echo "Maybe, you need to set \"--cap-add SYS_ADMIN --device /dev/fuse --security-opt apparmor:unconfined\""
		echo "to your docker."
		echo "And you can find \"fuse: failed to open /dev/fuse: Operation not permitted\" in logs, when this failure"
		echo "reason is permission on docker."
		echo ""
		echo "***** THEN WE SKIP THIS TEST, MUST CHECK TEST ON YOUR LOCAL ENVIRONMENT *****"
		echo ""
		echo "***************************************************************************************************"
		echo ""
	else
		#
		# cleaning
		#
		rm -f /tmp/mnt/k2hftfusesvr/log/unify.log
		if [ $? -ne 0 ]; then
			echo "RESULT --> FAILED(COULD NOT REMOVE RESULT FILE)"
			exit 1
		fi
	fi
	force_stop_all_processes ${MOUNTDIR} > /dev/null 2>&1

	#
	# clean old files in /tmp
	#
	rm -rf /tmp/mnt/k2hftfuse
	rm -rf /tmp/mnt/k2hftfusesvr
	rm -rf /tmp/test/K2HFUSESVRTEST-*.chmpx
	rm -rf /tmp/test/K2HFUSESVRTEST-*.k2h
	rm -rf /tmp/test/K2HFUSETEST-*.chmpx
	rm -rf /tmp/test/K2HFUSETEST-*.k2h
	rm -rf /tmp/k2hftfuse_np_*
	rm -rf /tmp/test/nest_*.log
	rm -rf /tmp/test/single_*.log

	#######################
	# Finish
	#######################
	echo ""
	echo "INI CONF TEST RESULT --> SUCCEED"
	echo ""
fi

##############################################################
# Start yaml conf file
##############################################################
if [ "X${DO_YAML_CONF}" = "Xyes" ]; then
	echo ""
	echo ""
	echo "====== START TEST FOR YAML CONF FILE ======================="
	echo ""
	CONF_FILE_EXT=".yaml"

	#######################
	# TEST for SINGLE SERVER
	#######################
	#
	# chmpx for server
	#
	echo "------ RUN chmpx on server side ----------------------------"
	chmpx -conf ${INIFILEDIR}/k2hftfuse_test_server${CONF_FILE_EXT} -d ${DEBUGMODE} > ${SINGLE_CHMPX_SERVER_LOG} 2>&1 &
	CHMPXSVRPID=$!
	echo "chmpx on server side pid = ${CHMPXSVRPID}"
	sleep 2

	#
	# k2hftfusesvr server process
	#
	echo "------ RUN server process on server side ------------------"
	${K2HFUSESVRBIN} -conf ${INIFILEDIR}/k2hftfuse_test_server${CONF_FILE_EXT} -d ${DEBUGMODE} > ${SINGLE_K2HFTFUSESVR_LOG} 2>&1 &
	K2HFTFUSESVRPID=$!
	echo "server process pid = ${K2HFTFUSESVRPID}"
	sleep 2

	#
	# chmpx for slave
	#
	echo "------ RUN chmpx on slave side ----------------------------"
	chmpx -conf ${INIFILEDIR}/k2hftfuse_test_slave${CONF_FILE_EXT} -d ${DEBUGMODE} > ${SINGLE_CHMPX_SLAVE_LOG} 2>&1 &
	CHMPXSLVPID=$!
	echo "chmpx on slave side pid = ${CHMPXSLVPID}"
	sleep 2

	#
	# fuse client( without allow_other option)
	#
	echo "------ RUN fuse client ------------------------------------"
	K2HDBGMODE=INFO K2HDBGFILE=${SINGLE_K2HFTFUSE_K2HASH_LOG} CHMDBGMODE=MSG CHMDBGFILE=${SINGLE_K2HFTFUSE_CHMPX_LOG} ${K2HFUSEBIN} ${MOUNTDIR} -o dbglevel=msg,conf=${INIFILEDIR}/k2hftfuse_test_slave${CONF_FILE_EXT},chmpxlog=${SINGLE_K2HFTFUSE_CHMPX_LOG} -f > ${SINGLE_K2HFTFUSE_LOG} 2>&1 &
	K2HFTFUSEPID=$!
	echo "k2hftfuse pid = ${K2HFTFUSEPID}"
	sleep 10

	#
	# check process
	#
	RESULT_RUN_K2HFTFUSE_PROC=true
	ps -p ${K2HFTFUSEPID} > /dev/null 2>&1
	if [ $? != 0 ]; then
		echo "ERROR: could not run k2hftfuse process"
		RESULT_RUN_K2HFTFUSE_PROC=false
	fi

	#
	# test( we need to run this if failed to run k2hftfuse for gcov )
	#
	echo "------ RUN test program(k2hftfusetest) --------------------"
	${K2HFUSETESTBIN} ${MOUNTDIR}/log/nothing2.log 10 MY_TEST_PROGRAM_OUTPUT > ${SINGLE_K2HFTFUSETEST_LOG} 2>&1
	TESTSUBRESULT=$?
	echo "k2hftfusetest finished."
	sleep 2

	#
	# exit all process
	#
	kill -HUP ${K2HFTFUSEPID} > /dev/null 2>&1
	sleep 1
	kill -HUP ${K2HFTFUSESVRPID} > /dev/null 2>&1
	sleep 1
	kill -HUP ${CHMPXSLVPID} > /dev/null 2>&1
	sleep 1
	kill -HUP ${CHMPXSVRPID} > /dev/null 2>&1
	sleep 1
	kill -9 ${K2HFTFUSEPID} ${K2HFTFUSESVRPID} ${CHMPXSLVPID} ${CHMPXSVRPID} > /dev/null 2>&1

	#
	# check line count
	#
	LINES=`wc -l /tmp/mnt/k2hftfusesvr/log/unify.log 2> /dev/null | awk '{print $1}' 2> /dev/null`
	if [ "X$LINES" != "X10" ]; then
		echo "ERROR: unify.log file line count(${LINES}) is not as same as 10."
		TESTSUBRESULT=1
	fi

	#
	# result for single test
	#
	if [ ${TESTSUBRESULT} -ne 0 ]; then
		echo ""
		echo "### FAILED ON SINGLE TEST #########################################################################"
		echo ""
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${SINGLE_CHMPX_SERVER_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${SINGLE_CHMPX_SERVER_LOG}
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${SINGLE_CHMPX_SLAVE_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${SINGLE_CHMPX_SLAVE_LOG}
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${SINGLE_K2HFTFUSESVR_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${SINGLE_K2HFTFUSESVR_LOG}
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${SINGLE_K2HFTFUSE_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${SINGLE_K2HFTFUSE_LOG}
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${SINGLE_K2HFTFUSE_CHMPX_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${SINGLE_K2HFTFUSE_CHMPX_LOG}
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${SINGLE_K2HFTFUSE_K2HASH_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${SINGLE_K2HFTFUSE_K2HASH_LOG}
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${SINGLE_K2HFTFUSETEST_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${SINGLE_K2HFTFUSETEST_LOG}
		echo "---------------------------------------------------------------------------------------------------"

		if [ "X${RESULT_RUN_K2HFTFUSE_PROC}" = "Xtrue" ]; then
			echo ""
			echo "RESULT --> FAILED(ON SINGLE TEST)"
			echo ""
			exit 1
		fi

		if [ ${IS_SAFE_TEST} -eq 0 ]; then
			echo ""
			echo "RESULT --> FAILED(ON SINGLE TEST)"
			echo ""
			exit 1
		fi

		echo ""
		echo "***************************************************************************************************"
		echo "This test is run on CI(docker) and we could not run k2hftfuse process."
		echo "Maybe, you need to set \"--cap-add SYS_ADMIN --device /dev/fuse --security-opt apparmor:unconfined\""
		echo "to your docker."
		echo "And you can find \"fuse: failed to open /dev/fuse: Operation not permitted\" in logs, when this failure"
		echo "reason is permission on docker."
		echo ""
		echo "***** THEN WE SKIP THIS TEST, MUST CHECK TEST ON YOUR LOCAL ENVIRONMENT *****"
		echo ""
		echo "***************************************************************************************************"
		echo ""
	else
		#
		# cleaning
		#
		rm -f /tmp/mnt/k2hftfusesvr/log/unify.log
		if [ $? -ne 0 ]; then
			echo "RESULT --> FAILED(COULD NOT REMOVE RESULT FILE)"
			exit 1
		fi
	fi
	force_stop_all_processes ${MOUNTDIR} > /dev/null 2>&1

	#
	# clean old files in /tmp
	#
	rm -rf /tmp/mnt/k2hftfuse
	rm -rf /tmp/mnt/k2hftfusesvr
	rm -rf /tmp/test/K2HFUSESVRTEST-*.chmpx
	rm -rf /tmp/test/K2HFUSESVRTEST-*.k2h
	rm -rf /tmp/test/K2HFUSETEST-*.chmpx
	rm -rf /tmp/test/K2HFUSETEST-*.k2h
	rm -rf /tmp/k2hftfuse_np_*
	rm -rf /tmp/test/nest_*.log
	rm -rf /tmp/test/single_*.log

	#######################
	# TEST for NEST SERVER
	#######################
	#
	# chmpx for transferred server
	#
	echo "------ RUN transferred chmpx on server side ----------------"
	chmpx -conf ${INIFILEDIR}/k2hftfusesvr_test_trans_server${CONF_FILE_EXT} -d ${DEBUGMODE} > ${NEST_TRANS_CHMPX_SERVER_LOG} 2>&1 &
	CHMPXTRANSSVRPID=$!
	echo "transferred chmpx on server side pid = ${CHMPXTRANSSVRPID}"
	sleep 2

	#
	# k2hftfusesvr transferred server process
	#
	echo "------ RUN transferred server process on server side -------"
	${K2HFUSESVRBIN} -conf ${INIFILEDIR}/k2hftfusesvr_test_trans_server${CONF_FILE_EXT} -d ${DEBUGMODE} > ${NEST_TRANS_K2HFTFUSESVR_LOG} 2>&1 &
	K2HFTFUSESVRTRANSPID=$!
	echo "transferred server process pid = ${K2HFTFUSESVRTRANSPID}"
	sleep 2

	#
	# transferred chmpx for slave
	#
	echo "------ RUN transferred chmpx on slave side -----------------"
	chmpx -conf ${INIFILEDIR}/k2hftfusesvr_test_trans_slave${CONF_FILE_EXT} -d ${DEBUGMODE} > ${NEST_TRANS_CHMPX_SLAVE_LOG} 2>&1 &
	CHMPXTRANSSLVPID=$!
	echo "transferred chmpx on slave side pid = ${CHMPXTRANSSLVPID}"
	sleep 2

	#
	# chmpx for server
	#
	echo "------ RUN chmpx on server side ----------------------------"
	chmpx -conf ${INIFILEDIR}/k2hftfuse_test_trans_server${CONF_FILE_EXT} -d ${DEBUGMODE} > ${NEST_CHMPX_SERVER_LOG} 2>&1 &
	CHMPXSVRPID=$!
	echo "chmpx on server side pid = ${CHMPXSVRPID}"
	sleep 2

	#
	# k2hftfusesvr server process
	#
	echo "------ RUN server process on server side ------------------"
	${K2HFUSESVRBIN} -conf ${INIFILEDIR}/k2hftfuse_test_trans_server${CONF_FILE_EXT} -d ${DEBUGMODE} > ${NEST_K2HFTFUSESVR_LOG} 2>&1 &
	K2HFTFUSESVRPID=$!
	echo "server process pid = ${K2HFTFUSESVRPID}"
	sleep 2

	#
	# chmpx for slave
	#
	echo "------ RUN chmpx on slave side ----------------------------"
	chmpx -conf ${INIFILEDIR}/k2hftfuse_test_slave${CONF_FILE_EXT} -d ${DEBUGMODE} > ${NEST_CHMPX_SLAVE_LOG} 2>&1 &
	CHMPXSLVPID=$!
	echo "chmpx on slave side pid = ${CHMPXSLVPID}"
	sleep 2

	#
	# fuse client ( without allow_other option )
	#
	echo "------ RUN fuse client ------------------------------------"
	K2HDBGMODE=INFO K2HDBGFILE=${NEST_K2HFTFUSE_K2HASH_LOG} CHMDBGMODE=MSG CHMDBGFILE=${NEST_K2HFTFUSE_CHMPX_LOG} ${K2HFUSEBIN} ${MOUNTDIR} -o dbglevel=msg,conf=${INIFILEDIR}/k2hftfuse_test_slave${CONF_FILE_EXT},chmpxlog=${NEST_K2HFTFUSE_CHMPX_LOG} -f > ${NEST_K2HFTFUSE_LOG} 2>&1 &
	K2HFTFUSEPID=$!
	echo "k2hftfuse pid = ${K2HFTFUSEPID}"
	sleep 10

	#
	# check process
	#
	RESULT_RUN_K2HFTFUSE_PROC=true
	ps -p ${K2HFTFUSEPID} > /dev/null 2>&1
	if [ $? != 0 ]; then
		echo "ERROR: could not run k2hftfuse process"
		RESULT_RUN_K2HFTFUSE_PROC=false
	fi

	#
	# test( we need to run this if failed to run k2hftfuse for gcov )
	#
	echo "------ RUN test program(k2hftfusetest) --------------------"
	${K2HFUSETESTBIN} ${MOUNTDIR}/log/nothing2.log 10 MY_TEST_PROGRAM_OUTPUT > ${NEST_K2HFTFUSETEST_LOG} 2>&1
	TESTSUBRESULT=$?
	echo "k2hftfusetest finished."
	sleep 2

	#
	# exit all process
	#
	kill -HUP ${K2HFTFUSEPID} > /dev/null 2>&1
	sleep 1
	kill -HUP ${K2HFTFUSESVRPID} > /dev/null 2>&1
	sleep 1
	kill -HUP ${K2HFTFUSESVRTRANSPID} > /dev/null 2>&1
	sleep 1
	kill -HUP ${CHMPXSLVPID} > /dev/null 2>&1
	sleep 1
	kill -HUP ${CHMPXSVRPID} > /dev/null 2>&1
	sleep 1
	kill -HUP ${CHMPXTRANSSLVPID} > /dev/null 2>&1
	sleep 1
	kill -HUP ${CHMPXTRANSSVRPID} > /dev/null 2>&1
	sleep 1
	kill -9 ${K2HFTFUSEPID} ${K2HFTFUSESVRPID} ${K2HFTFUSESVRTRANSPID} ${CHMPXSLVPID} ${CHMPXSVRPID} ${CHMPXTRANSSLVPID} ${CHMPXTRANSSVRPID} > /dev/null 2>&1

	#
	# check line count
	#
	LINES=`grep 'log/nothing2.log' unify.log 2> /dev/null | wc -l 2> /dev/null | awk '{print $1}' 2> /dev/null`
	if [ "X$LINES" != "X10" ]; then
		echo "ERROR: unify.log file line count(${LINES}) is not as same as 10."
		TESTSUBRESULT=1
	fi

	#
	# result for single test
	#
	if [ ${TESTSUBRESULT} -ne 0 ]; then
		echo ""
		echo "### FAILED ON NEST TEST ###########################################################################"
		echo ""
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${NEST_TRANS_CHMPX_SERVER_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${NEST_TRANS_CHMPX_SERVER_LOG}
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${NEST_CHMPX_SERVER_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${NEST_CHMPX_SERVER_LOG}
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${NEST_TRANS_CHMPX_SLAVE_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${NEST_TRANS_CHMPX_SLAVE_LOG}
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${NEST_CHMPX_SLAVE_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${NEST_CHMPX_SLAVE_LOG}
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${NEST_TRANS_K2HFTFUSESVR_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${NEST_TRANS_K2HFTFUSESVR_LOG}
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${NEST_K2HFTFUSESVR_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${NEST_K2HFTFUSESVR_LOG}
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${NEST_K2HFTFUSE_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${NEST_K2HFTFUSE_LOG}
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${NEST_K2HFTFUSE_CHMPX_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${NEST_K2HFTFUSE_CHMPX_LOG}
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${NEST_K2HFTFUSE_K2HASH_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${NEST_K2HFTFUSE_K2HASH_LOG}
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${NEST_K2HFTFUSETEST_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${NEST_K2HFTFUSETEST_LOG}
		echo "---------------------------------------------------------------------------------------------------"

		if [ "X${RESULT_RUN_K2HFTFUSE_PROC}" = "Xtrue" ]; then
			echo ""
			echo "RESULT --> FAILED(ON NEST TEST)"
			echo ""
			exit 1
		fi

		if [ ${IS_SAFE_TEST} -eq 0 ]; then
			echo ""
			echo "RESULT --> FAILED(ON NEST TEST)"
			echo ""
			exit 1
		fi

		echo ""
		echo "***************************************************************************************************"
		echo "This test is run on CI(docker) and we could not run k2hftfuse process."
		echo "Maybe, you need to set \"--cap-add SYS_ADMIN --device /dev/fuse --security-opt apparmor:unconfined\""
		echo "to your docker."
		echo "And you can find \"fuse: failed to open /dev/fuse: Operation not permitted\" in logs, when this failure"
		echo "reason is permission on docker."
		echo ""
		echo "***** THEN WE SKIP THIS TEST, MUST CHECK TEST ON YOUR LOCAL ENVIRONMENT *****"
		echo ""
		echo "***************************************************************************************************"
		echo ""
	else
		#
		# cleaning
		#
		rm -f /tmp/mnt/k2hftfusesvr/log/unify.log
		if [ $? -ne 0 ]; then
			echo "RESULT --> FAILED(COULD NOT REMOVE RESULT FILE)"
			exit 1
		fi
	fi
	force_stop_all_processes ${MOUNTDIR} > /dev/null 2>&1

	#
	# clean old files in /tmp
	#
	rm -rf /tmp/mnt/k2hftfuse
	rm -rf /tmp/mnt/k2hftfusesvr
	rm -rf /tmp/test/K2HFUSESVRTEST-*.chmpx
	rm -rf /tmp/test/K2HFUSESVRTEST-*.k2h
	rm -rf /tmp/test/K2HFUSETEST-*.chmpx
	rm -rf /tmp/test/K2HFUSETEST-*.k2h
	rm -rf /tmp/k2hftfuse_np_*
	rm -rf /tmp/test/nest_*.log
	rm -rf /tmp/test/single_*.log

	#######################
	# Finish
	#######################
	echo ""
	echo "YAML CONF TEST RESULT --> SUCCEED"
	echo ""
fi

##############################################################
# Start json conf file
##############################################################
if [ "X${DO_JSON_CONF}" = "Xyes" ]; then
	echo ""
	echo ""
	echo "====== START TEST FOR JSON CONF FILE ======================="
	echo ""
	CONF_FILE_EXT=".json"

	#######################
	# TEST for SINGLE SERVER
	#######################
	#
	# chmpx for server
	#
	echo "------ RUN chmpx on server side ----------------------------"
	chmpx -conf ${INIFILEDIR}/k2hftfuse_test_server${CONF_FILE_EXT} -d ${DEBUGMODE} > ${SINGLE_CHMPX_SERVER_LOG} 2>&1 &
	CHMPXSVRPID=$!
	echo "chmpx on server side pid = ${CHMPXSVRPID}"
	sleep 2

	#
	# k2hftfusesvr server process
	#
	echo "------ RUN server process on server side ------------------"
	${K2HFUSESVRBIN} -conf ${INIFILEDIR}/k2hftfuse_test_server${CONF_FILE_EXT} -d ${DEBUGMODE} > ${SINGLE_K2HFTFUSESVR_LOG} 2>&1 &
	K2HFTFUSESVRPID=$!
	echo "server process pid = ${K2HFTFUSESVRPID}"
	sleep 2

	#
	# chmpx for slave
	#
	echo "------ RUN chmpx on slave side ----------------------------"
	chmpx -conf ${INIFILEDIR}/k2hftfuse_test_slave${CONF_FILE_EXT} -d ${DEBUGMODE} > ${SINGLE_CHMPX_SLAVE_LOG} 2>&1 &
	CHMPXSLVPID=$!
	echo "chmpx on slave side pid = ${CHMPXSLVPID}"
	sleep 2

	#
	# fuse client( without allow_other option)
	#
	echo "------ RUN fuse client ------------------------------------"
	K2HDBGMODE=INFO K2HDBGFILE=${SINGLE_K2HFTFUSE_K2HASH_LOG} CHMDBGMODE=MSG CHMDBGFILE=${SINGLE_K2HFTFUSE_CHMPX_LOG} ${K2HFUSEBIN} ${MOUNTDIR} -o dbglevel=msg,conf=${INIFILEDIR}/k2hftfuse_test_slave${CONF_FILE_EXT},chmpxlog=${SINGLE_K2HFTFUSE_CHMPX_LOG} -f > ${SINGLE_K2HFTFUSE_LOG} 2>&1 &
	K2HFTFUSEPID=$!
	echo "k2hftfuse pid = ${K2HFTFUSEPID}"
	sleep 10

	#
	# check process
	#
	RESULT_RUN_K2HFTFUSE_PROC=true
	ps -p ${K2HFTFUSEPID} > /dev/null 2>&1
	if [ $? != 0 ]; then
		echo "ERROR: could not run k2hftfuse process"
		RESULT_RUN_K2HFTFUSE_PROC=false
	fi

	#
	# test( we need to run this if failed to run k2hftfuse for gcov )
	#
	echo "------ RUN test program(k2hftfusetest) --------------------"
	${K2HFUSETESTBIN} ${MOUNTDIR}/log/nothing2.log 10 MY_TEST_PROGRAM_OUTPUT > ${SINGLE_K2HFTFUSETEST_LOG} 2>&1
	TESTSUBRESULT=$?
	echo "k2hftfusetest finished."
	sleep 2

	#
	# exit all process
	#
	kill -HUP ${K2HFTFUSEPID} > /dev/null 2>&1
	sleep 1
	kill -HUP ${K2HFTFUSESVRPID} > /dev/null 2>&1
	sleep 1
	kill -HUP ${CHMPXSLVPID} > /dev/null 2>&1
	sleep 1
	kill -HUP ${CHMPXSVRPID} > /dev/null 2>&1
	sleep 1
	kill -9 ${K2HFTFUSEPID} ${K2HFTFUSESVRPID} ${CHMPXSLVPID} ${CHMPXSVRPID} > /dev/null 2>&1

	#
	# check line count
	#
	LINES=`grep 'log/nothing2.log' unify.log 2> /dev/null | wc -l 2> /dev/null | awk '{print $1}' 2> /dev/null`
	if [ "X$LINES" != "X10" ]; then
		echo "ERROR: unify.log file line count(${LINES}) is not as same as 10."
		TESTSUBRESULT=1
	fi

	#
	# result for single test
	#
	if [ ${TESTSUBRESULT} -ne 0 ]; then
		echo ""
		echo "### FAILED ON SINGLE TEST #########################################################################"
		echo ""
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${SINGLE_CHMPX_SERVER_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${SINGLE_CHMPX_SERVER_LOG}
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${SINGLE_CHMPX_SLAVE_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${SINGLE_CHMPX_SLAVE_LOG}
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${SINGLE_K2HFTFUSESVR_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${SINGLE_K2HFTFUSESVR_LOG}
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${SINGLE_K2HFTFUSE_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${SINGLE_K2HFTFUSE_LOG}
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${SINGLE_K2HFTFUSE_CHMPX_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${SINGLE_K2HFTFUSE_CHMPX_LOG}
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${SINGLE_K2HFTFUSE_K2HASH_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${SINGLE_K2HFTFUSE_K2HASH_LOG}
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${SINGLE_K2HFTFUSETEST_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${SINGLE_K2HFTFUSETEST_LOG}
		echo "---------------------------------------------------------------------------------------------------"

		if [ "X${RESULT_RUN_K2HFTFUSE_PROC}" = "Xtrue" ]; then
			echo ""
			echo "RESULT --> FAILED(ON SINGLE TEST)"
			echo ""
			exit 1
		fi

		if [ ${IS_SAFE_TEST} -eq 0 ]; then
			echo ""
			echo "RESULT --> FAILED(ON SINGLE TEST)"
			echo ""
			exit 1
		fi

		echo ""
		echo "***************************************************************************************************"
		echo "This test is run on CI(docker) and we could not run k2hftfuse process."
		echo "Maybe, you need to set \"--cap-add SYS_ADMIN --device /dev/fuse --security-opt apparmor:unconfined\""
		echo "to your docker."
		echo "And you can find \"fuse: failed to open /dev/fuse: Operation not permitted\" in logs, when this failure"
		echo "reason is permission on docker."
		echo ""
		echo "***** THEN WE SKIP THIS TEST, MUST CHECK TEST ON YOUR LOCAL ENVIRONMENT *****"
		echo ""
		echo "***************************************************************************************************"
		echo ""
	else
		#
		# cleaning
		#
		rm -f /tmp/mnt/k2hftfusesvr/log/unify.log
		if [ $? -ne 0 ]; then
			echo "RESULT --> FAILED(COULD NOT REMOVE RESULT FILE)"
			exit 1
		fi
	fi
	force_stop_all_processes ${MOUNTDIR} > /dev/null 2>&1

	#
	# clean old files in /tmp
	#
	rm -rf /tmp/mnt/k2hftfuse
	rm -rf /tmp/mnt/k2hftfusesvr
	rm -rf /tmp/test/K2HFUSESVRTEST-*.chmpx
	rm -rf /tmp/test/K2HFUSESVRTEST-*.k2h
	rm -rf /tmp/test/K2HFUSETEST-*.chmpx
	rm -rf /tmp/test/K2HFUSETEST-*.k2h
	rm -rf /tmp/k2hftfuse_np_*
	rm -rf /tmp/test/nest_*.log
	rm -rf /tmp/test/single_*.log

	#######################
	# TEST for NEST SERVER
	#######################
	#
	# chmpx for transferred server
	#
	echo "------ RUN transferred chmpx on server side ----------------"
	chmpx -conf ${INIFILEDIR}/k2hftfusesvr_test_trans_server${CONF_FILE_EXT} -d ${DEBUGMODE} > ${NEST_TRANS_CHMPX_SERVER_LOG} 2>&1 &
	CHMPXTRANSSVRPID=$!
	echo "transferred chmpx on server side pid = ${CHMPXTRANSSVRPID}"
	sleep 2

	#
	# k2hftfusesvr transferred server process
	#
	echo "------ RUN transferred server process on server side -------"
	${K2HFUSESVRBIN} -conf ${INIFILEDIR}/k2hftfusesvr_test_trans_server${CONF_FILE_EXT} -d ${DEBUGMODE} > ${NEST_TRANS_K2HFTFUSESVR_LOG} 2>&1 &
	K2HFTFUSESVRTRANSPID=$!
	echo "transferred server process pid = ${K2HFTFUSESVRTRANSPID}"
	sleep 2

	#
	# transferred chmpx for slave
	#
	echo "------ RUN transferred chmpx on slave side -----------------"
	chmpx -conf ${INIFILEDIR}/k2hftfusesvr_test_trans_slave${CONF_FILE_EXT} -d ${DEBUGMODE} > ${NEST_TRANS_CHMPX_SLAVE_LOG} 2>&1 &
	CHMPXTRANSSLVPID=$!
	echo "transferred chmpx on slave side pid = ${CHMPXTRANSSLVPID}"
	sleep 2

	#
	# chmpx for server
	#
	echo "------ RUN chmpx on server side ----------------------------"
	chmpx -conf ${INIFILEDIR}/k2hftfuse_test_trans_server${CONF_FILE_EXT} -d ${DEBUGMODE} > ${NEST_CHMPX_SERVER_LOG} 2>&1 &
	CHMPXSVRPID=$!
	echo "chmpx on server side pid = ${CHMPXSVRPID}"
	sleep 2

	#
	# k2hftfusesvr server process
	#
	echo "------ RUN server process on server side ------------------"
	${K2HFUSESVRBIN} -conf ${INIFILEDIR}/k2hftfuse_test_trans_server${CONF_FILE_EXT} -d ${DEBUGMODE} > ${NEST_K2HFTFUSESVR_LOG} 2>&1 &
	K2HFTFUSESVRPID=$!
	echo "server process pid = ${K2HFTFUSESVRPID}"
	sleep 2

	#
	# chmpx for slave
	#
	echo "------ RUN chmpx on slave side ----------------------------"
	chmpx -conf ${INIFILEDIR}/k2hftfuse_test_slave${CONF_FILE_EXT} -d ${DEBUGMODE} > ${NEST_CHMPX_SLAVE_LOG} 2>&1 &
	CHMPXSLVPID=$!
	echo "chmpx on slave side pid = ${CHMPXSLVPID}"
	sleep 2

	#
	# fuse client ( without allow_other option )
	#
	echo "------ RUN fuse client ------------------------------------"
	K2HDBGMODE=INFO K2HDBGFILE=${NEST_K2HFTFUSE_K2HASH_LOG} CHMDBGMODE=MSG CHMDBGFILE=${NEST_K2HFTFUSE_CHMPX_LOG} ${K2HFUSEBIN} ${MOUNTDIR} -o dbglevel=msg,conf=${INIFILEDIR}/k2hftfuse_test_slave${CONF_FILE_EXT},chmpxlog=${NEST_K2HFTFUSE_CHMPX_LOG} -f > ${NEST_K2HFTFUSE_LOG} 2>&1 &
	K2HFTFUSEPID=$!
	echo "k2hftfuse pid = ${K2HFTFUSEPID}"
	sleep 10

	#
	# check process
	#
	RESULT_RUN_K2HFTFUSE_PROC=true
	ps -p ${K2HFTFUSEPID} > /dev/null 2>&1
	if [ $? != 0 ]; then
		echo "ERROR: could not run k2hftfuse process"
		RESULT_RUN_K2HFTFUSE_PROC=false
	fi

	#
	# test( we need to run this if failed to run k2hftfuse for gcov )
	#
	echo "------ RUN test program(k2hftfusetest) --------------------"
	${K2HFUSETESTBIN} ${MOUNTDIR}/log/nothing2.log 10 MY_TEST_PROGRAM_OUTPUT > ${NEST_K2HFTFUSETEST_LOG} 2>&1
	TESTSUBRESULT=$?
	echo "k2hftfusetest finished."
	sleep 2

	#
	# exit all process
	#
	kill -HUP ${K2HFTFUSEPID} > /dev/null 2>&1
	sleep 1
	kill -HUP ${K2HFTFUSESVRPID} > /dev/null 2>&1
	sleep 1
	kill -HUP ${K2HFTFUSESVRTRANSPID} > /dev/null 2>&1
	sleep 1
	kill -HUP ${CHMPXSLVPID} > /dev/null 2>&1
	sleep 1
	kill -HUP ${CHMPXSVRPID} > /dev/null 2>&1
	sleep 1
	kill -HUP ${CHMPXTRANSSLVPID} > /dev/null 2>&1
	sleep 1
	kill -HUP ${CHMPXTRANSSVRPID} > /dev/null 2>&1
	sleep 1
	kill -9 ${K2HFTFUSEPID} ${K2HFTFUSESVRPID} ${K2HFTFUSESVRTRANSPID} ${CHMPXSLVPID} ${CHMPXSVRPID} ${CHMPXTRANSSLVPID} ${CHMPXTRANSSVRPID} > /dev/null 2>&1

	#
	# check line count
	#
	LINES=`grep 'log/nothing2.log' unify.log 2> /dev/null | wc -l 2> /dev/null | awk '{print $1}' 2> /dev/null`
	if [ "X$LINES" != "X10" ]; then
		echo "ERROR: unify.log file line count(${LINES}) is not as same as 10."
		TESTSUBRESULT=1
	fi

	#
	# result for single test
	#
	if [ ${TESTSUBRESULT} -ne 0 ]; then
		echo ""
		echo "### FAILED ON NEST TEST ###########################################################################"
		echo ""
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${NEST_TRANS_CHMPX_SERVER_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${NEST_TRANS_CHMPX_SERVER_LOG}
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${NEST_CHMPX_SERVER_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${NEST_CHMPX_SERVER_LOG}
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${NEST_TRANS_CHMPX_SLAVE_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${NEST_TRANS_CHMPX_SLAVE_LOG}
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${NEST_CHMPX_SLAVE_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${NEST_CHMPX_SLAVE_LOG}
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${NEST_TRANS_K2HFTFUSESVR_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${NEST_TRANS_K2HFTFUSESVR_LOG}
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${NEST_K2HFTFUSESVR_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${NEST_K2HFTFUSESVR_LOG}
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${NEST_K2HFTFUSE_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${NEST_K2HFTFUSE_LOG}
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${NEST_K2HFTFUSE_CHMPX_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${NEST_K2HFTFUSE_CHMPX_LOG}
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${NEST_K2HFTFUSE_K2HASH_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${NEST_K2HFTFUSE_K2HASH_LOG}
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${NEST_K2HFTFUSETEST_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${NEST_K2HFTFUSETEST_LOG}
		echo "---------------------------------------------------------------------------------------------------"

		if [ "X${RESULT_RUN_K2HFTFUSE_PROC}" = "Xtrue" ]; then
			echo ""
			echo "RESULT --> FAILED(ON NEST TEST)"
			echo ""
			exit 1
		fi

		if [ ${IS_SAFE_TEST} -eq 0 ]; then
			echo ""
			echo "RESULT --> FAILED(ON NEST TEST)"
			echo ""
			exit 1
		fi

		echo ""
		echo "***************************************************************************************************"
		echo "This test is run on CI(docker) and we could not run k2hftfuse process."
		echo "Maybe, you need to set \"--cap-add SYS_ADMIN --device /dev/fuse --security-opt apparmor:unconfined\""
		echo "to your docker."
		echo "And you can find \"fuse: failed to open /dev/fuse: Operation not permitted\" in logs, when this failure"
		echo "reason is permission on docker."
		echo ""
		echo "***** THEN WE SKIP THIS TEST, MUST CHECK TEST ON YOUR LOCAL ENVIRONMENT *****"
		echo ""
		echo "***************************************************************************************************"
		echo ""
	else
		#
		# cleaning
		#
		rm -f /tmp/mnt/k2hftfusesvr/log/unify.log
		if [ $? -ne 0 ]; then
			echo "RESULT --> FAILED(COULD NOT REMOVE RESULT FILE)"
			exit 1
		fi
	fi
	force_stop_all_processes ${MOUNTDIR} > /dev/null 2>&1

	#
	# clean old files in /tmp
	#
	rm -rf /tmp/mnt/k2hftfuse
	rm -rf /tmp/mnt/k2hftfusesvr
	rm -rf /tmp/test/K2HFUSESVRTEST-*.chmpx
	rm -rf /tmp/test/K2HFUSESVRTEST-*.k2h
	rm -rf /tmp/test/K2HFUSETEST-*.chmpx
	rm -rf /tmp/test/K2HFUSETEST-*.k2h
	rm -rf /tmp/k2hftfuse_np_*
	rm -rf /tmp/test/nest_*.log
	rm -rf /tmp/test/single_*.log

	#######################
	# Finish
	#######################
	echo ""
	echo "JSON CONF TEST RESULT --> SUCCEED"
	echo ""
fi

##############################################################
# Start json string conf
##############################################################
if [ "X${DO_JSON_STRING}" = "Xyes" ]; then
	echo ""
	echo ""
	echo "====== START TEST FOR JSON STRING =========================="
	echo ""

	#######################
	# TEST for SINGLE SERVER
	#######################
	#
	# chmpx for server
	#
	echo "------ RUN chmpx on server side ----------------------------"
	chmpx -json "${K2HFTFUSE_TEST_SERVER_JSON}" -d ${DEBUGMODE} > ${SINGLE_CHMPX_SERVER_LOG} 2>&1 &
	CHMPXSVRPID=$!
	echo "chmpx on server side pid = ${CHMPXSVRPID}"
	sleep 2

	#
	# k2hftfusesvr server process
	#
	echo "------ RUN server process on server side ------------------"
	${K2HFUSESVRBIN} -json "${K2HFTFUSE_TEST_SERVER_JSON}" -d ${DEBUGMODE} > ${SINGLE_K2HFTFUSESVR_LOG} 2>&1 &
	K2HFTFUSESVRPID=$!
	echo "server process pid = ${K2HFTFUSESVRPID}"
	sleep 2

	#
	# chmpx for slave
	#
	echo "------ RUN chmpx on slave side ----------------------------"
	chmpx -json "${K2HFTFUSE_TEST_SLAVE_JSON}" -d ${DEBUGMODE} > ${SINGLE_CHMPX_SLAVE_LOG} 2>&1 &
	CHMPXSLVPID=$!
	echo "chmpx on slave side pid = ${CHMPXSLVPID}"
	sleep 2

	#
	# fuse client( without allow_other option)
	#
	echo "------ RUN fuse client ------------------------------------"
	K2HDBGMODE=INFO K2HDBGFILE=${SINGLE_K2HFTFUSE_K2HASH_LOG} CHMDBGMODE=MSG CHMDBGFILE=${SINGLE_K2HFTFUSE_CHMPX_LOG} ${K2HFUSEBIN} ${MOUNTDIR} -o dbglevel=msg,json="${K2HFTFUSE_TEST_SLAVE_JSON_FUSE}",chmpxlog=${SINGLE_K2HFTFUSE_CHMPX_LOG} -f > ${SINGLE_K2HFTFUSE_LOG} 2>&1 &
	K2HFTFUSEPID=$!
	echo "k2hftfuse pid = ${K2HFTFUSEPID}"
	sleep 10

	#
	# check process
	#
	RESULT_RUN_K2HFTFUSE_PROC=true
	ps -p ${K2HFTFUSEPID} > /dev/null 2>&1
	if [ $? != 0 ]; then
		echo "ERROR: could not run k2hftfuse process"
		RESULT_RUN_K2HFTFUSE_PROC=false
	fi

	#
	# test( we need to run this if failed to run k2hftfuse for gcov )
	#
	echo "------ RUN test program(k2hftfusetest) --------------------"
	${K2HFUSETESTBIN} ${MOUNTDIR}/log/nothing2.log 10 MY_TEST_PROGRAM_OUTPUT > ${SINGLE_K2HFTFUSETEST_LOG} 2>&1
	TESTSUBRESULT=$?
	echo "k2hftfusetest finished."
	sleep 2

	#
	# exit all process
	#
	kill -HUP ${K2HFTFUSEPID} > /dev/null 2>&1
	sleep 1
	kill -HUP ${K2HFTFUSESVRPID} > /dev/null 2>&1
	sleep 1
	kill -HUP ${CHMPXSLVPID} > /dev/null 2>&1
	sleep 1
	kill -HUP ${CHMPXSVRPID} > /dev/null 2>&1
	sleep 1
	kill -9 ${K2HFTFUSEPID} ${K2HFTFUSESVRPID} ${CHMPXSLVPID} ${CHMPXSVRPID} > /dev/null 2>&1

	#
	# check line count
	#
	LINES=`grep 'log/nothing2.log' unify.log 2> /dev/null | wc -l 2> /dev/null | awk '{print $1}' 2> /dev/null`
	if [ "X$LINES" != "X10" ]; then
		echo "ERROR: unify.log file line count(${LINES}) is not as same as 10."
		TESTSUBRESULT=1
	fi

	#
	# result for single test
	#
	if [ ${TESTSUBRESULT} -ne 0 ]; then
		echo ""
		echo "### FAILED ON SINGLE TEST #########################################################################"
		echo ""
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${SINGLE_CHMPX_SERVER_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${SINGLE_CHMPX_SERVER_LOG}
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${SINGLE_CHMPX_SLAVE_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${SINGLE_CHMPX_SLAVE_LOG}
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${SINGLE_K2HFTFUSESVR_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${SINGLE_K2HFTFUSESVR_LOG}
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${SINGLE_K2HFTFUSE_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${SINGLE_K2HFTFUSE_LOG}
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${SINGLE_K2HFTFUSE_CHMPX_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${SINGLE_K2HFTFUSE_CHMPX_LOG}
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${SINGLE_K2HFTFUSE_K2HASH_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${SINGLE_K2HFTFUSE_K2HASH_LOG}
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${SINGLE_K2HFTFUSETEST_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${SINGLE_K2HFTFUSETEST_LOG}
		echo "---------------------------------------------------------------------------------------------------"

		if [ "X${RESULT_RUN_K2HFTFUSE_PROC}" = "Xtrue" ]; then
			echo ""
			echo "RESULT --> FAILED(ON SINGLE TEST)"
			echo ""
			exit 1
		fi

		if [ ${IS_SAFE_TEST} -eq 0 ]; then
			echo ""
			echo "RESULT --> FAILED(ON SINGLE TEST)"
			echo ""
			exit 1
		fi

		echo ""
		echo "***************************************************************************************************"
		echo "This test is run on CI(docker) and we could not run k2hftfuse process."
		echo "Maybe, you need to set \"--cap-add SYS_ADMIN --device /dev/fuse --security-opt apparmor:unconfined\""
		echo "to your docker."
		echo "And you can find \"fuse: failed to open /dev/fuse: Operation not permitted\" in logs, when this failure"
		echo "reason is permission on docker."
		echo ""
		echo "***** THEN WE SKIP THIS TEST, MUST CHECK TEST ON YOUR LOCAL ENVIRONMENT *****"
		echo ""
		echo "***************************************************************************************************"
		echo ""
	else
		#
		# cleaning
		#
		rm -f /tmp/mnt/k2hftfusesvr/log/unify.log
		if [ $? -ne 0 ]; then
			echo "RESULT --> FAILED(COULD NOT REMOVE RESULT FILE)"
			exit 1
		fi
	fi
	force_stop_all_processes ${MOUNTDIR} > /dev/null 2>&1

	#
	# clean old files in /tmp
	#
	rm -rf /tmp/mnt/k2hftfuse
	rm -rf /tmp/mnt/k2hftfusesvr
	rm -rf /tmp/test/K2HFUSESVRTEST-*.chmpx
	rm -rf /tmp/test/K2HFUSESVRTEST-*.k2h
	rm -rf /tmp/test/K2HFUSETEST-*.chmpx
	rm -rf /tmp/test/K2HFUSETEST-*.k2h
	rm -rf /tmp/k2hftfuse_np_*
	rm -rf /tmp/test/nest_*.log
	rm -rf /tmp/test/single_*.log

	#######################
	# TEST for NEST SERVER
	#######################
	#
	# chmpx for transferred server
	#
	echo "------ RUN transferred chmpx on server side ----------------"
	chmpx -json "${K2HFTFUSESVR_TEST_TRANS_SERVER_JSON}" -d ${DEBUGMODE} > ${NEST_TRANS_CHMPX_SERVER_LOG} 2>&1 &
	CHMPXTRANSSVRPID=$!
	echo "transferred chmpx on server side pid = ${CHMPXTRANSSVRPID}"
	sleep 2

	#
	# k2hftfusesvr transferred server process
	#
	echo "------ RUN transferred server process on server side -------"
	${K2HFUSESVRBIN} -json "${K2HFTFUSESVR_TEST_TRANS_SERVER_JSON}" -d ${DEBUGMODE} > ${NEST_TRANS_K2HFTFUSESVR_LOG} 2>&1 &
	K2HFTFUSESVRTRANSPID=$!
	echo "transferred server process pid = ${K2HFTFUSESVRTRANSPID}"
	sleep 2

	#
	# transferred chmpx for slave
	#
	echo "------ RUN transferred chmpx on slave side -----------------"
	chmpx -json "${K2HFTFUSESVR_TEST_TRANS_SLAVE_JSON}" -d ${DEBUGMODE} > ${NEST_TRANS_CHMPX_SLAVE_LOG} 2>&1 &
	CHMPXTRANSSLVPID=$!
	echo "transferred chmpx on slave side pid = ${CHMPXTRANSSLVPID}"
	sleep 2

	#
	# chmpx for server
	#
	echo "------ RUN chmpx on server side ----------------------------"
	chmpx -json "${K2HFTFUSE_TEST_TRANS_SERVER_JSON}" -d ${DEBUGMODE} > ${NEST_CHMPX_SERVER_LOG} 2>&1 &
	CHMPXSVRPID=$!
	echo "chmpx on server side pid = ${CHMPXSVRPID}"
	sleep 2

	#
	# k2hftfusesvr server process
	#
	echo "------ RUN server process on server side ------------------"
	${K2HFUSESVRBIN} -json "${K2HFTFUSE_TEST_TRANS_SERVER_JSON}" -d ${DEBUGMODE} > ${NEST_K2HFTFUSESVR_LOG} 2>&1 &
	K2HFTFUSESVRPID=$!
	echo "server process pid = ${K2HFTFUSESVRPID}"
	sleep 2

	#
	# chmpx for slave
	#
	echo "------ RUN chmpx on slave side ----------------------------"
	chmpx -json "${K2HFTFUSE_TEST_SLAVE_JSON}" -d ${DEBUGMODE} > ${NEST_CHMPX_SLAVE_LOG} 2>&1 &
	CHMPXSLVPID=$!
	echo "chmpx on slave side pid = ${CHMPXSLVPID}"
	sleep 2

	#
	# fuse client ( without allow_other option )
	#
	echo "------ RUN fuse client ------------------------------------"
	K2HDBGMODE=INFO K2HDBGFILE=${NEST_K2HFTFUSE_K2HASH_LOG} CHMDBGMODE=MSG CHMDBGFILE=${NEST_K2HFTFUSE_CHMPX_LOG} ${K2HFUSEBIN} ${MOUNTDIR} -o dbglevel=msg,json="${K2HFTFUSE_TEST_SLAVE_JSON_FUSE}",chmpxlog=${NEST_K2HFTFUSE_CHMPX_LOG} -f > ${NEST_K2HFTFUSE_LOG} 2>&1 &
	K2HFTFUSEPID=$!
	echo "k2hftfuse pid = ${K2HFTFUSEPID}"
	sleep 10

	#
	# check process
	#
	RESULT_RUN_K2HFTFUSE_PROC=true
	ps -p ${K2HFTFUSEPID} > /dev/null 2>&1
	if [ $? != 0 ]; then
		echo "ERROR: could not run k2hftfuse process"
		RESULT_RUN_K2HFTFUSE_PROC=false
	fi

	#
	# test( we need to run this if failed to run k2hftfuse for gcov )
	#
	echo "------ RUN test program(k2hftfusetest) --------------------"
	${K2HFUSETESTBIN} ${MOUNTDIR}/log/nothing2.log 10 MY_TEST_PROGRAM_OUTPUT > ${NEST_K2HFTFUSETEST_LOG} 2>&1
	TESTSUBRESULT=$?
	echo "k2hftfusetest finished."
	sleep 2

	#
	# exit all process
	#
	kill -HUP ${K2HFTFUSEPID} > /dev/null 2>&1
	sleep 1
	kill -HUP ${K2HFTFUSESVRPID} > /dev/null 2>&1
	sleep 1
	kill -HUP ${K2HFTFUSESVRTRANSPID} > /dev/null 2>&1
	sleep 1
	kill -HUP ${CHMPXSLVPID} > /dev/null 2>&1
	sleep 1
	kill -HUP ${CHMPXSVRPID} > /dev/null 2>&1
	sleep 1
	kill -HUP ${CHMPXTRANSSLVPID} > /dev/null 2>&1
	sleep 1
	kill -HUP ${CHMPXTRANSSVRPID} > /dev/null 2>&1
	sleep 1
	kill -9 ${K2HFTFUSEPID} ${K2HFTFUSESVRPID} ${K2HFTFUSESVRTRANSPID} ${CHMPXSLVPID} ${CHMPXSVRPID} ${CHMPXTRANSSLVPID} ${CHMPXTRANSSVRPID} > /dev/null 2>&1

	#
	# check line count
	#
	LINES=`grep 'log/nothing2.log' unify.log 2> /dev/null | wc -l 2> /dev/null | awk '{print $1}' 2> /dev/null`
	if [ "X$LINES" != "X10" ]; then
		echo "ERROR: unify.log file line count(${LINES}) is not as same as 10."
		TESTSUBRESULT=1
	fi

	#
	# result for single test
	#
	if [ ${TESTSUBRESULT} -ne 0 ]; then
		echo ""
		echo "### FAILED ON NEST TEST ###########################################################################"
		echo ""
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${NEST_TRANS_CHMPX_SERVER_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${NEST_TRANS_CHMPX_SERVER_LOG}
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${NEST_CHMPX_SERVER_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${NEST_CHMPX_SERVER_LOG}
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${NEST_TRANS_CHMPX_SLAVE_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${NEST_TRANS_CHMPX_SLAVE_LOG}
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${NEST_CHMPX_SLAVE_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${NEST_CHMPX_SLAVE_LOG}
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${NEST_TRANS_K2HFTFUSESVR_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${NEST_TRANS_K2HFTFUSESVR_LOG}
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${NEST_K2HFTFUSESVR_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${NEST_K2HFTFUSESVR_LOG}
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${NEST_K2HFTFUSE_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${NEST_K2HFTFUSE_LOG}
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${NEST_K2HFTFUSE_CHMPX_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${NEST_K2HFTFUSE_CHMPX_LOG}
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${NEST_K2HFTFUSE_K2HASH_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${NEST_K2HFTFUSE_K2HASH_LOG}
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${NEST_K2HFTFUSETEST_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${NEST_K2HFTFUSETEST_LOG}
		echo "---------------------------------------------------------------------------------------------------"

		if [ "X${RESULT_RUN_K2HFTFUSE_PROC}" = "Xtrue" ]; then
			echo ""
			echo "RESULT --> FAILED(ON NEST TEST)"
			echo ""
			exit 1
		fi

		if [ ${IS_SAFE_TEST} -eq 0 ]; then
			echo ""
			echo "RESULT --> FAILED(ON NEST TEST)"
			echo ""
			exit 1
		fi

		echo ""
		echo "***************************************************************************************************"
		echo "This test is run on CI(docker) and we could not run k2hftfuse process."
		echo "Maybe, you need to set \"--cap-add SYS_ADMIN --device /dev/fuse --security-opt apparmor:unconfined\""
		echo "to your docker."
		echo "And you can find \"fuse: failed to open /dev/fuse: Operation not permitted\" in logs, when this failure"
		echo "reason is permission on docker."
		echo ""
		echo "***** THEN WE SKIP THIS TEST, MUST CHECK TEST ON YOUR LOCAL ENVIRONMENT *****"
		echo ""
		echo "***************************************************************************************************"
		echo ""
	else
		#
		# cleaning
		#
		rm -f /tmp/mnt/k2hftfusesvr/log/unify.log
		if [ $? -ne 0 ]; then
			echo "RESULT --> FAILED(COULD NOT REMOVE RESULT FILE)"
			exit 1
		fi
	fi
	force_stop_all_processes ${MOUNTDIR} > /dev/null 2>&1

	#
	# clean old files in /tmp
	#
	rm -rf /tmp/mnt/k2hftfuse
	rm -rf /tmp/mnt/k2hftfusesvr
	rm -rf /tmp/test/K2HFUSESVRTEST-*.chmpx
	rm -rf /tmp/test/K2HFUSESVRTEST-*.k2h
	rm -rf /tmp/test/K2HFUSETEST-*.chmpx
	rm -rf /tmp/test/K2HFUSETEST-*.k2h
	rm -rf /tmp/k2hftfuse_np_*
	rm -rf /tmp/test/nest_*.log
	rm -rf /tmp/test/single_*.log

	#######################
	# Finish
	#######################
	echo ""
	echo "JSON STRING TEST RESULT --> SUCCEED"
	echo ""
fi

##############################################################
# Start json environment conf
##############################################################
if [ "X${DO_JSON_ENV}" = "Xyes" ]; then
	echo ""
	echo ""
	echo "====== START TEST FOR JSON ENVIRONMENT ====================="
	echo ""

	#######################
	# TEST for SINGLE SERVER
	#######################
	#
	# chmpx for server
	#
	echo "------ RUN chmpx on server side ----------------------------"
	CHMJSONCONF="${K2HFTFUSE_TEST_SERVER_JSON}" chmpx -d ${DEBUGMODE} > ${SINGLE_CHMPX_SERVER_LOG} 2>&1 &
	CHMPXSVRPID=$!
	echo "chmpx on server side pid = ${CHMPXSVRPID}"
	sleep 2

	#
	# k2hftfusesvr server process
	#
	echo "------ RUN server process on server side ------------------"
	K2HFTSVRJSONCONF="${K2HFTFUSE_TEST_SERVER_JSON}" ${K2HFUSESVRBIN} -d ${DEBUGMODE} > ${SINGLE_K2HFTFUSESVR_LOG} 2>&1 &
	K2HFTFUSESVRPID=$!
	echo "server process pid = ${K2HFTFUSESVRPID}"
	sleep 2

	#
	# chmpx for slave
	#
	echo "------ RUN chmpx on slave side ----------------------------"
	CHMJSONCONF="${K2HFTFUSE_TEST_SLAVE_JSON}" chmpx -d ${DEBUGMODE} > ${SINGLE_CHMPX_SLAVE_LOG} 2>&1 &
	CHMPXSLVPID=$!
	echo "chmpx on slave side pid = ${CHMPXSLVPID}"
	sleep 2

	#
	# fuse client( without allow_other option)
	#
	echo "------ RUN fuse client ------------------------------------"
	K2HFTJSONCONF="${K2HFTFUSE_TEST_SLAVE_JSON}" K2HDBGMODE=INFO K2HDBGFILE=${SINGLE_K2HFTFUSE_K2HASH_LOG} CHMDBGMODE=MSG CHMDBGFILE=${SINGLE_K2HFTFUSE_CHMPX_LOG} ${K2HFUSEBIN} ${MOUNTDIR} -o dbglevel=msg,chmpxlog=${SINGLE_K2HFTFUSE_CHMPX_LOG} -f > ${SINGLE_K2HFTFUSE_LOG} 2>&1 &
	K2HFTFUSEPID=$!
	echo "k2hftfuse pid = ${K2HFTFUSEPID}"
	sleep 10

	#
	# check process
	#
	RESULT_RUN_K2HFTFUSE_PROC=true
	ps -p ${K2HFTFUSEPID} > /dev/null 2>&1
	if [ $? != 0 ]; then
		echo "ERROR: could not run k2hftfuse process"
		RESULT_RUN_K2HFTFUSE_PROC=false
	fi

	#
	# test( we need to run this if failed to run k2hftfuse for gcov )
	#
	echo "------ RUN test program(k2hftfusetest) --------------------"
	${K2HFUSETESTBIN} ${MOUNTDIR}/log/nothing2.log 10 MY_TEST_PROGRAM_OUTPUT > ${SINGLE_K2HFTFUSETEST_LOG} 2>&1
	TESTSUBRESULT=$?
	echo "k2hftfusetest finished."
	sleep 2

	#
	# exit all process
	#
	kill -HUP ${K2HFTFUSEPID} > /dev/null 2>&1
	sleep 1
	kill -HUP ${K2HFTFUSESVRPID} > /dev/null 2>&1
	sleep 1
	kill -HUP ${CHMPXSLVPID} > /dev/null 2>&1
	sleep 1
	kill -HUP ${CHMPXSVRPID} > /dev/null 2>&1
	sleep 1
	kill -9 ${K2HFTFUSEPID} ${K2HFTFUSESVRPID} ${CHMPXSLVPID} ${CHMPXSVRPID} > /dev/null 2>&1

	#
	# check line count
	#
	LINES=`grep 'log/nothing2.log' unify.log 2> /dev/null | wc -l 2> /dev/null | awk '{print $1}' 2> /dev/null`
	if [ "X$LINES" != "X10" ]; then
		echo "ERROR: unify.log file line count(${LINES}) is not as same as 10."
		TESTSUBRESULT=1
	fi

	#
	# result for single test
	#
	if [ ${TESTSUBRESULT} -ne 0 ]; then
		echo ""
		echo "### FAILED ON SINGLE TEST #########################################################################"
		echo ""
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${SINGLE_CHMPX_SERVER_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${SINGLE_CHMPX_SERVER_LOG}
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${SINGLE_CHMPX_SLAVE_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${SINGLE_CHMPX_SLAVE_LOG}
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${SINGLE_K2HFTFUSESVR_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${SINGLE_K2HFTFUSESVR_LOG}
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${SINGLE_K2HFTFUSE_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${SINGLE_K2HFTFUSE_LOG}
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${SINGLE_K2HFTFUSE_CHMPX_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${SINGLE_K2HFTFUSE_CHMPX_LOG}
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${SINGLE_K2HFTFUSE_K2HASH_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${SINGLE_K2HFTFUSE_K2HASH_LOG}
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${SINGLE_K2HFTFUSETEST_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${SINGLE_K2HFTFUSETEST_LOG}
		echo "---------------------------------------------------------------------------------------------------"

		if [ "X${RESULT_RUN_K2HFTFUSE_PROC}" = "Xtrue" ]; then
			echo ""
			echo "RESULT --> FAILED(ON SINGLE TEST)"
			echo ""
			exit 1
		fi

		if [ ${IS_SAFE_TEST} -eq 0 ]; then
			echo ""
			echo "RESULT --> FAILED(ON SINGLE TEST)"
			echo ""
			exit 1
		fi

		echo ""
		echo "***************************************************************************************************"
		echo "This test is run on CI(docker) and we could not run k2hftfuse process."
		echo "Maybe, you need to set \"--cap-add SYS_ADMIN --device /dev/fuse --security-opt apparmor:unconfined\""
		echo "to your docker."
		echo "And you can find \"fuse: failed to open /dev/fuse: Operation not permitted\" in logs, when this failure"
		echo "reason is permission on docker."
		echo ""
		echo "***** THEN WE SKIP THIS TEST, MUST CHECK TEST ON YOUR LOCAL ENVIRONMENT *****"
		echo ""
		echo "***************************************************************************************************"
		echo ""
	else
		#
		# cleaning
		#
		rm -f /tmp/mnt/k2hftfusesvr/log/unify.log
		if [ $? -ne 0 ]; then
			echo "RESULT --> FAILED(COULD NOT REMOVE RESULT FILE)"
			exit 1
		fi
	fi
	force_stop_all_processes ${MOUNTDIR} > /dev/null 2>&1

	#
	# clean old files in /tmp
	#
	rm -rf /tmp/mnt/k2hftfuse
	rm -rf /tmp/mnt/k2hftfusesvr
	rm -rf /tmp/test/K2HFUSESVRTEST-*.chmpx
	rm -rf /tmp/test/K2HFUSESVRTEST-*.k2h
	rm -rf /tmp/test/K2HFUSETEST-*.chmpx
	rm -rf /tmp/test/K2HFUSETEST-*.k2h
	rm -rf /tmp/k2hftfuse_np_*
	rm -rf /tmp/test/nest_*.log
	rm -rf /tmp/test/single_*.log

	#######################
	# TEST for NEST SERVER
	#######################
	#
	# chmpx for transferred server
	#
	echo "------ RUN transferred chmpx on server side ----------------"
	CHMJSONCONF="${K2HFTFUSESVR_TEST_TRANS_SERVER_JSON}" chmpx -d ${DEBUGMODE} > ${NEST_TRANS_CHMPX_SERVER_LOG} 2>&1 &
	CHMPXTRANSSVRPID=$!
	echo "transferred chmpx on server side pid = ${CHMPXTRANSSVRPID}"
	sleep 2

	#
	# k2hftfusesvr transferred server process
	#
	echo "------ RUN transferred server process on server side -------"
	K2HFTSVRJSONCONF="${K2HFTFUSESVR_TEST_TRANS_SERVER_JSON}" ${K2HFUSESVRBIN} -d ${DEBUGMODE} > ${NEST_TRANS_K2HFTFUSESVR_LOG} 2>&1 &
	K2HFTFUSESVRTRANSPID=$!
	echo "transferred server process pid = ${K2HFTFUSESVRTRANSPID}"
	sleep 2

	#
	# transferred chmpx for slave
	#
	echo "------ RUN transferred chmpx on slave side -----------------"
	CHMJSONCONF="${K2HFTFUSESVR_TEST_TRANS_SLAVE_JSON}" chmpx -d ${DEBUGMODE} > ${NEST_TRANS_CHMPX_SLAVE_LOG} 2>&1 &
	CHMPXTRANSSLVPID=$!
	echo "transferred chmpx on slave side pid = ${CHMPXTRANSSLVPID}"
	sleep 2

	#
	# chmpx for server
	#
	echo "------ RUN chmpx on server side ----------------------------"
	CHMJSONCONF="${K2HFTFUSE_TEST_TRANS_SERVER_JSON}" chmpx -d ${DEBUGMODE} > ${NEST_CHMPX_SERVER_LOG} 2>&1 &
	CHMPXSVRPID=$!
	echo "chmpx on server side pid = ${CHMPXSVRPID}"
	sleep 2

	#
	# k2hftfusesvr server process
	#
	echo "------ RUN server process on server side ------------------"
	K2HFTSVRJSONCONF="${K2HFTFUSE_TEST_TRANS_SERVER_JSON}" ${K2HFUSESVRBIN} -d ${DEBUGMODE} > ${NEST_K2HFTFUSESVR_LOG} 2>&1 &
	K2HFTFUSESVRPID=$!
	echo "server process pid = ${K2HFTFUSESVRPID}"
	sleep 2

	#
	# chmpx for slave
	#
	echo "------ RUN chmpx on slave side ----------------------------"
	CHMJSONCONF="${K2HFTFUSE_TEST_SLAVE_JSON}" chmpx -d ${DEBUGMODE} > ${NEST_CHMPX_SLAVE_LOG} 2>&1 &
	CHMPXSLVPID=$!
	echo "chmpx on slave side pid = ${CHMPXSLVPID}"
	sleep 2

	#
	# fuse client ( without allow_other option )
	#
	echo "------ RUN fuse client ------------------------------------"
	K2HFTJSONCONF="${K2HFTFUSE_TEST_SLAVE_JSON}" K2HDBGMODE=INFO K2HDBGFILE=${NEST_K2HFTFUSE_K2HASH_LOG} CHMDBGMODE=MSG CHMDBGFILE=${NEST_K2HFTFUSE_CHMPX_LOG} ${K2HFUSEBIN} ${MOUNTDIR} -o dbglevel=msg,chmpxlog=${NEST_K2HFTFUSE_CHMPX_LOG} -f > ${NEST_K2HFTFUSE_LOG} 2>&1 &
	K2HFTFUSEPID=$!
	echo "k2hftfuse pid = ${K2HFTFUSEPID}"
	sleep 10

	#
	# check process
	#
	RESULT_RUN_K2HFTFUSE_PROC=true
	ps -p ${K2HFTFUSEPID} > /dev/null 2>&1
	if [ $? != 0 ]; then
		echo "ERROR: could not run k2hftfuse process"
		RESULT_RUN_K2HFTFUSE_PROC=false
	fi

	#
	# test( we need to run this if failed to run k2hftfuse for gcov )
	#
	echo "------ RUN test program(k2hftfusetest) --------------------"
	${K2HFUSETESTBIN} ${MOUNTDIR}/log/nothing2.log 10 MY_TEST_PROGRAM_OUTPUT > ${NEST_K2HFTFUSETEST_LOG} 2>&1
	TESTSUBRESULT=$?
	echo "k2hftfusetest finished."
	sleep 2

	#
	# exit all process
	#
	kill -HUP ${K2HFTFUSEPID} > /dev/null 2>&1
	sleep 1
	kill -HUP ${K2HFTFUSESVRPID} > /dev/null 2>&1
	sleep 1
	kill -HUP ${K2HFTFUSESVRTRANSPID} > /dev/null 2>&1
	sleep 1
	kill -HUP ${CHMPXSLVPID} > /dev/null 2>&1
	sleep 1
	kill -HUP ${CHMPXSVRPID} > /dev/null 2>&1
	sleep 1
	kill -HUP ${CHMPXTRANSSLVPID} > /dev/null 2>&1
	sleep 1
	kill -HUP ${CHMPXTRANSSVRPID} > /dev/null 2>&1
	sleep 1
	kill -9 ${K2HFTFUSEPID} ${K2HFTFUSESVRPID} ${K2HFTFUSESVRTRANSPID} ${CHMPXSLVPID} ${CHMPXSVRPID} ${CHMPXTRANSSLVPID} ${CHMPXTRANSSVRPID} > /dev/null 2>&1

	#
	# check line count
	#
	LINES=`grep 'log/nothing2.log' unify.log 2> /dev/null | wc -l 2> /dev/null | awk '{print $1}' 2> /dev/null`
	if [ "X$LINES" != "X10" ]; then
		echo "ERROR: unify.log file line count(${LINES}) is not as same as 10."
		TESTSUBRESULT=1
	fi

	#
	# result for single test
	#
	if [ ${TESTSUBRESULT} -ne 0 ]; then
		echo ""
		echo "### FAILED ON NEST TEST ###########################################################################"
		echo ""
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${NEST_TRANS_CHMPX_SERVER_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${NEST_TRANS_CHMPX_SERVER_LOG}
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${NEST_CHMPX_SERVER_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${NEST_CHMPX_SERVER_LOG}
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${NEST_TRANS_CHMPX_SLAVE_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${NEST_TRANS_CHMPX_SLAVE_LOG}
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${NEST_CHMPX_SLAVE_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${NEST_CHMPX_SLAVE_LOG}
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${NEST_TRANS_K2HFTFUSESVR_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${NEST_TRANS_K2HFTFUSESVR_LOG}
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${NEST_K2HFTFUSESVR_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${NEST_K2HFTFUSESVR_LOG}
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${NEST_K2HFTFUSE_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${NEST_K2HFTFUSE_LOG}
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${NEST_K2HFTFUSE_CHMPX_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${NEST_K2HFTFUSE_CHMPX_LOG}
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${NEST_K2HFTFUSE_K2HASH_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${NEST_K2HFTFUSE_K2HASH_LOG}
		echo "---------------------------------------------------------------------------------------------------"
		echo " LOG: ${NEST_K2HFTFUSETEST_LOG}"
		echo "---------------------------------------------------------------------------------------------------"
		cat ${NEST_K2HFTFUSETEST_LOG}
		echo "---------------------------------------------------------------------------------------------------"

		if [ "X${RESULT_RUN_K2HFTFUSE_PROC}" = "Xtrue" ]; then
			echo ""
			echo "RESULT --> FAILED(ON NEST TEST)"
			echo ""
			exit 1
		fi

		if [ ${IS_SAFE_TEST} -eq 0 ]; then
			echo ""
			echo "RESULT --> FAILED(ON NEST TEST)"
			echo ""
			exit 1
		fi

		echo ""
		echo "***************************************************************************************************"
		echo "This test is run on CI(docker) and we could not run k2hftfuse process."
		echo "Maybe, you need to set \"--cap-add SYS_ADMIN --device /dev/fuse --security-opt apparmor:unconfined\""
		echo "to your docker."
		echo "And you can find \"fuse: failed to open /dev/fuse: Operation not permitted\" in logs, when this failure"
		echo "reason is permission on docker."
		echo ""
		echo "***** THEN WE SKIP THIS TEST, MUST CHECK TEST ON YOUR LOCAL ENVIRONMENT *****"
		echo ""
		echo "***************************************************************************************************"
		echo ""
	else
		#
		# cleaning
		#
		rm -f /tmp/mnt/k2hftfusesvr/log/unify.log
		if [ $? -ne 0 ]; then
			echo "RESULT --> FAILED(COULD NOT REMOVE RESULT FILE)"
			exit 1
		fi
	fi
	force_stop_all_processes ${MOUNTDIR} > /dev/null 2>&1

	#
	# clean old files in /tmp
	#
	rm -rf /tmp/mnt/k2hftfuse
	rm -rf /tmp/mnt/k2hftfusesvr
	rm -rf /tmp/test/K2HFUSESVRTEST-*.chmpx
	rm -rf /tmp/test/K2HFUSESVRTEST-*.k2h
	rm -rf /tmp/test/K2HFUSETEST-*.chmpx
	rm -rf /tmp/test/K2HFUSETEST-*.k2h
	rm -rf /tmp/k2hftfuse_np_*
	rm -rf /tmp/test/nest_*.log
	rm -rf /tmp/test/single_*.log

	#######################
	# Finish
	#######################
	echo ""
	echo "JSON STRING TEST RESULT --> SUCCEED"
	echo ""
fi

##############################################################
# Finished all test
##############################################################
echo ""
echo "====== Finish all =========================================="

exit 0

#
# Local variables:
# tab-width: 4
# c-basic-offset: 4
# End:
# vim600: noexpandtab sw=4 ts=4 fdm=marker
# vim<600: noexpandtab sw=4 ts=4
#
