/*
 * k2hftfuse for file transaction by FUSE-based file system
 * 
 * Copyright 2015 Yahoo Japan Corporation.
 * 
 * k2hftfuse is file transaction system on FUSE file system with
 * K2HASH and K2HASH TRANSACTION PLUGIN, CHMPX.
 * 
 * For the full copyright and license information, please view
 * the license file that was distributed with this source code.
 *
 * AUTHOR:   Takeshi Nakatani
 * CREATE:   Thu Sep 17 2015
 * REVISION:
 *
 */

#ifndef	K2HFTINIPARSER_H
#define K2HFTINIPARSER_H

#include <string>

#include "k2hftcommon.h"
#include "k2hftutil.h"

//---------------------------------------------------------
// Symbols( for common ini file )
//---------------------------------------------------------
#define	INI_INCLUDE_STR					"INCLUDE"
#define	INI_KV_SEP_CHAR					'='
#define	INI_VALUE_AREA_CHAR				'\"'
#define	INI_VALUE_SEP_CHAR				','
#define	INI_COMMENT_CHAR				'#'
#define	INI_SEC_START_CHAR				'['
#define	INI_SEC_END_CHAR				']'

// Value words for all section
#define	INI_K2HFT_YES1_VAL_STR			"Y"
#define	INI_K2HFT_YES2_VAL_STR			"YES"
#define	INI_K2HFT_NO1_VAL_STR			"N"
#define	INI_K2HFT_NO2_VAL_STR			"NO"
#define	INI_K2HFT_ON_VAL_STR			"ON"
#define	INI_K2HFT_OFF_VAL_STR			"OFF"

//---------------------------------------------------------
// Symbols( for slave ini file )
//---------------------------------------------------------
#define	K2HFT_MAIN_SEC_STR				"K2HFTFUSE"
#define	K2HFT_RULEDIR_SEC_STR			"K2HFTFUSE_RULE_DIR"
#define	K2HFT_RULE_SEC_STR				"K2HFTFUSE_RULE"
#define	INI_K2HFT_MAIN_SEC_STR			"[" K2HFT_MAIN_SEC_STR "]"
#define	INI_K2HFT_RULEDIR_SEC_STR		"[" K2HFT_RULEDIR_SEC_STR "]"
#define	INI_K2HFT_RULE_SEC_STR			"[" K2HFT_RULE_SEC_STR "]"

// Key words for K2HFTFUSE section
#define	INI_K2HFT_K2HTYPE_STR			"K2HTYPE"
#define	INI_K2HFT_K2HFILE_STR			"K2HFILE"
#define	INI_K2HFT_K2HFULLMAP_STR		"K2HFULLMAP"
#define	INI_K2HFT_K2HINIT_STR			"K2HINIT"
#define	INI_K2HFT_K2HMASKBIT_STR		"K2HMASKBIT"
#define	INI_K2HFT_K2HCMASKBIT_STR		"K2HCMASKBIT"
#define	INI_K2HFT_K2HMAXELE_STR			"K2HMAXELE"
#define	INI_K2HFT_K2HPAGESIZE_STR		"K2HPAGESIZE"
#define	INI_K2HFT_DTORTHREADCNT_STR		"DTORTHREADCNT"
#define	INI_K2HFT_DTORCTP_STR			"DTORCTP"
#define	INI_K2HFT_BINTRANS_STR			"BINTRANS"
#define	INI_K2HFT_EXPIRE_STR			"EXPIRE"
#define	INI_K2HFT_TRANSLINECNT_STR		"TRANSLINECNT"
#define	INI_K2HFT_TRANSTIMEUP_STR		"TRANSTIMEUP"
#define	INI_K2HFT_BYTELIMIT_STR			"BYTELIMIT"

// Key words for K2HFTFUSE_RULE_DIR and K2HFTFUSE_RULE section
#define	INI_K2HFT_TARGET_STR			"TARGET"
#define	INI_K2HFT_TRUNS_STR				"TRUNS"
#define	INI_K2HFT_OUTPUTFILE_STR		"OUTPUTFILE"
#define	INI_K2HFT_PLUGIN_STR			"PLUGIN"
#define	INI_K2HFT_DEFAULTALL_STR		"DEFAULTALL"
#define	INI_K2HFT_ALLOW_STR				"ALLOW"
#define	INI_K2HFT_DENY_STR				"DENY"

// Value words for K2HFTFUSE section
#define	INI_K2HFT_MEM1_VAL_STR			"M"
#define	INI_K2HFT_MEM2_VAL_STR			"MEM"
#define	INI_K2HFT_MEM3_VAL_STR			"MEMORY"
#define	INI_K2HFT_FILE1_VAL_STR			"F"
#define	INI_K2HFT_FILE2_VAL_STR			"FILE"
#define	INI_K2HFT_TEMP1_VAL_STR			"T"
#define	INI_K2HFT_TEMP2_VAL_STR			"TEMP"
#define	INI_K2HFT_TEMP3_VAL_STR			"TEMPORARY"

// Value words for K2HFTFUSE_RULE_DIR and K2HFTFUSE_RULE section
#define	INI_K2HFT_ALLOW_VAL_STR			"ALLOW"
#define	INI_K2HFT_DENY_VAL_STR			"DENY"

// Maximum replace placement number for regex
#define	INI_K2HFT_MAX_REPLACE_NUMBER	9

//---------------------------------------------------------
// Symbols( for server ini file )
//---------------------------------------------------------
#define	K2HFTSVR_MAIN_SEC_STR			"K2HFTFUSESVR"
#define	INI_K2HFTSVR_MAIN_SEC_STR		"[" K2HFTSVR_MAIN_SEC_STR "]"

// Key words for K2HFTFUSESVR section
#define	INI_K2HFTSVR_TYPE_STR			"TYPE"
#define	INI_K2HFTSVR_FILE_BASEDIR_STR	"FILE_BASEDIR"
#define	INI_K2HFTSVR_FILE_UNIFY_STR		"FILE_UNIFY"
#define	INI_K2HFTSVR_FILE_TIMEFORM_STR	"FILE_TIMEFORM"
#define	INI_K2HFTSVR_PLUGIN_STR			INI_K2HFT_PLUGIN_STR
#define	INI_K2HFTSVR_FORMAT_STR			"FORMAT"
#define	INI_K2HFTSVR_BINTRANS_STR		INI_K2HFT_BINTRANS_STR

#define	INI_K2HFTSVR_TRANSCONF_STR		"TRANSCONF"
#define	INI_K2HFTSVR_K2HTYPE_STR		INI_K2HFT_K2HTYPE_STR
#define	INI_K2HFTSVR_K2HFILE_STR		INI_K2HFT_K2HFILE_STR
#define	INI_K2HFTSVR_K2HFULLMAP_STR		INI_K2HFT_K2HFULLMAP_STR
#define	INI_K2HFTSVR_K2HINIT_STR		INI_K2HFT_K2HINIT_STR
#define	INI_K2HFTSVR_K2HMASKBIT_STR		INI_K2HFT_K2HMASKBIT_STR
#define	INI_K2HFTSVR_K2HCMASKBIT_STR	INI_K2HFT_K2HCMASKBIT_STR
#define	INI_K2HFTSVR_K2HMAXELE_STR		INI_K2HFT_K2HMAXELE_STR
#define	INI_K2HFTSVR_K2HPAGESIZE_STR	INI_K2HFT_K2HPAGESIZE_STR
#define	INI_K2HFTSVR_DTORTHREADCNT_STR	INI_K2HFT_DTORTHREADCNT_STR
#define	INI_K2HFTSVR_DTORCTP_STR		INI_K2HFT_DTORCTP_STR

// Value words for K2HFTFUSESVR section
#define	INI_K2HFTSVR_FILE1_VAL_STR		"F"
#define	INI_K2HFTSVR_FILE2_VAL_STR		"FILE"
#define	INI_K2HFTSVR_TRANS1_VAL_STR		"T"
#define	INI_K2HFTSVR_TRANS2_VAL_STR		"TRANS"
#define	INI_K2HFTSVR_TRANS3_VAL_STR		"TRANSFER"
#define	INI_K2HFTSVR_BOTH1_VAL_STR		"B"
#define	INI_K2HFTSVR_BOTH2_VAL_STR		"BOTH"

//---------------------------------------------------------
// Utilities for INI file
//---------------------------------------------------------
bool read_ini_file_contents(const char* path, strlst_t& lines, strlst_t& files);
void parse_ini_line(const std::string& line, std::string& key, std::string& value);
void parse_ini_value(const std::string& value, strlst_t& values);

#endif	// K2HFTINIPARSER_H

/*
 * VIM modelines
 *
 * vim:set ts=4 fenc=utf-8:
 */
