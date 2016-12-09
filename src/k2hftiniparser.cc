/*
 * k2hftfuse for file transaction by FUSE-based file system
 * 
 * Copyright 2015 Yahoo! JAPAN corporation.
 * 
 * k2hftfuse is file transaction system on FUSE file system with
 * K2HASH and K2HASH TRANSACTION PLUGIN, CHMPX.
 * 
 * For the full copyright and license information, please view
 * the LICENSE file that was distributed with this source code.
 *
 * AUTHOR:   Takeshi Nakatani
 * CREATE:   Thu Sep 17 2015
 * REVISION:
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <chmpx/chmconfutil.h>

#include <fstream>

#include "k2hftcommon.h"
#include "k2hftiniparser.h"
#include "k2hftutil.h"
#include "k2hftdbg.h"

using namespace std;

//---------------------------------------------------------
// Utilities for INI file
//---------------------------------------------------------
bool read_ini_file_contents(const char* path, strlst_t& lines, strlst_t& files)
{
	if(K2HFT_ISEMPTYSTR(path)){
		ERR_K2HFTPRN("path is empty.");
		return false;
	}

	// check file path
	string	realpath;
	if(!check_path_real_path(path, realpath)){
		ERR_K2HFTPRN("Could not convert realpath from file(%s)", path);
		return false;
	}
	// is already listed
	for(strlst_t::const_iterator iter = files.begin(); iter != files.end(); ++iter){
		if(realpath == (*iter)){
			ERR_K2HFTPRN("file(%s:%s) is already loaded.", path, realpath.c_str());
			return false;
		}
	}
	files.push_back(realpath);

	// load
	ifstream	cfgstream(realpath.c_str(), ios::in);
	if(!cfgstream.good()){
		ERR_K2HFTPRN("Could not open(read only) file(%s:%s)", path, realpath.c_str());
		return false;
	}

	string		line;
	int			lineno;
	for(lineno = 1; cfgstream.good() && getline(cfgstream, line); lineno++){
		line = trim(line);
		if(0 == line.length()){
			continue;
		}
		if(INI_COMMENT_CHAR == line[0]){
			continue;
		}

		// check only include
		string::size_type	pos;
		if(string::npos != (pos = line.find(INI_INCLUDE_STR))){
			string	value	= trim(line.substr(pos + 1));
			string	key		= trim(line.substr(0, pos));
			if(key == INI_INCLUDE_STR){
				// found include, do reentrant
				if(!read_ini_file_contents(value.c_str(), lines, files)){
					ERR_K2HFTPRN("Failed to load include file(%s)", value.c_str());
					cfgstream.close();
					return false;
				}
				continue;
			}
		}
		// add
		lines.push_back(line);
	}
	cfgstream.close();

	return true;
}

void parse_ini_line(const string& line, string& key, string& value)
{
	string::size_type	pos;
	if(string::npos != (pos = line.find(INI_KV_SEP_CHAR))){
		key		= trim(line.substr(0, pos));
		value	= trim(line.substr(pos + 1));
	}else{
		key		= trim(line);
		value	= "";
	}
	if(!extract_conf_value(key) || !extract_conf_value(value)){
		key.clear();
		value.clear();
	}else{
		key		= upper(key);								// key convert to upper
	}
}

void parse_ini_value(const string& value, strlst_t& values)
{
	values.clear();

	if(value.empty()){
		// nothing in value
		return;

	}else if(INI_VALUE_AREA_CHAR != value[0]){
		// value does not start (") charactor, it could not parse any.
		values.push_back(value);

	}else{
		string				tempstack("");
		string::size_type	pos;
		for(string tempval = value; 0 < tempval.length(); ){
			if(string::npos != (pos = tempval.find(INI_VALUE_SEP_CHAR))){
				// found "," charactor
				string	temptrimval = rtrim(tempval.substr(0, pos));
				if(0 < temptrimval.length() && INI_VALUE_AREA_CHAR == temptrimval[temptrimval.length() - 1]){
					// string end charactor before "," is ("), it is OK.
					tempstack	+= temptrimval;
					values.push_back(tempstack);
					tempstack	= "";

					// lest string
					tempval = ltrim(tempval.substr(pos + 1));
					if(0 < tempval.length() && INI_VALUE_AREA_CHAR != tempval[0]){
						// lest string does not start (") charactor.
						values.push_back(tempval);
						tempval = "";
					}

				}else{
					// string end charactor before "," is not ("), so skip these string(stack)
					tempstack	+= tempval.substr(0, pos + 1);
					tempval		=  tempval.substr(pos + 1);
				}
			}else{
				// not found "," charactor
				tempstack	+= tempval;
				tempval		= "";
				values.push_back(tempstack);
				tempstack	= "";
			}
		}
	}
}

/*
 * VIM modelines
 *
 * vim:set ts=4 fenc=utf-8:
 */
