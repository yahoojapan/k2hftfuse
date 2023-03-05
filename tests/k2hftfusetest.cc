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
 * CREATE:   Thu Sep 24 2015
 * REVISION:
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <string>

using namespace std;

//---------------------------------------------------------
// Utility
//---------------------------------------------------------
static void k2hftclient_usage(void)
{
	fprintf(stdout,	"[Usage] k2hftfusetest <filepath> <count> <output string>\n"
					"\n"
					"        filepath        output file path\n"
					"        count           output count\n"
					"        output string   output string\n"
					"\n");
}

static bool parse_parameter(int argc, char** argv, string& filepath, int& count, string& output)
{
	if(argc < 4 || !argv){
		fprintf(stderr, "[ERROR] Parameters are wrong.\n");
		return false;
	}
	// parse
	filepath= argv[1];
	count	= atoi(argv[2]);
	output	= "";
	for(int cnt = 3; cnt < argc; ++cnt){
		if(!output.empty()){
			output += ' ';
		}
		output += argv[cnt];
	}
	// check
	if(count <= 0){
		fprintf(stderr, "[ERROR] count parameter is wrong(count=%d).\n", count);
		return false;
	}
	if(output.empty()){
		fprintf(stderr, "[ERROR] output parameter is empty.\n");
		return false;
	}
	return true;
}

//---------------------------------------------------------
// Main
//---------------------------------------------------------
int main(int argc, char** argv)
{
	string	filepath("");
	string	output("");
	int		count = 0;
	if(!parse_parameter(argc, argv, filepath, count, output)){
		k2hftclient_usage();
		exit(EXIT_FAILURE);
	}

	// buffer
	char*	pBuff;
	if(NULL == (pBuff = reinterpret_cast<char*>(malloc(output.length() + 32)))){
		fprintf(stderr, "[ERROR] could not allocate memory.\n");
		exit(EXIT_FAILURE);
	}

	// writing(with open/close)
	for(int cnt = 0; cnt < count; ++cnt){
		// open output file
		FILE*	fp = fopen(filepath.c_str(), "a");
		if(!fp){
			fprintf(stderr, "[ERROR] could not open file(%s)\n", filepath.c_str());
			free(pBuff);
			exit(EXIT_FAILURE);
		}

		sprintf(pBuff, "%s(%d)\n", output.c_str(), cnt + 1);
        fwrite(pBuff, sizeof(char), strlen(pBuff), fp);				// no check...(this is test)
		fflush(fp);
		fclose(fp);
		sleep(1);
	}

	free(pBuff);
	exit(EXIT_SUCCESS);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noexpandtab sw=4 ts=4 fdm=marker
 * vim<600: noexpandtab sw=4 ts=4
 */
