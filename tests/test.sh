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

#--------------------------------------------------------------
# Common Variables
#--------------------------------------------------------------
PRGNAME=$(basename "${0}")
SCRIPTDIR=$(dirname "${0}")
SCRIPTDIR=$(cd "${SCRIPTDIR}" || exit 1; pwd)
SRCTOP=$(cd "${SCRIPTDIR}/.." || exit 1; pwd)

#
# Directories
#
SRCDIR="${SRCTOP}/src"
TESTDIR="${SRCTOP}/tests"

MOUNTDIR=$(cd -P "${HOME}" || exit 1; pwd)
MOUNTDIR="${MOUNTDIR}/k2htpfs"

TEST_LOG_TOPDIR="/tmp/test"
TEST_SUBPROC_TOPDIR="/tmp/mnt"

K2HFTFUSE_FILES_TOPDIR="${TEST_SUBPROC_TOPDIR}/k2hftfuse"
K2HFTFUSESVR_FILES_TOPDIR="${TEST_SUBPROC_TOPDIR}/k2hftfusesvr"

#
# Binary files
#
K2HFUSEBIN="${SRCDIR}/k2hftfuse"
K2HFUSESVRBIN="${SRCDIR}/k2hftfusesvr"
K2HFUSETESTBIN="${TESTDIR}/k2hftfusetest"
K2HFTFUSE_PREINST="${SRCTOP}/buildutils/k2hftfuse.preinst"

#
# Files
#
CFG_JSON_STRING_DATA_FILE="${TESTDIR}/test_json_string.data"
CFG_SVR_FILE_PREFIX="${TESTDIR}/k2hftfuse_test_server"
CFG_SLV_FILE_PREFIX="${TESTDIR}/k2hftfuse_test_slave"
CFG_TRANS_SVR_FILE_PREFIX="${TESTDIR}/k2hftfusesvr_test_trans_server"
CFG_TRANS_SLV_FILE_PREFIX="${TESTDIR}/k2hftfusesvr_test_trans_slave"
CFG_TRANS_FUSE_FILE_PREFIX="${TESTDIR}/k2hftfuse_test_trans_server"

#
# Log files
#
SINGLE_CHMPX_SERVER_LOG="${TEST_LOG_TOPDIR}/single_chmpx_server.log"
SINGLE_CHMPX_SLAVE_LOG="${TEST_LOG_TOPDIR}/single_chmpx_slave.log"
SINGLE_K2HFTFUSESVR_LOG="${TEST_LOG_TOPDIR}/single_k2hftfusesvr.log"
SINGLE_K2HFTFUSE_LOG="${TEST_LOG_TOPDIR}/single_k2hftfuse.log"
SINGLE_K2HFTFUSE_CHMPX_LOG="${TEST_LOG_TOPDIR}/single_k2hftfuse_chmpx.log"
SINGLE_K2HFTFUSE_K2HASH_LOG="${TEST_LOG_TOPDIR}/single_k2hftfuse_k2hash.log"
SINGLE_K2HFTFUSETEST_LOG="${TEST_LOG_TOPDIR}/single_k2hftfusetest.log"

NEST_TRANS_CHMPX_SERVER_LOG="${TEST_LOG_TOPDIR}/nest_trans_chmpx_server.log"
NEST_CHMPX_SERVER_LOG="${TEST_LOG_TOPDIR}/nest_chmpx_server.log"
NEST_TRANS_CHMPX_SLAVE_LOG="${TEST_LOG_TOPDIR}/nest_trans_chmpx_slave.log"
NEST_CHMPX_SLAVE_LOG="${TEST_LOG_TOPDIR}/nest_chmpx_slave.log"
NEST_TRANS_K2HFTFUSESVR_LOG="${TEST_LOG_TOPDIR}/nest_trans_k2hftfusesvr.log"
NEST_K2HFTFUSESVR_LOG="${TEST_LOG_TOPDIR}/nest_k2hftfusesvr.log"
NEST_K2HFTFUSE_LOG="${TEST_LOG_TOPDIR}/nest_k2hftfuse.log"
NEST_K2HFTFUSE_CHMPX_LOG="${TEST_LOG_TOPDIR}/nest_k2hftfuse_chmpx.log"
NEST_K2HFTFUSE_K2HASH_LOG="${TEST_LOG_TOPDIR}/nest_k2hftfuse_k2hash.log"
NEST_K2HFTFUSETEST_LOG="${TEST_LOG_TOPDIR}/nest_k2hftfusetest.log"

K2HFTFUSESVR_UNITY_LOGFILE_NAME="unify.log"
K2HFTFUSESVR_UNITY_LOGFILE="${K2HFTFUSESVR_FILES_TOPDIR}/log/${K2HFTFUSESVR_UNITY_LOGFILE_NAME}"

K2HFTFUSETEST_LOGFILE_RELPATH="log/nothing2.log"
K2HFTFUSETEST_LOGFILE="${MOUNTDIR}/log/nothing2.log"

#
# Other
#
K2HFTFUSETEST_LOOP_COUNT=10
K2HFTFUSETEST_OUTPUT_STRING="MY_TEST_PROGRAM_OUTPUT"

WAIT_SEC_AFTER_RUN_CHMPX=4
WAIT_SEC_AFTER_RUN_K2HFTFUSE_SVR=4
WAIT_SEC_AFTER_RUN_K2HFTFUSE=10
WAIT_SEC_AFTER_RUN_K2HFTFUSETEST=10
WAIT_SEC_AFTER_STOP_PROCESS=4
WAIT_SEC_AFTER_UMOUNT_K2HFTFUSE=10

#--------------------------------------------------------------
# Usage
#--------------------------------------------------------------
func_usage()
{
	echo ""
	echo "Usage: $1 { -help } { ini_conf } { yaml_conf } { json_conf } { json_string } { json_env } { silent | err | wan | msg }"
	echo ""
}

#--------------------------------------------------------------
# Variables and Utility functions
#--------------------------------------------------------------
#
# Escape sequence
#
if [ -t 1 ]; then
	CBLD=$(printf '\033[1m')
	CREV=$(printf '\033[7m')
	CRED=$(printf '\033[31m')
	CYEL=$(printf '\033[33m')
	CGRN=$(printf '\033[32m')
	CDEF=$(printf '\033[0m')
else
	CBLD=""
	CREV=""
	CRED=""
	CYEL=""
	CGRN=""
	CDEF=""
fi

#--------------------------------------------------------------
# Message functions
#--------------------------------------------------------------
PRNTITLE()
{
	echo ""
	echo "${CGRN}---------------------------------------------------------------------${CDEF}"
	echo "${CGRN}${CREV}[TITLE]${CDEF} ${CGRN}$*${CDEF}"
	echo "${CGRN}---------------------------------------------------------------------${CDEF}"
}

PRNERR()
{
	echo "${CBLD}${CRED}[ERROR]${CDEF} ${CRED}$*${CDEF}"
}

PRNWARN()
{
	echo "${CYEL}${CREV}[WARNING]${CDEF} $*"
}

PRNMSG()
{
	echo "${CYEL}${CREV}[MSG]${CDEF} ${CYEL}$*${CDEF}"
}

PRNINFO()
{
	echo "${CREV}[INFO]${CDEF} $*"
}

PRNSUCCEED()
{
	echo "${CREV}[SUCCEED]${CDEF} $*"
}

#--------------------------------------------------------------
# Utilitiy functions
#--------------------------------------------------------------
#
# Run all processes
#
# $1:	mode(single / transferred)
# $2:	type(ini, yaml, json, jsonstring, jsonenv)
#
run_all_processes()
{
	if [ $# -ne 2 ] || [ -z "$1" ] || [ -z "$2" ]; then
		PRNERR "run_all_processes function parameters are wrong."
		return 1
	fi
	if [ "$1" = "single" ] || [ "$1" = "SINGLE" ]; then
		_IS_TRANSFERRED=0

		LOCAL_CHMPX_SERVER_LOG="${SINGLE_CHMPX_SERVER_LOG}"
		LOCAL_K2HFTFUSESVR_LOG="${SINGLE_K2HFTFUSESVR_LOG}"
		LOCAL_CHMPX_SLAVE_LOG="${SINGLE_CHMPX_SLAVE_LOG}"
		LOCAL_K2HFTFUSE_K2HASH_LOG="${SINGLE_K2HFTFUSE_K2HASH_LOG}"
		LOCAL_K2HFTFUSE_CHMPX_LOG="${SINGLE_K2HFTFUSE_CHMPX_LOG}"
		LOCAL_K2HFTFUSE_LOG="${SINGLE_K2HFTFUSE_LOG}"

	elif [ "$1" = "nest" ] || [ "$1" = "NEST" ] || [ "$1" = "trans" ] || [ "$1" = "TRANS" ] || [ "$1" = "transferred" ] || [ "$1" = "TRANSFERRED" ]; then
		_IS_TRANSFERRED=1

		LOCAL_CHMPX_SERVER_LOG="${NEST_CHMPX_SERVER_LOG}"
		LOCAL_K2HFTFUSESVR_LOG="${NEST_K2HFTFUSESVR_LOG}"
		LOCAL_CHMPX_SLAVE_LOG="${NEST_CHMPX_SLAVE_LOG}"
		LOCAL_K2HFTFUSE_K2HASH_LOG="${NEST_K2HFTFUSE_K2HASH_LOG}"
		LOCAL_K2HFTFUSE_CHMPX_LOG="${NEST_K2HFTFUSE_CHMPX_LOG}"
		LOCAL_K2HFTFUSE_LOG="${NEST_K2HFTFUSE_LOG}"

	else
		PRNERR "run_all_processes function first parameter($1) is wrong."
		return 1
	fi

	_JSON_ENV_MODE=0
	if [ "$2" = "ini" ] || [ "$2" = "INI" ]; then
		CONF_OPT_STRING="-conf"
		CONF_FILE_EXT=".ini"
		CONF_OPT_SLV_PARAM="${CFG_SLV_FILE_PREFIX}${CONF_FILE_EXT}"
		CONF_OPT_SLV_FUSE_PARAM="${CONF_OPT_SLV_PARAM}"

		if [ "${_IS_TRANSFERRED}" -eq 0 ]; then
			CONF_OPT_SVR_PARAM="${CFG_SVR_FILE_PREFIX}${CONF_FILE_EXT}"
		else
			CONF_OPT_SVR_PARAM="${CFG_TRANS_FUSE_FILE_PREFIX}${CONF_FILE_EXT}"
			CONF_OPT_TRANS_SVR_PARAM="${CFG_TRANS_SVR_FILE_PREFIX}${CONF_FILE_EXT}"
			CONF_OPT_TRANS_SLV_PARAM="${CFG_TRANS_SLV_FILE_PREFIX}${CONF_FILE_EXT}"
		fi

	elif [ "$2" = "yaml" ] || [ "$2" = "YAML" ]; then
		CONF_OPT_STRING="-conf"
		CONF_FILE_EXT=".yaml"
		CONF_OPT_SLV_PARAM="${CFG_SLV_FILE_PREFIX}${CONF_FILE_EXT}"
		CONF_OPT_SLV_FUSE_PARAM="${CONF_OPT_SLV_PARAM}"

		if [ "${_IS_TRANSFERRED}" -eq 0 ]; then
			CONF_OPT_SVR_PARAM="${CFG_SVR_FILE_PREFIX}${CONF_FILE_EXT}"
		else
			CONF_OPT_SVR_PARAM="${CFG_TRANS_FUSE_FILE_PREFIX}${CONF_FILE_EXT}"
			CONF_OPT_TRANS_SVR_PARAM="${CFG_TRANS_SVR_FILE_PREFIX}${CONF_FILE_EXT}"
			CONF_OPT_TRANS_SLV_PARAM="${CFG_TRANS_SLV_FILE_PREFIX}${CONF_FILE_EXT}"
		fi

	elif [ "$2" = "json" ] || [ "$2" = "JSON" ]; then
		CONF_OPT_STRING="-conf"
		CONF_FILE_EXT=".json"
		CONF_OPT_SLV_PARAM="${CFG_SLV_FILE_PREFIX}${CONF_FILE_EXT}"
		CONF_OPT_SLV_FUSE_PARAM="${CONF_OPT_SLV_PARAM}"

		if [ "${_IS_TRANSFERRED}" -eq 0 ]; then
			CONF_OPT_SVR_PARAM="${CFG_SVR_FILE_PREFIX}${CONF_FILE_EXT}"
		else
			CONF_OPT_SVR_PARAM="${CFG_TRANS_FUSE_FILE_PREFIX}${CONF_FILE_EXT}"
			CONF_OPT_TRANS_SVR_PARAM="${CFG_TRANS_SVR_FILE_PREFIX}${CONF_FILE_EXT}"
			CONF_OPT_TRANS_SLV_PARAM="${CFG_TRANS_SLV_FILE_PREFIX}${CONF_FILE_EXT}"
		fi

	elif [ "$2" = "jsonstring" ] || [ "$2" = "JSONSTRING" ] || [ "$2" = "sjson" ] || [ "$2" = "SJSON" ] || [ "$2" = "jsonstr" ] || [ "$2" = "JSONSTR" ]; then
		CONF_OPT_STRING="-json"
		CONF_OPT_SLV_PARAM="${K2HFTFUSE_TEST_SLAVE_JSON}"
		CONF_OPT_SLV_FUSE_PARAM="${K2HFTFUSE_TEST_SLAVE_JSON_FUSE}"

		if [ "${_IS_TRANSFERRED}" -eq 0 ]; then
			CONF_OPT_SVR_PARAM="${K2HFTFUSE_TEST_SERVER_JSON}"
		else
			CONF_OPT_SVR_PARAM="${K2HFTFUSE_TEST_TRANS_SERVER_JSON}"
			CONF_OPT_TRANS_SVR_PARAM="${K2HFTFUSESVR_TEST_TRANS_SERVER_JSON}"
			CONF_OPT_TRANS_SLV_PARAM="${K2HFTFUSESVR_TEST_TRANS_SLAVE_JSON}"
		fi

	elif [ "$2" = "jsonenv" ] || [ "$2" = "JSONENV" ] || [ "$2" = "ejson" ] || [ "$2" = "EJSON" ]; then
		_JSON_ENV_MODE=1

		CONF_OPT_SLV_PARAM="${K2HFTFUSE_TEST_SLAVE_JSON}"
		CONF_OPT_SLV_FUSE_PARAM="${K2HFTFUSE_TEST_SLAVE_JSON}"

		if [ "${_IS_TRANSFERRED}" -eq 0 ]; then
			CONF_OPT_SVR_PARAM="${K2HFTFUSE_TEST_SERVER_JSON}"
		else
			CONF_OPT_SVR_PARAM="${K2HFTFUSE_TEST_TRANS_SERVER_JSON}"
			CONF_OPT_TRANS_SVR_PARAM="${K2HFTFUSESVR_TEST_TRANS_SERVER_JSON}"
			CONF_OPT_TRANS_SLV_PARAM="${K2HFTFUSESVR_TEST_TRANS_SLAVE_JSON}"
		fi
	else
		PRNERR "run_all_processes function second parameter($2) is wrong."
		return 1
	fi

	#------------------------------------------------------
	# RUN chmpx processes and k2hdkc server processes
	#------------------------------------------------------
	if [ "${_IS_TRANSFERRED}" -ne 0 ]; then
		PRNMSG "RUN K2HFTFUSE TRANSFERRED SERVERS"

		#
		# Run transferred chmpx server
		#
		if [ "${_JSON_ENV_MODE}" -ne 1 ]; then
			"${CHMPXBIN}" "${CONF_OPT_STRING}" "${CONF_OPT_TRANS_SVR_PARAM}" -d "${DEBUGMODE}" > "${NEST_TRANS_CHMPX_SERVER_LOG}" 2>&1 &
		else
			CHMJSONCONF="${CONF_OPT_TRANS_SVR_PARAM}" "${CHMPXBIN}" -d "${DEBUGMODE}" > "${NEST_TRANS_CHMPX_SERVER_LOG}" 2>&1 &
		fi
		CHMPXTRANSSVRPID=$!
		sleep "${WAIT_SEC_AFTER_RUN_CHMPX}"
		PRNINFO "CHMPX(tranferred server) : ${CHMPXTRANSSVRPID}"

		#
		# Run transferred k2hftfusesvr
		#
		if [ "${_JSON_ENV_MODE}" -ne 1 ]; then
			"${K2HFUSESVRBIN}" "${CONF_OPT_STRING}" "${CONF_OPT_TRANS_SVR_PARAM}" -d "${DEBUGMODE}" > "${NEST_TRANS_K2HFTFUSESVR_LOG}" 2>&1 &
		else
			K2HFTSVRJSONCONF="${CONF_OPT_TRANS_SVR_PARAM}" "${K2HFUSESVRBIN}" -d "${DEBUGMODE}" > "${NEST_TRANS_K2HFTFUSESVR_LOG}" 2>&1 &
		fi
		K2HFTFUSESVRTRANSPID=$!
		sleep "${WAIT_SEC_AFTER_RUN_K2HFTFUSE_SVR}"
		PRNINFO "K2HFTFUSESVR(tranferred) : ${K2HFTFUSESVRTRANSPID}"

		#
		# Run transferred chmpx slave
		#
		if [ "${_JSON_ENV_MODE}" -ne 1 ]; then
			"${CHMPXBIN}" "${CONF_OPT_STRING}" "${CONF_OPT_TRANS_SLV_PARAM}" -d "${DEBUGMODE}" > "${NEST_TRANS_CHMPX_SLAVE_LOG}" 2>&1 &
		else
			CHMJSONCONF="${CONF_OPT_TRANS_SLV_PARAM}" "${CHMPXBIN}" -d "${DEBUGMODE}" > "${NEST_TRANS_CHMPX_SLAVE_LOG}" 2>&1 &
		fi
		CHMPXTRANSSLVPID=$!
		sleep "${WAIT_SEC_AFTER_RUN_CHMPX}"
		PRNINFO "CHMPX(tranferred slave)  : ${CHMPXTRANSSLVPID}"
	fi

	PRNMSG "RUN K2HFTFUSE SERVERS"
	#
	# Run chmpx server
	#
	if [ "${_JSON_ENV_MODE}" -ne 1 ]; then
		"${CHMPXBIN}" "${CONF_OPT_STRING}" "${CONF_OPT_SVR_PARAM}" -d "${DEBUGMODE}" > "${LOCAL_CHMPX_SERVER_LOG}" 2>&1 &
	else
		CHMJSONCONF="${CONF_OPT_SVR_PARAM}" "${CHMPXBIN}" -d "${DEBUGMODE}" > "${LOCAL_CHMPX_SERVER_LOG}" 2>&1 &
	fi
	CHMPXSVRPID=$!
	sleep "${WAIT_SEC_AFTER_RUN_CHMPX}"
	PRNINFO "CHMPX(server)            : ${CHMPXSVRPID}"

	#
	# Run k2hftfusesvr
	#
	if [ "${_JSON_ENV_MODE}" -ne 1 ]; then
		"${K2HFUSESVRBIN}" "${CONF_OPT_STRING}" "${CONF_OPT_SVR_PARAM}" -d "${DEBUGMODE}" > "${LOCAL_K2HFTFUSESVR_LOG}" 2>&1 &
	else
		K2HFTSVRJSONCONF="${CONF_OPT_SVR_PARAM}" "${K2HFUSESVRBIN}" -d "${DEBUGMODE}" > "${LOCAL_K2HFTFUSESVR_LOG}" 2>&1 &
	fi
	K2HFTFUSESVRPID=$!
	sleep "${WAIT_SEC_AFTER_RUN_K2HFTFUSE_SVR}"
	PRNINFO "K2HFTFUSESVR             : ${K2HFTFUSESVRPID}"

	#
	# Run chmpx slave
	#
	if [ "${_JSON_ENV_MODE}" -ne 1 ]; then
		"${CHMPXBIN}" "${CONF_OPT_STRING}" "${CONF_OPT_SLV_PARAM}" -d "${DEBUGMODE}" > "${LOCAL_CHMPX_SLAVE_LOG}" 2>&1 &
	else
		CHMJSONCONF="${CONF_OPT_SLV_PARAM}" "${CHMPXBIN}" -d "${DEBUGMODE}" > "${LOCAL_CHMPX_SLAVE_LOG}" 2>&1 &
	fi
	CHMPXSLVPID=$!
	sleep "${WAIT_SEC_AFTER_RUN_CHMPX}"
	PRNINFO "CHMPX(slave)             : ${CHMPXSLVPID}"

	#
	# Run k2hftfuse( without allow_other option)
	#
	# [NOTE]
	# CI such as GithubActions may fail to start k2hftfuse.
	# The cause of this may be that the test relies on k2hftfuse being
	# started and terminated several times.
	# Now, we have not been able to identify the cause, but it has been
	# resolved by restarting.
	#
	_RETRY_COUNT=10
	while [ "${_RETRY_COUNT}" -gt 0 ]; do
		#
		# Run
		#
		if [ "${_JSON_ENV_MODE}" -ne 1 ]; then
			K2HDBGMODE=INFO K2HDBGFILE="${LOCAL_K2HFTFUSE_K2HASH_LOG}" CHMDBGMODE=MSG CHMDBGFILE="${LOCAL_K2HFTFUSE_CHMPX_LOG}" "${K2HFUSEBIN}" "${MOUNTDIR}" -o dbglevel=msg,conf="${CONF_OPT_SLV_FUSE_PARAM}",chmpxlog="${LOCAL_K2HFTFUSE_CHMPX_LOG}" -f > "${LOCAL_K2HFTFUSE_LOG}" 2>&1 &
		else
			K2HFTJSONCONF="${CONF_OPT_SLV_FUSE_PARAM}" K2HDBGMODE=INFO K2HDBGFILE="${LOCAL_K2HFTFUSE_K2HASH_LOG}" CHMDBGMODE=MSG CHMDBGFILE="${LOCAL_K2HFTFUSE_CHMPX_LOG}" "${K2HFUSEBIN}" "${MOUNTDIR}" -o dbglevel=msg,chmpxlog="${LOCAL_K2HFTFUSE_CHMPX_LOG}" -f > "${LOCAL_K2HFTFUSE_LOG}" 2>&1 &
		fi
		K2HFTFUSEPID=$!
		sleep "${WAIT_SEC_AFTER_RUN_K2HFTFUSE}"

		#
		# Check k2hftfuse process
		#
		if df | grep -q "${MOUNTDIR}"; then
			PRNINFO "K2HFTFUSE                : ${K2HFTFUSEPID}"
			break
		fi

		_RETRY_COUNT=$((_RETRY_COUNT - 1))
	done

	if [ "${_RETRY_COUNT}" -le 0 ]; then
		PRNINFO "K2HFTFUSE                : <Not found>"
		PRNERR "Not found mount point ${MOUNTDIR}, it maybe not run k2hftfuse process."
		return 1
	fi

	return 0
}

#
# Stop processes
#
# shellcheck disable=SC2317
stop_process()
{
	if [ $# -eq 0 ]; then
		return 1
	fi
	_STOP_PIDS="$*"

	_STOP_RESULT=0
	for _ONE_PID in ${_STOP_PIDS}; do
		_MAX_TRYCOUNT=10

		while [ "${_MAX_TRYCOUNT}" -gt 0 ]; do
			#
			# Check running status
			#
			if ! ps -p "${_ONE_PID}" >/dev/null 2>&1; then
				break
			fi
			#
			# Try HUP
			#
			kill -HUP "${_ONE_PID}" > /dev/null 2>&1
			sleep 1
			if ! ps -p "${_ONE_PID}" >/dev/null 2>&1; then
				break
			fi

			#
			# Try KILL
			#
			kill -KILL "${_ONE_PID}" > /dev/null 2>&1
			sleep 1
			if ! ps -p "${_ONE_PID}" >/dev/null 2>&1; then
				break
			fi
			_MAX_TRYCOUNT=$((_MAX_TRYCOUNT - 1))
		done

		if [ "${_MAX_TRYCOUNT}" -le 0 ]; then
			# shellcheck disable=SC2009
			if ps -p "${_ONE_PID}" | grep -v PID | grep -q -i 'defunct'; then
				PRNWARN "Could not stop ${_ONE_PID} process, because it has defunct status. So assume we were able to stop it."
			else
				PRNERR "Could not stop ${_ONE_PID} process"
				_STOP_RESULT=1
			fi
		fi
	done
	return "${_STOP_RESULT}"
}

#
# Stop processes
#
# $1:		mountpoint(directory path)
# $2...: 	process ids
#
stop_processes()
{
	if [ $# -lt 1 ] || [ -z "$1" ]; then
		PRNERR "stop_processes function prameter is wrong."
		return 1
	fi
	TG_MOUNT_DIR="$1"
	shift
	_STOP_PIDS="$*"

	#
	# Umount k2hftfuse
	#
	if df | grep -q "${TG_MOUNT_DIR}"; then
		if ! fusermount -u "${TG_MOUNT_DIR}" 2>/dev/null; then
			PRNWARN "Failed to umount ${TG_MOUNT_DIR}, but processing will continue."
		fi
		sleep "${WAIT_SEC_AFTER_UMOUNT_K2HFTFUSE}"
	fi

	#
	# Stop all processes
	#
	_STOP_RESULT=0
	for _ONE_PID in ${_STOP_PIDS}; do
		_MAX_TRYCOUNT=10

		while [ "${_MAX_TRYCOUNT}" -gt 0 ]; do
			#
			# Check running status
			#
			if ! ps -p "${_ONE_PID}" >/dev/null 2>&1; then
				break
			fi
			#
			# Try HUP
			#
			if kill -HUP "${_ONE_PID}" > /dev/null 2>&1; then
				sleep "${WAIT_SEC_AFTER_STOP_PROCESS}"
			else
				PRNWARN "Failed to send singnal(HUP) to ${_ONE_PID} process."
			fi
			if ! ps -p "${_ONE_PID}" >/dev/null 2>&1; then
				break
			fi

			#
			# Try KILL
			#
			if kill -KILL "${_ONE_PID}" > /dev/null 2>&1; then
				sleep "${WAIT_SEC_AFTER_STOP_PROCESS}"
			else
				PRNWARN "Failed to send singnal(KILL) to ${_ONE_PID} process."
			fi
			if ! ps -p "${_ONE_PID}" >/dev/null 2>&1; then
				break
			fi
			_MAX_TRYCOUNT=$((_MAX_TRYCOUNT - 1))
		done

		if [ "${_MAX_TRYCOUNT}" -le 0 ]; then
			# shellcheck disable=SC2009
			if ps -p "${_ONE_PID}" | grep -v PID | grep -q -i 'defunct'; then
				PRNWARN "Could not stop ${_ONE_PID} process, because it has defunct status. So assume we were able to stop it."
			else
				PRNERR "Could not stop ${_ONE_PID} process"
				_STOP_RESULT=1
			fi
		fi
	done
	return "${_STOP_RESULT}"
}

#
# Cleanup directries and log files
#
cleanup_directories_logfiles()
{
	#
	# Cleanup log files in /tmp
	#
	rm -rf "${K2HFTFUSE_FILES_TOPDIR}"
	rm -rf "${K2HFTFUSESVR_FILES_TOPDIR}"
	rm -rf /tmp/k2hftfuse_np_*
	rm -f "${TEST_LOG_TOPDIR}"/K2HFUSESVRTEST-*.chmpx
	rm -f "${TEST_LOG_TOPDIR}"/K2HFUSESVRTEST-*.k2h
	rm -f "${TEST_LOG_TOPDIR}"/K2HFUSETEST-*.chmpx
	rm -f "${TEST_LOG_TOPDIR}"/K2HFUSETEST-*.k2h
	rm -f "${TEST_LOG_TOPDIR}"/nest_*.log
	rm -f "${TEST_LOG_TOPDIR}"/single_*.log

	#
	# Cleanup and Initialize directories
	#
	if [ -d "${TEST_LOG_TOPDIR}" ] || [ -f "${TEST_LOG_TOPDIR}" ]; then
		rm -rf "${TEST_LOG_TOPDIR}"
	fi
	if ! mkdir -p "${TEST_LOG_TOPDIR}"; then
		PRNERR "Could not create ${TEST_LOG_TOPDIR}"
		return 1
	fi
	if ! chmod 0777 "${TEST_LOG_TOPDIR}"; then
		PRNERR "Could not set permisstion to ${TEST_LOG_TOPDIR}"
		return 1
	fi

	if [ -d "${TEST_SUBPROC_TOPDIR}" ] || [ -f "${TEST_SUBPROC_TOPDIR}" ]; then
		rm -rf "${TEST_SUBPROC_TOPDIR}"
	fi
	if ! mkdir -p "${TEST_SUBPROC_TOPDIR}"; then
		PRNERR "Could not create ${TEST_SUBPROC_TOPDIR}"
		return 1
	fi
	if ! chmod 0777 "${TEST_SUBPROC_TOPDIR}"; then
		PRNERR "Could not set permisstion to ${TEST_SUBPROC_TOPDIR}"
		return 1
	fi

	if [ -d "${MOUNTDIR}" ] || [ -f "${MOUNTDIR}" ]; then
		rm -rf "${MOUNTDIR}"
	fi
	if ! mkdir -p "${MOUNTDIR}"; then
		PRNERR "Cound not create ${MOUNTDIR} directory for mount point."
		return 1
	fi
	if ! chmod 0777 "${MOUNTDIR}"; then
		PRNERR "Could not set permisstion to ${MOUNTDIR} directory for mount point."
		return 1
	fi

	return 0
}

#
# Cleanup all(processes, directories, log files)
#
# $1:		mountpoint(directory path)
#
cleanup_all()
{
	if [ $# -ne 1 ] || [ -z "$1" ]; then
		PRNERR "stop_all_processes function prameter is wrong."
		return 1
	fi
	TG_MOUNT_DIR="$1"

	#
	# Get all process ids
	#
	# shellcheck disable=SC2009
	K2HFTFUSE_PROCS=$(ps ax | grep ' k2hftfuse ' | grep -v grep | awk '{print $1}' | tr '\n' ' ')
	# shellcheck disable=SC2009
	K2HFTFUSESVR_PROCS=$(ps ax | grep ' k2hftfusesvr ' | grep -v grep | awk '{print $1}' | tr '\n' ' ')
	# shellcheck disable=SC2009
	CHMPX_PROCS=$(ps ax | grep ' chmpx ' | grep -v grep | awk '{print $1}' | tr '\n' ' ')

	#
	# Stop all
	#
	if ! stop_processes "${TG_MOUNT_DIR}" "${K2HFTFUSE_PROCS} ${K2HFTFUSESVR_PROCS} ${CHMPX_PROCS}"; then
		PRNWARN "Some processes may have failed to stop, but continue processing, but continue...."
	fi

	#
	# Cleanup directories and log files
	#
	if ! cleanup_directories_logfiles; then
		PRNERR "Failed to cleanup directories or log files."
		return 1
	fi
	return 0
}

#
# Print all log
#
print_log_detail()
{
	PRNMSG "FAILED TEST : PRINT ALL LOG"

	_ALL_LOG_FILES="	${SINGLE_CHMPX_SERVER_LOG}
						${SINGLE_CHMPX_SLAVE_LOG}
						${SINGLE_K2HFTFUSESVR_LOG}
						${SINGLE_K2HFTFUSE_LOG}
						${SINGLE_K2HFTFUSE_CHMPX_LOG}
						${SINGLE_K2HFTFUSE_K2HASH_LOG}
						${SINGLE_K2HFTFUSETEST_LOG}
						${NEST_TRANS_CHMPX_SERVER_LOG}
						${NEST_CHMPX_SERVER_LOG}
						${NEST_TRANS_CHMPX_SLAVE_LOG}
						${NEST_CHMPX_SLAVE_LOG}
						${NEST_TRANS_K2HFTFUSESVR_LOG}
						${NEST_K2HFTFUSESVR_LOG}
						${NEST_K2HFTFUSE_LOG}
						${NEST_K2HFTFUSE_CHMPX_LOG}
						${NEST_K2HFTFUSE_K2HASH_LOG}
						${NEST_K2HFTFUSETEST_LOG}"

	for _ONE_LOG in ${_ALL_LOG_FILES}; do
		if [ -f "${_ONE_LOG}" ]; then
			echo "    [LOG] ${_ONE_LOG}"
			if ! sed -e 's/^/      /g' "${_ONE_LOG}"; then
				PRNERR "Somthing error occurred to access ${_ONE_LOG}"
			fi
			echo ""
		fi
	done

	return 0
}

#==============================================================
# Parse options
#==============================================================
#
# Variables
#
DO_INI_CONF=0
DO_YAML_CONF=0
DO_JSON_CONF=0
DO_JSON_STRING=0
DO_JSON_ENV=0
OPT_DEBUGMODE=""

while [ $# -ne 0 ]; do
	if [ -z "$1" ]; then
		break

	elif [ "$1" = "-h" ] || [ "$1" = "-H" ] || [ "$1" = "--help" ] || [ "$1" = "--HELP" ]; then
		func_usage "${PRGNAME}"
		exit 0
	elif [ "$1" = "ini_conf" ] || [ "$1" = "INI_CONF" ]; then
		if [ "${DO_INI_CONF}" -ne 0 ]; then
			PRNERR "Already specified \"ini_conf\", \"yaml_conf\", \"json_string\" or \"json_env\" option."
			exit 1
		fi
		DO_INI_CONF=1

	elif [ "$1" = "yaml_conf" ] || [ "$1" = "YAML_CONF" ]; then
		if [ "${DO_YAML_CONF}" -ne 0 ]; then
			PRNERR "Already specified \"ini_conf\", \"yaml_conf\", \"json_string\" or \"json_env\" option."
			exit 1
		fi
		DO_YAML_CONF=1

	elif [ "$1" = "json_conf" ] || [ "$1" = "JSON_CONF" ]; then
		if [ "${DO_JSON_CONF}" -ne 0 ]; then
			PRNERR "Already specified \"ini_conf\", \"yaml_conf\", \"json_string\" or \"json_env\" option."
			exit 1
		fi
		DO_JSON_CONF=1

	elif [ "$1" = "json_string" ] || [ "$1" = "JSON_STRING" ]; then
		if [ "${DO_JSON_STRING}" -ne 0 ]; then
			PRNERR "Already specified \"ini_conf\", \"yaml_conf\", \"json_string\" or \"json_env\" option."
			exit 1
		fi
		DO_JSON_STRING=1

	elif [ "$1" = "json_env" ] || [ "$1" = "JSON_ENV" ]; then
		if [ "${DO_JSON_ENV}" -ne 0 ]; then
			PRNERR "Already specified \"ini_conf\", \"yaml_conf\", \"json_string\" or \"json_env\" option."
			exit 1
		fi
		DO_JSON_ENV=1

	elif [ "$1" = "silent" ] || [ "$1" = "SILENT" ]; then
		if [ -n "${OPT_DEBUGMODE}" ]; then
			PRNERR "Already specified \"silent\", \"err\", \"wan\" or \"msg\" option."
			exit 1
		fi
		OPT_DEBUGMODE="silent"

	elif [ "$1" = "err" ] || [ "$1" = "ERR" ] || [ "$1" = "error" ] || [ "$1" = "ERROR" ]; then
		if [ -n "${OPT_DEBUGMODE}" ]; then
			PRNERR "Already specified \"silent\", \"err\", \"wan\" or \"msg\" option."
			exit 1
		fi
		OPT_DEBUGMODE="err"

	elif [ "$1" = "wan" ] || [ "$1" = "WAN" ] || [ "$1" = "warn" ] || [ "$1" = "WARN" ] || [ "$1" = "warning" ] || [ "$1" = "WARNING" ]; then
		if [ -n "${OPT_DEBUGMODE}" ]; then
			PRNERR "Already specified \"silent\", \"err\", \"wan\" or \"msg\" option."
			exit 1
		fi
		OPT_DEBUGMODE="wan"

	elif [ "$1" = "msg" ] || [ "$1" = "MSG" ] || [ "$1" = "message" ] || [ "$1" = "MESSAGE" ]; then
		if [ -n "${OPT_DEBUGMODE}" ]; then
			PRNERR "Already specified \"silent\", \"err\", \"wan\" or \"msg\" option."
			exit 1
		fi
		OPT_DEBUGMODE="msg"

	else
		PRNERR "Unknown option $1 specified."
		exit 1
	fi
	shift
done

#
# Check options
#
if [ "${DO_INI_CONF}" -eq 0 ] && [ "${DO_YAML_CONF}" -eq 0 ] && [ "${DO_JSON_CONF}" -eq 0 ] && [ "${DO_JSON_STRING}" -eq 0 ] && [ "${DO_JSON_ENV}" -eq 0 ]; then
	#
	# Default
	#
	DO_INI_CONF=1
fi

if [ -n "${OPT_DEBUGMODE}" ]; then
	DEBUGMODE="${OPT_DEBUGMODE}"
else
	DEBUGMODE="msg"
fi

#==============================================================
# Check system and Initialize
#==============================================================
PRNTITLE "Check system and Initialize"

if [ ! -c /dev/fuse ]; then
	# [NOTE]
	# This host does not have /dev/fuse, it maybe not allow privileged container.
	# Then this test will fail, thus we skip all test.
	#
	echo "${CYEL}----------------------------- ${CREV}[NOTICE]${CDEF}${CYEL} -----------------------------${CDEF}"	1>&2
	echo ""																											1>&2
	echo "                       ${CYEL}THIS TEST IS SKIPPED${CDEF}"												1>&2
	echo ""																											1>&2
	echo " To run tests with FUSE in a container, you need to set"													1>&2
	echo " \"--cap-add SYS_ADMIN --device /dev/fuse --security-opt apparmor:unconfined\"."							1>&2
	echo " If the test fails due to this permission, it prints"														1>&2
	echo " \"fuse: failed to open /dev/fuse: Operation not allowed\" in the log."									1>&2
	echo "${CYEL}---------------------------------------------------------------------${CDEF}"						1>&2

	exit 0
fi

#
# Check chmpx binary
#
if ! CHMPXBIN=$(command -v chmpx | tr -d '\n'); then
	PRNERR "Not found chmpx binary"
	exit 1
fi

#
# Check permission for fusermount, etc
#
# [NOTE]
# Don't raise an error here, even if it's in an inappropriate state.
# The test will continue, but may fail.
#
if ! command -v fusermount >/dev/null 2>&1; then
	PRNWARN "This test may fail if the fusermount command is not found or permission is not granted."
else
	#
	# Check device file
	#
	if [ ! -c /dev/fuse ]; then
		PRNWARN "This test may fail if /dev/fuse is not found or permissions are not allowed."
	fi

	#
	# Check common configuration file
	#
	if [ ! -f /etc/fuse.conf ]; then
		PRNINFO "Not found /etc/fuse.conf, try to set up by preinst script."

		# Setup
		if ! /bin/sh "${K2HFTFUSE_PREINST}" install; then
			PRNWARN "This test may fail because \"${K2HFTFUSE_PREINST} install\" failed to run."
		fi
	fi

	if [ -f /etc/fuse.conf ]; then
		if ! sed -e 's/#.*$//g' /etc/fuse.conf | grep -q 'user_allow_other'; then
			PRNINFO "Not found \"user_allow_other\" in /etc/fuse.conf, try to set up by preinst script."

			# setup
			if ! /bin/sh "${K2HFTFUSE_PREINST}" install; then
				PRNWARN "This test may fail because \"${K2HFTFUSE_PREINST} install\" failed to run."
			fi

			# re-check
			if ! sed -e 's/#.*$//g' /etc/fuse.conf | grep -q 'user_allow_other'; then
				PRNWARN "This test may fail because \"user_allow_other\" could not be set in /etc/fuse.conf."
			fi
		fi
	fi
fi

#
# Set Json string parameter
#
K2HFTFUSE_TEST_SERVER_JSON=$(grep 'K2HFTFUSE_TEST_SERVER_JSON=' "${CFG_JSON_STRING_DATA_FILE}" | sed -e 's/K2HFTFUSE_TEST_SERVER_JSON=//g')
K2HFTFUSE_TEST_SLAVE_JSON=$(grep 'K2HFTFUSE_TEST_SLAVE_JSON=' "${CFG_JSON_STRING_DATA_FILE}" | sed -e 's/K2HFTFUSE_TEST_SLAVE_JSON=//g')
K2HFTFUSE_TEST_SLAVE_JSON_FUSE=$(grep 'K2HFTFUSE_TEST_SLAVE_JSON=' "${CFG_JSON_STRING_DATA_FILE}" | sed -e 's/K2HFTFUSE_TEST_SLAVE_JSON=//g' -e 's/,/\\\\,/g')
K2HFTFUSE_TEST_TRANS_SERVER_JSON=$(grep 'K2HFTFUSE_TEST_TRANS_SERVER_JSON=' "${CFG_JSON_STRING_DATA_FILE}" | sed -e 's/K2HFTFUSE_TEST_TRANS_SERVER_JSON=//g')
K2HFTFUSESVR_TEST_TRANS_SERVER_JSON=$(grep 'K2HFTFUSESVR_TEST_TRANS_SERVER_JSON=' "${CFG_JSON_STRING_DATA_FILE}" | sed -e 's/K2HFTFUSESVR_TEST_TRANS_SERVER_JSON=//g')
K2HFTFUSESVR_TEST_TRANS_SLAVE_JSON=$(grep 'K2HFTFUSESVR_TEST_TRANS_SLAVE_JSON=' "${CFG_JSON_STRING_DATA_FILE}" | sed -e 's/K2HFTFUSESVR_TEST_TRANS_SLAVE_JSON=//g')

#
# Initializing mount point and log files
#
cleanup_all "${MOUNTDIR}"

#
# Print k2hftfuse version
#
PRNMSG "k2hftfuse version"
"${K2HFUSEBIN}" -v

#
# Change current directry
#
cd "${TESTDIR}" || exit 1

PRNSUCCEED "Check system and Initialize"

#==============================================================
# Start INI conf file
#==============================================================
if [ "${DO_INI_CONF}" -eq 1 ]; then
	#==========================================================
	# Test single server
	#==========================================================
	PRNTITLE "TEST FOR INI CONF FILE (SINGLE)"

	#------------------------------------------------------
	# RUN all test processes for single server
	#------------------------------------------------------
	if run_all_processes "single" "ini"; then
		TEST_PROCESS_RESULT=0
	else
		TEST_PROCESS_RESULT=1
	fi

	#------------------------------------------------------
	# RUN k2hdkclinetool process
	#------------------------------------------------------
	PRNMSG "RUN TEST PROCESS(k2hftfusetest)"

	if "${K2HFUSETESTBIN}" "${K2HFTFUSETEST_LOGFILE}" "${K2HFTFUSETEST_LOOP_COUNT}" "${K2HFTFUSETEST_OUTPUT_STRING}" > "${SINGLE_K2HFTFUSETEST_LOG}" 2>&1; then
		PRNINFO "SUCCEED : RUN TEST PROCESS(k2hftfusetest)"
		TEST_SUBRESULT=0
		sleep "${WAIT_SEC_AFTER_RUN_K2HFTFUSETEST}"
	else
		PRNERR "FAILED : RUN TEST PROCESS(k2hftfusetest)"
		TEST_SUBRESULT=1
	fi

	#------------------------------------------------------
	# Stop all processes
	#------------------------------------------------------
	PRNMSG "STOP ALL PROCESSES"

	# [NOTE]
	# The order of the list is the stop order and is important.
	#
	ALL_PIDS="	${K2HFTFUSEPID}
				${K2HFTFUSESVRPID}
				${CHMPXSLVPID}
				${CHMPXSVRPID}"

	if ! stop_processes "${MOUNTDIR}" "${ALL_PIDS}"; then
		PRNERR "FAILED : STOP ALL PROCESSES"
	else
		PRNINFO "SUCCEED : STOP ALL PROCESSES"
	fi

	#------------------------------------------------------
	# Check result
	#------------------------------------------------------
	PRNMSG "CHECK RESULT"

	#
	# Check line count
	#
	if [ ! -f "${K2HFTFUSESVR_UNITY_LOGFILE}" ]; then
		PRNERR "Not found ${K2HFTFUSESVR_UNITY_LOGFILE} file."
		TEST_SUBRESULT=1
	elif ! RESULT_LINE_COUNT=$(wc -l "${K2HFTFUSESVR_UNITY_LOGFILE}" | awk '{print $1}'); then
		PRNERR "Failed to get ${K2HFTFUSESVR_UNITY_LOGFILE} file line count."
		TEST_SUBRESULT=1
	elif [ -z "${RESULT_LINE_COUNT}" ] || [ "${RESULT_LINE_COUNT}" != "10" ]; then
		PRNERR "${K2HFTFUSESVR_UNITY_LOGFILE} file line count(${RESULT_LINE_COUNT}) is not as same as 10."
		TEST_SUBRESULT=1
	else
		PRNINFO "unify.log file line count(${RESULT_LINE_COUNT}) is as same as 10."
	fi

	#
	# Print all logs (if error)
	#
	if [ "${TEST_PROCESS_RESULT}" -ne 0 ] || [ "${TEST_SUBRESULT}" -ne 0 ]; then
		print_log_detail

		PRNERR "TEST FOR INI CONF FILE (SINGLE)"
		exit 1
	else
		PRNSUCCEED "TEST FOR INI CONF FILE (SINGLE)"
	fi

	#----------------------------------------------------------
	# Cleanup for after test processing
	#----------------------------------------------------------
	cleanup_all "${MOUNTDIR}"

	#==========================================================
	# Test transferred server
	#==========================================================
	PRNTITLE "TEST FOR INI CONF FILE (TRANSFERRED)"

	#------------------------------------------------------
	# RUN all test processes for transferred server
	#------------------------------------------------------
	if run_all_processes "transferred" "ini"; then
		TEST_PROCESS_RESULT=0
	else
		TEST_PROCESS_RESULT=1
	fi

	#------------------------------------------------------
	# RUN k2hdkclinetool process
	#------------------------------------------------------
	PRNMSG "RUN TEST PROCESS(k2hftfusetest)"

	if "${K2HFUSETESTBIN}" "${K2HFTFUSETEST_LOGFILE}" "${K2HFTFUSETEST_LOOP_COUNT}" "${K2HFTFUSETEST_OUTPUT_STRING}" > "${NEST_K2HFTFUSETEST_LOG}" 2>&1; then
		PRNINFO "SUCCEED : RUN TEST PROCESS(k2hftfusetest)"
		TEST_SUBRESULT=0
		sleep "${WAIT_SEC_AFTER_RUN_K2HFTFUSETEST}"
	else
		PRNERR "FAILED : RUN TEST PROCESS(k2hftfusetest)"
		TEST_SUBRESULT=1
	fi

	#------------------------------------------------------
	# Stop all processes
	#------------------------------------------------------
	PRNMSG "STOP ALL PROCESSES"

	# [NOTE]
	# The order of the list is the stop order and is important.
	#
	ALL_PIDS="	${K2HFTFUSEPID}
				${K2HFTFUSESVRPID}
				${K2HFTFUSESVRTRANSPID}
				${CHMPXSLVPID}
				${CHMPXSVRPID}
				${CHMPXTRANSSLVPID}
				${CHMPXTRANSSVRPID}"

	if ! stop_processes "${MOUNTDIR}" "${ALL_PIDS}"; then
		PRNERR "FAILED : STOP ALL PROCESSES"
	else
		PRNINFO "SUCCEED : STOP ALL PROCESSES"
	fi

	#------------------------------------------------------
	# Check result
	#------------------------------------------------------
	PRNMSG "CHECK RESULT"

	#
	# Check line count
	#
	if [ ! -f "${K2HFTFUSESVR_UNITY_LOGFILE}" ]; then
		PRNERR "Not found ${K2HFTFUSESVR_UNITY_LOGFILE} file."
		TEST_SUBRESULT=1
	elif ! RESULT_LINE_COUNT=$(wc -l "${K2HFTFUSESVR_UNITY_LOGFILE}" | awk '{print $1}'); then
		PRNERR "Failed to get ${K2HFTFUSESVR_UNITY_LOGFILE} file line count."
		TEST_SUBRESULT=1
	elif [ -z "${RESULT_LINE_COUNT}" ] || [ "${RESULT_LINE_COUNT}" != "10" ]; then
		PRNERR "${K2HFTFUSESVR_UNITY_LOGFILE} file line count(${RESULT_LINE_COUNT}) is not as same as 10."
		TEST_SUBRESULT=1
	else
		PRNINFO "unify.log file line count(${RESULT_LINE_COUNT}) is as same as 10."
	fi

	#
	# Print all logs (if error)
	#
	if [ "${TEST_PROCESS_RESULT}" -ne 0 ] || [ "${TEST_SUBRESULT}" -ne 0 ]; then
		print_log_detail

		PRNERR "TEST FOR INI CONF FILE (TRANSFERRED)"
		exit 1
	else
		PRNSUCCEED "TEST FOR INI CONF FILE (TRANSFERRED)"
	fi

	#----------------------------------------------------------
	# Cleanup for after test processing
	#----------------------------------------------------------
	cleanup_all "${MOUNTDIR}"
fi

#==============================================================
# Start YAML conf file
#==============================================================
if [ "${DO_YAML_CONF}" -eq 1 ]; then
	#==========================================================
	# Test single server
	#==========================================================
	PRNTITLE "TEST FOR YAML CONF FILE (SINGLE)"

	#------------------------------------------------------
	# RUN all test processes for single server
	#------------------------------------------------------
	if run_all_processes "single" "yaml"; then
		TEST_PROCESS_RESULT=0
	else
		TEST_PROCESS_RESULT=1
	fi

	#------------------------------------------------------
	# RUN k2hdkclinetool process
	#------------------------------------------------------
	PRNMSG "RUN TEST PROCESS(k2hftfusetest)"

	if "${K2HFUSETESTBIN}" "${K2HFTFUSETEST_LOGFILE}" "${K2HFTFUSETEST_LOOP_COUNT}" "${K2HFTFUSETEST_OUTPUT_STRING}" > "${SINGLE_K2HFTFUSETEST_LOG}" 2>&1; then
		PRNINFO "SUCCEED : RUN TEST PROCESS(k2hftfusetest)"
		TEST_SUBRESULT=0
		sleep "${WAIT_SEC_AFTER_RUN_K2HFTFUSETEST}"
	else
		PRNERR "FAILED : RUN TEST PROCESS(k2hftfusetest)"
		TEST_SUBRESULT=1
	fi

	#------------------------------------------------------
	# Stop all processes
	#------------------------------------------------------
	PRNMSG "STOP ALL PROCESSES"

	# [NOTE]
	# The order of the list is the stop order and is important.
	#
	ALL_PIDS="	${K2HFTFUSEPID}
				${K2HFTFUSESVRPID}
				${CHMPXSLVPID}
				${CHMPXSVRPID}"

	if ! stop_processes "${MOUNTDIR}" "${ALL_PIDS}"; then
		PRNERR "FAILED : STOP ALL PROCESSES"
	else
		PRNINFO "SUCCEED : STOP ALL PROCESSES"
	fi

	#------------------------------------------------------
	# Check result
	#------------------------------------------------------
	PRNMSG "CHECK RESULT"

	#
	# Check line count
	#
	if [ ! -f "${K2HFTFUSESVR_UNITY_LOGFILE}" ]; then
		PRNERR "Not found ${K2HFTFUSESVR_UNITY_LOGFILE} file."
		TEST_SUBRESULT=1
	elif ! RESULT_LINE_COUNT=$(wc -l "${K2HFTFUSESVR_UNITY_LOGFILE}" | awk '{print $1}'); then
		PRNERR "Failed to get ${K2HFTFUSESVR_UNITY_LOGFILE} file line count."
		TEST_SUBRESULT=1
	elif [ -z "${RESULT_LINE_COUNT}" ] || [ "${RESULT_LINE_COUNT}" != "10" ]; then
		PRNERR "${K2HFTFUSESVR_UNITY_LOGFILE} file line count(${RESULT_LINE_COUNT}) is not as same as 10."
		TEST_SUBRESULT=1
	else
		PRNINFO "unify.log file line count(${RESULT_LINE_COUNT}) is as same as 10."
	fi

	#
	# Print all logs (if error)
	#
	if [ "${TEST_PROCESS_RESULT}" -ne 0 ] || [ "${TEST_SUBRESULT}" -ne 0 ]; then
		print_log_detail

		PRNERR "TEST FOR YAML CONF FILE (SINGLE)"
		exit 1
	else
		PRNSUCCEED "TEST FOR YAML CONF FILE (SINGLE)"
	fi

	#----------------------------------------------------------
	# Cleanup for after test processing
	#----------------------------------------------------------
	cleanup_all "${MOUNTDIR}"

	#==========================================================
	# Test transferred server
	#==========================================================
	PRNTITLE "TEST FOR YAML CONF FILE (TRANSFERRED)"

	#------------------------------------------------------
	# RUN all test processes for transferred server
	#------------------------------------------------------
	if run_all_processes "transferred" "yaml"; then
		TEST_PROCESS_RESULT=0
	else
		TEST_PROCESS_RESULT=1
	fi

	#------------------------------------------------------
	# RUN k2hdkclinetool process
	#------------------------------------------------------
	PRNMSG "RUN TEST PROCESS(k2hftfusetest)"

	if "${K2HFUSETESTBIN}" "${K2HFTFUSETEST_LOGFILE}" "${K2HFTFUSETEST_LOOP_COUNT}" "${K2HFTFUSETEST_OUTPUT_STRING}" > "${NEST_K2HFTFUSETEST_LOG}" 2>&1; then
		PRNINFO "SUCCEED : RUN TEST PROCESS(k2hftfusetest)"
		TEST_SUBRESULT=0
		sleep "${WAIT_SEC_AFTER_RUN_K2HFTFUSETEST}"
	else
		PRNERR "FAILED : RUN TEST PROCESS(k2hftfusetest)"
		TEST_SUBRESULT=1
	fi

	#------------------------------------------------------
	# Stop all processes
	#------------------------------------------------------
	PRNMSG "STOP ALL PROCESSES"

	# [NOTE]
	# The order of the list is the stop order and is important.
	#
	ALL_PIDS="	${K2HFTFUSEPID}
				${K2HFTFUSESVRPID}
				${K2HFTFUSESVRTRANSPID}
				${CHMPXSLVPID}
				${CHMPXSVRPID}
				${CHMPXTRANSSLVPID}
				${CHMPXTRANSSVRPID}"

	if ! stop_processes "${MOUNTDIR}" "${ALL_PIDS}"; then
		PRNERR "FAILED : STOP ALL PROCESSES"
	else
		PRNINFO "SUCCEED : STOP ALL PROCESSES"
	fi

	#------------------------------------------------------
	# Check result
	#------------------------------------------------------
	PRNMSG "CHECK RESULT"

	#
	# Check line count
	#
	if ! RESULT_LINE_COUNT=$(grep -c "${K2HFTFUSETEST_LOGFILE_RELPATH}" "${K2HFTFUSESVR_UNITY_LOGFILE_NAME}"); then
		PRNERR "Failed to get ${K2HFTFUSESVR_UNITY_LOGFILE_NAME} file line count."
		TEST_SUBRESULT=1
	elif [ -z "${RESULT_LINE_COUNT}" ] || [ "${RESULT_LINE_COUNT}" != "10" ]; then
		PRNERR "${K2HFTFUSESVR_UNITY_LOGFILE_NAME} file line count(${RESULT_LINE_COUNT}) is not as same as 10."
		TEST_SUBRESULT=1
	else
		PRNINFO "${K2HFTFUSESVR_UNITY_LOGFILE_NAME} file line count(${RESULT_LINE_COUNT}) is as same as 10."
	fi

	#
	# Print all logs (if error)
	#
	if [ "${TEST_PROCESS_RESULT}" -ne 0 ] || [ "${TEST_SUBRESULT}" -ne 0 ]; then
		print_log_detail

		PRNERR "TEST FOR YAML CONF FILE (TRANSFERRED)"
		exit 1
	else
		PRNSUCCEED "TEST FOR YAML CONF FILE (TRANSFERRED)"
	fi

	#----------------------------------------------------------
	# Cleanup for after test processing
	#----------------------------------------------------------
	cleanup_all "${MOUNTDIR}"
fi

#==============================================================
# Start JSON conf file
#==============================================================
if [ "${DO_JSON_CONF}" -eq 1 ]; then
	#==========================================================
	# Test single server
	#==========================================================
	PRNTITLE "TEST FOR JSON CONF FILE (SINGLE)"

	#------------------------------------------------------
	# RUN all test processes for single server
	#------------------------------------------------------
	if run_all_processes "single" "json"; then
		TEST_PROCESS_RESULT=0
	else
		TEST_PROCESS_RESULT=1
	fi

	#------------------------------------------------------
	# RUN k2hdkclinetool process
	#------------------------------------------------------
	PRNMSG "RUN TEST PROCESS(k2hftfusetest)"

	if "${K2HFUSETESTBIN}" "${K2HFTFUSETEST_LOGFILE}" "${K2HFTFUSETEST_LOOP_COUNT}" "${K2HFTFUSETEST_OUTPUT_STRING}" > "${SINGLE_K2HFTFUSETEST_LOG}" 2>&1; then
		PRNINFO "SUCCEED : RUN TEST PROCESS(k2hftfusetest)"
		TEST_SUBRESULT=0
		sleep "${WAIT_SEC_AFTER_RUN_K2HFTFUSETEST}"
	else
		PRNERR "FAILED : RUN TEST PROCESS(k2hftfusetest)"
		TEST_SUBRESULT=1
	fi

	#------------------------------------------------------
	# Stop all processes
	#------------------------------------------------------
	PRNMSG "STOP ALL PROCESSES"

	# [NOTE]
	# The order of the list is the stop order and is important.
	#
	ALL_PIDS="	${K2HFTFUSEPID}
				${K2HFTFUSESVRPID}
				${CHMPXSLVPID}
				${CHMPXSVRPID}"

	if ! stop_processes "${MOUNTDIR}" "${ALL_PIDS}"; then
		PRNERR "FAILED : STOP ALL PROCESSES"
	else
		PRNINFO "SUCCEED : STOP ALL PROCESSES"
	fi

	#------------------------------------------------------
	# Check result
	#------------------------------------------------------
	PRNMSG "CHECK RESULT"

	#
	# Check line count
	#
	if ! RESULT_LINE_COUNT=$(grep -c "${K2HFTFUSETEST_LOGFILE_RELPATH}" "${K2HFTFUSESVR_UNITY_LOGFILE_NAME}"); then
		PRNERR "Failed to get ${K2HFTFUSESVR_UNITY_LOGFILE_NAME} file line count."
		TEST_SUBRESULT=1
	elif [ -z "${RESULT_LINE_COUNT}" ] || [ "${RESULT_LINE_COUNT}" != "10" ]; then
		PRNERR "${K2HFTFUSESVR_UNITY_LOGFILE_NAME} file line count(${RESULT_LINE_COUNT}) is not as same as 10."
		TEST_SUBRESULT=1
	else
		PRNINFO "${K2HFTFUSESVR_UNITY_LOGFILE_NAME} file line count(${RESULT_LINE_COUNT}) is as same as 10."
	fi

	#
	# Print all logs (if error)
	#
	if [ "${TEST_PROCESS_RESULT}" -ne 0 ] || [ "${TEST_SUBRESULT}" -ne 0 ]; then
		print_log_detail

		PRNERR "TEST FOR JSON CONF FILE (SINGLE)"
		exit 1
	else
		PRNSUCCEED "TEST FOR JSON CONF FILE (SINGLE)"
	fi

	#----------------------------------------------------------
	# Cleanup for after test processing
	#----------------------------------------------------------
	cleanup_all "${MOUNTDIR}"

	#==========================================================
	# Test transferred server
	#==========================================================
	PRNTITLE "TEST FOR JSON CONF FILE (TRANSFERRED)"

	#------------------------------------------------------
	# RUN all test processes for transferred server
	#------------------------------------------------------
	if run_all_processes "transferred" "json"; then
		TEST_PROCESS_RESULT=0
	else
		TEST_PROCESS_RESULT=1
	fi

	#------------------------------------------------------
	# RUN k2hdkclinetool process
	#------------------------------------------------------
	PRNMSG "RUN TEST PROCESS(k2hftfusetest)"

	if "${K2HFUSETESTBIN}" "${K2HFTFUSETEST_LOGFILE}" "${K2HFTFUSETEST_LOOP_COUNT}" "${K2HFTFUSETEST_OUTPUT_STRING}" > "${NEST_K2HFTFUSETEST_LOG}" 2>&1; then
		PRNINFO "SUCCEED : RUN TEST PROCESS(k2hftfusetest)"
		TEST_SUBRESULT=0
		sleep "${WAIT_SEC_AFTER_RUN_K2HFTFUSETEST}"
	else
		PRNERR "FAILED : RUN TEST PROCESS(k2hftfusetest)"
		TEST_SUBRESULT=1
	fi

	#------------------------------------------------------
	# Stop all processes
	#------------------------------------------------------
	PRNMSG "STOP ALL PROCESSES"

	# [NOTE]
	# The order of the list is the stop order and is important.
	#
	ALL_PIDS="	${K2HFTFUSEPID}
				${K2HFTFUSESVRPID}
				${K2HFTFUSESVRTRANSPID}
				${CHMPXSLVPID}
				${CHMPXSVRPID}
				${CHMPXTRANSSLVPID}
				${CHMPXTRANSSVRPID}"

	if ! stop_processes "${MOUNTDIR}" "${ALL_PIDS}"; then
		PRNERR "FAILED : STOP ALL PROCESSES"
	else
		PRNINFO "SUCCEED : STOP ALL PROCESSES"
	fi

	#------------------------------------------------------
	# Check result
	#------------------------------------------------------
	PRNMSG "CHECK RESULT"

	#
	# Check line count
	#
	if ! RESULT_LINE_COUNT=$(grep -c "${K2HFTFUSETEST_LOGFILE_RELPATH}" "${K2HFTFUSESVR_UNITY_LOGFILE_NAME}"); then
		PRNERR "Failed to get ${K2HFTFUSESVR_UNITY_LOGFILE_NAME} file line count."
		TEST_SUBRESULT=1
	elif [ -z "${RESULT_LINE_COUNT}" ] || [ "${RESULT_LINE_COUNT}" != "10" ]; then
		PRNERR "${K2HFTFUSESVR_UNITY_LOGFILE_NAME} file line count(${RESULT_LINE_COUNT}) is not as same as 10."
		TEST_SUBRESULT=1
	else
		PRNINFO "${K2HFTFUSESVR_UNITY_LOGFILE_NAME} file line count(${RESULT_LINE_COUNT}) is as same as 10."
	fi

	#
	# Print all logs (if error)
	#
	if [ "${TEST_PROCESS_RESULT}" -ne 0 ] || [ "${TEST_SUBRESULT}" -ne 0 ]; then
		print_log_detail

		PRNERR "TEST FOR JSON CONF FILE (TRANSFERRED)"
		exit 1
	else
		PRNSUCCEED "TEST FOR JSON CONF FILE (TRANSFERRED)"
	fi

	#----------------------------------------------------------
	# Cleanup for after test processing
	#----------------------------------------------------------
	cleanup_all "${MOUNTDIR}"
fi

#==============================================================
# Start JSON String conf file
#==============================================================
if [ "${DO_JSON_STRING}" -eq 1 ]; then
	#==========================================================
	# Test single server
	#==========================================================
	PRNTITLE "TEST FOR JSON STRING (SINGLE)"

	#------------------------------------------------------
	# RUN all test processes for single server
	#------------------------------------------------------
	if run_all_processes "single" "jsonstring"; then
		TEST_PROCESS_RESULT=0
	else
		TEST_PROCESS_RESULT=1
	fi

	#------------------------------------------------------
	# RUN k2hdkclinetool process
	#------------------------------------------------------
	PRNMSG "RUN TEST PROCESS(k2hftfusetest)"

	if "${K2HFUSETESTBIN}" "${K2HFTFUSETEST_LOGFILE}" "${K2HFTFUSETEST_LOOP_COUNT}" "${K2HFTFUSETEST_OUTPUT_STRING}" > "${SINGLE_K2HFTFUSETEST_LOG}" 2>&1; then
		PRNINFO "SUCCEED : RUN TEST PROCESS(k2hftfusetest)"
		TEST_SUBRESULT=0
		sleep "${WAIT_SEC_AFTER_RUN_K2HFTFUSETEST}"
	else
		PRNERR "FAILED : RUN TEST PROCESS(k2hftfusetest)"
		TEST_SUBRESULT=1
	fi

	#------------------------------------------------------
	# Stop all processes
	#------------------------------------------------------
	PRNMSG "STOP ALL PROCESSES"

	# [NOTE]
	# The order of the list is the stop order and is important.
	#
	ALL_PIDS="	${K2HFTFUSEPID}
				${K2HFTFUSESVRPID}
				${CHMPXSLVPID}
				${CHMPXSVRPID}"

	if ! stop_processes "${MOUNTDIR}" "${ALL_PIDS}"; then
		PRNERR "FAILED : STOP ALL PROCESSES"
	else
		PRNINFO "SUCCEED : STOP ALL PROCESSES"
	fi

	#------------------------------------------------------
	# Check result
	#------------------------------------------------------
	PRNMSG "CHECK RESULT"

	#
	# Check line count
	#
	if ! RESULT_LINE_COUNT=$(grep -c "${K2HFTFUSETEST_LOGFILE_RELPATH}" "${K2HFTFUSESVR_UNITY_LOGFILE_NAME}"); then
		PRNERR "Failed to get ${K2HFTFUSESVR_UNITY_LOGFILE_NAME} file line count."
		TEST_SUBRESULT=1
	elif [ -z "${RESULT_LINE_COUNT}" ] || [ "${RESULT_LINE_COUNT}" != "10" ]; then
		PRNERR "${K2HFTFUSESVR_UNITY_LOGFILE_NAME} file line count(${RESULT_LINE_COUNT}) is not as same as 10."
		TEST_SUBRESULT=1
	else
		PRNINFO "${K2HFTFUSESVR_UNITY_LOGFILE_NAME} file line count(${RESULT_LINE_COUNT}) is as same as 10."
	fi

	#
	# Print all logs (if error)
	#
	if [ "${TEST_PROCESS_RESULT}" -ne 0 ] || [ "${TEST_SUBRESULT}" -ne 0 ]; then
		print_log_detail

		PRNERR "TEST FOR JSON STRING (SINGLE)"
		exit 1
	else
		PRNSUCCEED "TEST FOR JSON STRING (SINGLE)"
	fi

	#----------------------------------------------------------
	# Cleanup for after test processing
	#----------------------------------------------------------
	cleanup_all "${MOUNTDIR}"

	#==========================================================
	# Test transferred server
	#==========================================================
	PRNTITLE "TEST FOR JSON STRING (TRANSFERRED)"

	#------------------------------------------------------
	# RUN all test processes for transferred server
	#------------------------------------------------------
	if run_all_processes "transferred" "jsonstring"; then
		TEST_PROCESS_RESULT=0
	else
		TEST_PROCESS_RESULT=1
	fi

	#------------------------------------------------------
	# RUN k2hdkclinetool process
	#------------------------------------------------------
	PRNMSG "RUN TEST PROCESS(k2hftfusetest)"

	if "${K2HFUSETESTBIN}" "${K2HFTFUSETEST_LOGFILE}" "${K2HFTFUSETEST_LOOP_COUNT}" "${K2HFTFUSETEST_OUTPUT_STRING}" > "${NEST_K2HFTFUSETEST_LOG}" 2>&1; then
		PRNINFO "SUCCEED : RUN TEST PROCESS(k2hftfusetest)"
		TEST_SUBRESULT=0
		sleep "${WAIT_SEC_AFTER_RUN_K2HFTFUSETEST}"
	else
		PRNERR "FAILED : RUN TEST PROCESS(k2hftfusetest)"
		TEST_SUBRESULT=1
	fi

	#------------------------------------------------------
	# Stop all processes
	#------------------------------------------------------
	PRNMSG "STOP ALL PROCESSES"

	# [NOTE]
	# The order of the list is the stop order and is important.
	#
	ALL_PIDS="	${K2HFTFUSEPID}
				${K2HFTFUSESVRPID}
				${K2HFTFUSESVRTRANSPID}
				${CHMPXSLVPID}
				${CHMPXSVRPID}
				${CHMPXTRANSSLVPID}
				${CHMPXTRANSSVRPID}"

	if ! stop_processes "${MOUNTDIR}" "${ALL_PIDS}"; then
		PRNERR "FAILED : STOP ALL PROCESSES"
	else
		PRNINFO "SUCCEED : STOP ALL PROCESSES"
	fi

	#------------------------------------------------------
	# Check result
	#------------------------------------------------------
	PRNMSG "CHECK RESULT"

	#
	# Check line count
	#
	if ! RESULT_LINE_COUNT=$(grep -c "${K2HFTFUSETEST_LOGFILE_RELPATH}" "${K2HFTFUSESVR_UNITY_LOGFILE_NAME}"); then
		PRNERR "Failed to get ${K2HFTFUSESVR_UNITY_LOGFILE_NAME} file line count."
		TEST_SUBRESULT=1
	elif [ -z "${RESULT_LINE_COUNT}" ] || [ "${RESULT_LINE_COUNT}" != "10" ]; then
		PRNERR "${K2HFTFUSESVR_UNITY_LOGFILE_NAME} file line count(${RESULT_LINE_COUNT}) is not as same as 10."
		TEST_SUBRESULT=1
	else
		PRNINFO "${K2HFTFUSESVR_UNITY_LOGFILE_NAME} file line count(${RESULT_LINE_COUNT}) is as same as 10."
	fi

	#
	# Print all logs (if error)
	#
	if [ "${TEST_PROCESS_RESULT}" -ne 0 ] || [ "${TEST_SUBRESULT}" -ne 0 ]; then
		print_log_detail

		PRNERR "TEST FOR JSON STRING (TRANSFERRED)"
		exit 1
	else
		PRNSUCCEED "TEST FOR JSON STRING (TRANSFERRED)"
	fi

	#----------------------------------------------------------
	# Cleanup for after test processing
	#----------------------------------------------------------
	cleanup_all "${MOUNTDIR}"
fi

#==============================================================
# Start JSON Environment conf file
#==============================================================
if [ "${DO_JSON_ENV}" -eq 1 ]; then
	#==========================================================
	# Test single server
	#==========================================================
	PRNTITLE "TEST FOR JSON ENVIRONMENT (SINGLE)"

	#------------------------------------------------------
	# RUN all test processes for single server
	#------------------------------------------------------
	if run_all_processes "single" "jsonenv"; then
		TEST_PROCESS_RESULT=0
	else
		TEST_PROCESS_RESULT=1
	fi

	#------------------------------------------------------
	# RUN k2hdkclinetool process
	#------------------------------------------------------
	PRNMSG "RUN TEST PROCESS(k2hftfusetest)"

	if "${K2HFUSETESTBIN}" "${K2HFTFUSETEST_LOGFILE}" "${K2HFTFUSETEST_LOOP_COUNT}" "${K2HFTFUSETEST_OUTPUT_STRING}" > "${SINGLE_K2HFTFUSETEST_LOG}" 2>&1; then
		PRNINFO "SUCCEED : RUN TEST PROCESS(k2hftfusetest)"
		TEST_SUBRESULT=0
		sleep "${WAIT_SEC_AFTER_RUN_K2HFTFUSETEST}"
	else
		PRNERR "FAILED : RUN TEST PROCESS(k2hftfusetest)"
		TEST_SUBRESULT=1
	fi

	#------------------------------------------------------
	# Stop all processes
	#------------------------------------------------------
	PRNMSG "STOP ALL PROCESSES"

	# [NOTE]
	# The order of the list is the stop order and is important.
	#
	ALL_PIDS="	${K2HFTFUSEPID}
				${K2HFTFUSESVRPID}
				${CHMPXSLVPID}
				${CHMPXSVRPID}"

	if ! stop_processes "${MOUNTDIR}" "${ALL_PIDS}"; then
		PRNERR "FAILED : STOP ALL PROCESSES"
	else
		PRNINFO "SUCCEED : STOP ALL PROCESSES"
	fi

	#------------------------------------------------------
	# Check result
	#------------------------------------------------------
	PRNMSG "CHECK RESULT"

	#
	# Check line count
	#
	if ! RESULT_LINE_COUNT=$(grep -c "${K2HFTFUSETEST_LOGFILE_RELPATH}" "${K2HFTFUSESVR_UNITY_LOGFILE_NAME}"); then
		PRNERR "Failed to get ${K2HFTFUSESVR_UNITY_LOGFILE_NAME} file line count."
		TEST_SUBRESULT=1
	elif [ -z "${RESULT_LINE_COUNT}" ] || [ "${RESULT_LINE_COUNT}" != "10" ]; then
		PRNERR "${K2HFTFUSESVR_UNITY_LOGFILE_NAME} file line count(${RESULT_LINE_COUNT}) is not as same as 10."
		TEST_SUBRESULT=1
	else
		PRNINFO "${K2HFTFUSESVR_UNITY_LOGFILE_NAME} file line count(${RESULT_LINE_COUNT}) is as same as 10."
	fi

	#
	# Print all logs (if error)
	#
	if [ "${TEST_PROCESS_RESULT}" -ne 0 ] || [ "${TEST_SUBRESULT}" -ne 0 ]; then
		print_log_detail

		PRNERR "TEST FOR JSON ENVIRONMENT (SINGLE)"
		exit 1
	else
		PRNSUCCEED "TEST FOR JSON ENVIRONMENT (SINGLE)"
	fi

	#----------------------------------------------------------
	# Cleanup for after test processing
	#----------------------------------------------------------
	cleanup_all "${MOUNTDIR}"

	#==========================================================
	# Test transferred server
	#==========================================================
	PRNTITLE "TEST FOR JSON ENVIRONMENT (TRANSFERRED)"

	#------------------------------------------------------
	# RUN all test processes for transferred server
	#------------------------------------------------------
	if run_all_processes "transferred" "jsonenv"; then
		TEST_PROCESS_RESULT=0
	else
		TEST_PROCESS_RESULT=1
	fi

	#------------------------------------------------------
	# RUN k2hdkclinetool process
	#------------------------------------------------------
	PRNMSG "RUN TEST PROCESS(k2hftfusetest)"

	if "${K2HFUSETESTBIN}" "${K2HFTFUSETEST_LOGFILE}" "${K2HFTFUSETEST_LOOP_COUNT}" "${K2HFTFUSETEST_OUTPUT_STRING}" > "${NEST_K2HFTFUSETEST_LOG}" 2>&1; then
		PRNINFO "SUCCEED : RUN TEST PROCESS(k2hftfusetest)"
		TEST_SUBRESULT=0
		sleep "${WAIT_SEC_AFTER_RUN_K2HFTFUSETEST}"
	else
		PRNERR "FAILED : RUN TEST PROCESS(k2hftfusetest)"
		TEST_SUBRESULT=1
	fi

	#------------------------------------------------------
	# Stop all processes
	#------------------------------------------------------
	PRNMSG "STOP ALL PROCESSES"

	# [NOTE]
	# The order of the list is the stop order and is important.
	#
	ALL_PIDS="	${K2HFTFUSEPID}
				${K2HFTFUSESVRPID}
				${K2HFTFUSESVRTRANSPID}
				${CHMPXSLVPID}
				${CHMPXSVRPID}
				${CHMPXTRANSSLVPID}
				${CHMPXTRANSSVRPID}"

	if ! stop_processes "${MOUNTDIR}" "${ALL_PIDS}"; then
		PRNERR "FAILED : STOP ALL PROCESSES"
	else
		PRNINFO "SUCCEED : STOP ALL PROCESSES"
	fi

	#------------------------------------------------------
	# Check result
	#------------------------------------------------------
	PRNMSG "CHECK RESULT"

	#
	# Check line count
	#
	if ! RESULT_LINE_COUNT=$(grep -c "${K2HFTFUSETEST_LOGFILE_RELPATH}" "${K2HFTFUSESVR_UNITY_LOGFILE_NAME}"); then
		PRNERR "Failed to get ${K2HFTFUSESVR_UNITY_LOGFILE_NAME} file line count."
		TEST_SUBRESULT=1
	elif [ -z "${RESULT_LINE_COUNT}" ] || [ "${RESULT_LINE_COUNT}" != "10" ]; then
		PRNERR "${K2HFTFUSESVR_UNITY_LOGFILE_NAME} file line count(${RESULT_LINE_COUNT}) is not as same as 10."
		TEST_SUBRESULT=1
	else
		PRNINFO "${K2HFTFUSESVR_UNITY_LOGFILE_NAME} file line count(${RESULT_LINE_COUNT}) is as same as 10."
	fi

	#
	# Print all logs (if error)
	#
	if [ "${TEST_PROCESS_RESULT}" -ne 0 ] || [ "${TEST_SUBRESULT}" -ne 0 ]; then
		print_log_detail

		PRNERR "TEST FOR JSON ENVIRONMENT (TRANSFERRED)"
		exit 1
	else
		PRNSUCCEED "TEST FOR JSON ENVIRONMENT (TRANSFERRED)"
	fi

	#----------------------------------------------------------
	# Cleanup for after test processing
	#----------------------------------------------------------
	cleanup_all "${MOUNTDIR}"
fi

#==============================================================
# SUMMARY
#==============================================================
FINISH_DATE=$(date)

echo ""
echo "=============================================================="
echo "[SUMMAY LOG] ${START_DATE} -> ${FINISH_DATE}"
echo "--------------------------------------------------------------"

PRNSUCCEED "ALL TEST"

exit 0

#
# Local variables:
# tab-width: 4
# c-basic-offset: 4
# End:
# vim600: noexpandtab sw=4 ts=4 fdm=marker
# vim<600: noexpandtab sw=4 ts=4
#
