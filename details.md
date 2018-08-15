---
layout: contents
language: en-us
title: Details
short_desc: File Transaction by FUSE based file system
lang_opp_file: detailsja.html
lang_opp_word: To Japanese
prev_url: feature.html
prev_string: Feature
top_url: index.html
top_string: TOP
next_url: usage.html
next_string: Usage
---

# Details

## K2HFTFUSE program
There are two ways to start the K2HFTFUSE program as follows.

### 1. using mount command
Use the mount command to mount the virtual file system provided by K2HFTFUSE on the system.  
For the mount command, a entry of K2HFTFUSE program is required in /etc/fstab file.

### 2. run manually
Start up the virtual file system provided by K2HFTFUSE manually.  
Specify the required command option and start the K2HFTFUSE program.  
K2HFTFUSE program can be launched with two types of startup in the background(daemon) and the foreground.

### Options
The startup option of the K2HFTFUSE program inherits the same startup option (-o <option> = <option value>) as the mount command.

#### -h(--help)
display help for the optsions of K2HFTFUSE program
#### -v(--version)
display version of K2HFTFUSE program
#### -d(--debug)
It is an option defined by libfuse and outputs FUSE debug messages.  
If you specify this option and do not specify the dbglevel option, K2HFTFUSE will output messages with the same level as dbglevel = msg.
#### -f
Start K2HFTFUSE in the foreground.  
If this option is not specified, start up in the background (start up as a daemon).
#### -o umask=<number>
Specify the umask under the mount point. Please refer to man mount page.
#### -o uid=<number>
Specify the uid under the mount point. Please refer to man mount page.
#### -o gid=<number>
Specify the gid under the mount point. Please refer to man mount page.
#### -o dbglevel={err|wan|msg}
Specify the output level of K2HFTFUSE message. Levels can be silent, err (error), wan (warning), msg (message).  
If K2HFTFUSE is started in the background, it is output as syslog.
#### -o conf=<configration file>
Specify the configuration file(formatted by INI, YAML, JSON). This option is exclusive with the json option.
#### -o json=<json string>
Specify the configuration by string of JSON. This option is exclusive with the conf option.
#### -o enable_run_chmpx
You need to start [CHMPX](https://chmpx.antpick.ax/) before launching the K2HFTUSE program.  
CHMPX is a communication middleware program, which is necessary program for K2HFTFUSE.  
If this option is specified, the K2HFTFUSE program will start the CHMPX program before initializing itself.  
Therefore, you do not need to manually start the CHMPX program.
#### -o disable_run_chmpx
If this option is specified, the CHMPX program will not be started.  
If enable_run_chmpx and disable_run_chmpx options are not specified, disable_run_chmpx is the default.
#### -o chmpxlog=<log file path>
If the enable_run_chmpx option is specified, you can instruct the CHMPX program message to be output to a file.  
When the enable_run_chmpx option is specified and this option is not specified, the CHMPX program message is output to /dev/null.

### How to start
The method for starting K2HFTFUSE manually is shown below.
```
$ k2hftfuse [mount point] [k2hftfuse or fuse options]
```
An example of activation is shown below.
```
$ k2hftfuse /mnt/k2hfs -o allow_other -o nodev -o nosuid -o _netdev -o dbglevel=err -o conf=/etc/k2hftfuse.conf -f -d
```
or
```
$ k2hftfuse /mnt/k2hfs -o allow_other,nodev,nosuid,_netdev,dbglevel=err,conf=/etc/k2hftfuse.conf -f -d
```
When launching K2HFTFUSE from the mount command, you need to prepare an entry in /etc/fstab.  
(For details of fstab, you can see man mount/man fstab.)

An example of activation is shown below.
#### /etc/fstab
```
k2hftfuse /mnt/k2hfs fuse allow_other,nodev,nosuid,_netdev,dbglevel=err,conf=/etc/k2hftfuse.conf 0 0
```
#### mount command
```
$ mount /mnt/k2hfs
```

If you start K2HFTFUSE in the background, use the umount command or fusermount to exit as follows.
```
$ umount /mnt/k2hfs
```
or
```
$ fusermount -d /mnt/k2hfs
```

### K2HFTFUSE Environemnts
The K2HFTFUSE program can load the following environment variables.
#### K2HFTCONFFILE
as same as conf option.
#### K2HFTJSONCONF
as same as json option.

_If you do not specify both -o conf and -o json option, K2HFTFUSE checks K2HFTCONFFILE or K2HFTJSONCONF environments._  
_If there is not any option and environment for configuration, you can not run K2HFTFUSE program with error._

## K2HFTFUSESVR program
The K2HFTFUSESVR program is a standard program that receives data sent from K2HFTFUSE.  
K2HFTFUSESVR is prepared as a standard generic K2HFTUFSE receiver program.  
_You can create your own program to receive data sent from K2HFTUFSE._  
_Perhaps the source code of K2HFTFUSESVR will be helpful._

### How to start
The method for starting K2HFTFUSESVR is shown below.
```
k2hftfusesvr [options]
```

An example of activation is shown below.
```
$ k2hftfusesvr -conf /etc/k2hftfusesvr.conf
```

### Options
The startup option of the K2HFTFUSESVR program.

#### -h(help)
display help for the optsions of K2HFTFUSESVR program
#### -v(version)
display version of K2HFTFUSESVR program
#### -conf <configration file>
Specify the configuration file(formatted by INI, YAML, JSON). This option is exclusive with the -json option.
#### -json <json string>
Specify the configuration by string of JSON. This option is exclusive with the -conf option.
#### -d(g) {err|wan|msg|silent}
Specify the level of output message. The level value is silent, err, wan, msg. as default silent.

### K2HFTFUSESVR Environemnts
#### K2HFTSVRCONFFILE
as same as -conf option.
#### K2HFTSVRJSONCONF
as same as -json option.

_If you do not specify both -o conf and -o json option, K2HFTFUSESVR checks K2HFTSVRCONFFILE or K2HFTSVRJSONCONF environments._  
_If there is not any option and environment for configuration, you can not run K2HFTFUSESVR program with error._


## Configuration

### K2HFTFUSE Configuration

#### K2HFTFUSE Section(\[K2HFTFUSE\] is used for INI file)
##### K2HTYPE
Specify the type of [K2HASH](https://k2hash.antpick.ax/) created by the K2HASH library (KVS library) used internally by K2HFTFUSE.  
For the value, specify mem (memory type) / file (file type) / TEMP (temporary file type).  
The default is mem (memory) type.
##### K2HFILE
When K2HTYPE is specified as FILE or TEMP, specify the file path.
##### K2HFULLMAP
Specify the mapping type of the K2HASH file used internally by K2HFTFUSE.  
Specify whether the data mapping type is all (= yes) or index only (= no).  
If omitted, it will be all areas (= yes).  
If K2HTYPE is MEM, this item is ignored.
##### K2HINIT
When K2HTYPE is file, specify whether or not to initialize the K2HASH file. (In the case of mem/temp, it is always initialized and this item is ignored.)
##### K2HMASKBIT
Specify the setting value of K2HASH. As default 8. (See [K2HASH](https://k2hash.antpick.ax/) )
##### K2HCMASKBIT
Specify the setting value of K2HASH. As default 4. (See [K2HASH](https://k2hash.antpick.ax/) )
##### K2HMAXELE
Specify the setting value of K2HASH. As default 8. (See [K2HASH](https://k2hash.antpick.ax/) )
##### K2HPAGESIZE
Specify the setting value of K2HASH. As default 128. (See [K2HASH](https://k2hash.antpick.ax/) )
##### DTORTHREADCNT
Specify the number of threads that K2HFTFUSE assigns to output processing for transferring data.  
It is 1 if omitted. (Please refer to the explanation of K2HASH)
##### DTORCTP
Specify the program for output processing for K2HFTFUSE to transfer data.  
This program requires a shared library of the K2HASH transaction program.  
For details on how to make your own K2HASH transaction program, see [K2HTPDTOR](https://k2htp_dtor.antpick.ax/).  
If omitted, K2HTPDTOR (k2htpdtor.so) is used.
##### BINTRANS
K2HFTFUSE specifies the type of data to be transferred.  
If the delimiter is a text file with line feed code, specify no.  
For binary data with no data delimiter, specify yes.  
If this item is yes, you need to flush the file after writing data to the K2HFTFUSE virtual file system.  
Data is transferred by flushing. (See also BYTELIMIT)  
If omitted, it is no.
##### EXPIRE
K2HFTFUSE queues data in case of communication failure or data overflow.  
This queued data is transferred sequentially as FIFO after resuming.  
When queued in large quantities, depending on the system, it may be meaningless data transfer due to expiration.  
K2HFTFUSE can automatically discard such data.  
In this item, you can set the expiration date (seconds) of queued data.  
Data that expires after the expiration date is automatically discarded.  
Possible values are positive numbers of 0 or more, 0 has special meaning.  
If 0 is specified, it will not be discarded. The default is 0.
##### TRANSLINECNT
Specify whether to transfer the data to be transferred one by one, or to transfer multiple data one at a time.  
Specify it together with the TRANSTIMEUP item.  
One unit is the data for one line in the case of a text file.  
In the case of binary data, it is the data until it is flushed. (See BINTRANS)  
K2HFTFUSE accumulates up to the specified number of units, and transmits accumulated data by one transfer.  
If it is omitted, it is 0, data is transferred without accumulation.
##### TRANSTIMEUP
When TRANSLINECNT is specified, data to be transferred is accumulated.  
When the time (seconds) specified in this item elapses, accumulated data is forcibly transferred.  
If omitted, it is 0, and data is transferred without accumulating even if TRANSLINECNT is specified as 1 or more.
##### BYTELIMIT
Specify the maximum number of bytes of data to be transferred.  
Data exceeding this value will not be transferred.  
If omitted, it is 0 and there is no upper limit byte number.

#### K2HFTFUSE_RULE_DIR Section(\[K2HFTFUSE_RULE_DIR\] is used for INI file)
This section specifies the rule when there is a write to the file in the specified directory (the relative path from the mount point) under the virtual file system path mounted by K2HFTFUSE.  
Please specify the following items as an array in the K2HFTFUSE_RULE_DIR section.  
For except INI formatted file, this section is set in K2HFTFUSE section.  
Only the INI formatted file is special, multiple \[K2HFTFUSE_RULE_DIR\] sections can be described in one configuration file.

##### TARGET
Specify the target directory (relative path from the mount point).  
If omitted, it will be empty and point to the mount point itself.
##### TRUNS
Specify yes to transfer all writes to files below the specified directory. Specify no when not transferring.  
If omitted, it is no.
##### OUTPUTFILE
Writing to the file under the specified directory is copied to the file under the directory (absolute path specified) specified in this item.  
If omitted, it will not be copied.
##### PLUGIN
If there is a write to the file under the specified directory, the data is passed to the external program specified in this item.  
The program specified in this item is started as a child process of K2HFTFUSE, and the data is delivered to that stdin.  
If omitted, do not do anything.
##### DEFAULTALL
When pattern matching is performed with the value set in ALLOW / DENY, specify whether to enable (ALLOW) or not (DENY) the default.  
If omitted, it is DENY.

##### ALLOW
Specify a regular expression to perform pattern matching of written data.  
In case of matching, transfer and copying will be permitted.  
If DEFAULTALL is set to ALLOW, this item is ignored.  
The specified value can be set to one or two values.  
The first value is a regular expression to match.  
For the second value, specify the processed data format when matching with regular expressions.
- In case of INI format file  
Please surround the value of this item with "".  
To specify two values, specify the first and second with "," separating them.
- In cases other than INI format  
Please specify the value of this item in array representation.  
In case of only one, it should be an array of only one value.

##### DENY
Write a regular expression that performs pattern matching of the written data.  
In case of matching, transfer or copying is not permitted.  
If DEFAULTALL is set to DENY, this item is ignored.  
The value is a regular expression.
- In case of INI format file  
Please surround the value of this item with "".
- In cases other than INI format  
The value of this item is specified as an array representation, and the contents of the array are only one. (It has the same format as ALLOW)

#### K2HFTFUSE_RULE Section(\[K2HFTFUSE_RULE\] is used for INI file)
This section specifies the rule when there is a write to the specified file (the relative path from the mount point) under the virtual file system path mounted by K2HFTFUSE.  
If the file path under the directory path specified by K2HFTFUSE_RULE_DIR is specified, the setting specified by K2HFTFUSE_RULE takes precedence.  
Please specify the following items as an array in the K2HFTFUSE_RULE section.  
For except INI formatted file, this section is set in K2HFTFUSE section.  
Only the INI formatted file is special, multiple \[K2HFTFUSE_RULE\] sections can be described in one configuration file.

##### TARGET
Specify the target file (relative path from the mount point).  
You must specify this item.
##### TRUNS
Specify yes to transfer all writes to files below the specified directory. Specify no when not transferring.  
If omitted, it is no.
##### OUTPUTFILE
Writing to the file under the specified directory is copied to the file under the directory (absolute path specified) specified in this item.  
If omitted, it will not be copied.
##### PLUGIN
If there is a write to the file under the specified directory, the data is passed to the external program specified in this item.  
The program specified in this item is started as a child process of K2HFTFUSE, and the data is delivered to that stdin.  
If omitted, do not do anything.
##### DEFAULTALL
as same as K2HFTFUSE_RULE_DIR section.
##### ALLOW
as same as K2HFTFUSE_RULE_DIR section.
##### DENY
as same as K2HFTFUSE_RULE_DIR section.


#### Section for CHMPX
The configuration of K2HFTFUSE includes the configuration of CHMPX.  
It is also possible to separate the configuration of K2HFTFUSE and CHMPX.  
However, since management becomes difficult, describe both in almost the same configuration.  
There are three sections of CHMPX, GLOBAL, SVRNODE, and SLVNODE. (In INI formatted file, \[GLOBAL\], \[SVRNODE\], \[SLVNODE\])  
For details on these, please refer to CHMPX description.

#### Section for K2HTPDTOR
If you do not specify the DTORCTP item in the K2HFTFUSE section, K2HTPDTOR is started as a process for transfer.  
The section used by this K2HTPDTOR program is K2HTPDTOR (\[K2HTPDTOR\] in INI format) section.  
This section is also included in the configuration of K2HFTFUSE.  
For details on this setting, refer to CHMPX description.


### K2HFTFUSESVR Configuration
K2HFTFUSESVR Describes the items of the configuration (file or JSON character string) to be passed to the program.

#### K2HFTFUSESVR Section(\[K2HFTFUSESVR\] is used for INI file)
##### TYPE
Specify the operation type of K2HFTFUSESVR.  
The types that can be specified are transfer (trans), file output (file), transfer and file output (both).  
Transfer (trans) is used to operate as a relay host.  
The file output (file) is used when it becomes the termination host of the transfer.  
If both is specified, it operates as a relay host that also performs file transfer while also performing transfer.
##### FILE_BASEDIR
Specify the base directory of the output file when outputting files.  
According to the file path of the file to which the transfer data is written, the file is created with relative path from the mount point by K2HFTFUSE.  
Then, the transfer data is written to that file.  
However, if the FILE_UNIFY item is specified, it is always output as one file as an exception.  
This item is mandatory when TYPE item is file or both.
##### FILE_UNIFY
When outputting files, specify this when outputting all transfer data in one file.  
The file path specified in this item is relative to the directory specified by FILE_BASEDIR.
##### FILE_TIMEFORM
The data transferred from K2HFTFUSE includes the time stamp at the time when it was written as attached information.  
When outputting files, this time stamp information can be output.  
When outputting timestamp information, you can specify that format in the same format as the strftime function.  
The time stamp information of K2 HFTFUSE has information of ns which can not be expressed by strftime.  
To output ns information, use the notation "%-" which does not exist in the strftime format.  
If omitted, it is "", and nothing is output even if "%T" is specified in the FORMAT item.
##### PLUGIN
Specify the path of the external program for processing the received data independently.  
The specified external program is started as a child process of K2HFTFUSESVR, and data is passed to that stdin.  
If omitted, the external program will not be called.
##### FORMAT
When file or both is specified in TYPE, data is written to the specified file.  
K2HFTFUSE's transfer data includes attribute information (host name, process(thread) ID, file path, write time).  
When K2HFTFUSESVR writes data to a file, it can format and output it in its own format.  
It can also output attribute information.  
This item specifies this format.  
The format can be specified with the following keywords.
| Format | detail             |
|:-------|:-------------------|
| %H	 | host name          |
| %P	 | process(thread) ID |
| %F	 | file path          |
| %f	 | file name          |
| %T	 | time stamp(The time format is specified by the FILE_TIMEFORM item) |
| %L	 | data               |
_If omitted, it will be "%L" and only the received data will be output._
##### TRANSCONF
When trans or both is specified as the TYPE item, specify the configuration of the CHMPX program activated for transfer.  
This item is mandatory when trans or both is specified as the TYPE item.  
(However, when specifying CHMPX configuration with environment variable, it is possible to omit it, see [CHMPX](https://chmpx.antpick.ax/) for details)
##### K2HTYPE
This item is a required item when trans or both is specified as the TYPE item.  
Specify the type of K2HASH used internally by K2HFTFUSESVR.  
For the value, specify mem(memory type) / file(file type) / TEMP(temporary file type).
##### K2HFILE
When K2HTYPE is specified as FILE or TEMP, specify the file path.
##### K2HFULLMAP
Specify the mapping type of the K2HASH file used internally by K2HFTFUSE.  
Specify whether the data mapping type is all (= yes) or index only (= no).  
If omitted, it will be all areas (= yes).  
If K2HTYPE is MEM, this item is ignored.
##### K2HINIT
When K2HTYPE is file, specify whether or not to initialize the K2HASH file. (In the case of mem/temp, it is always initialized and this item is ignored.)
##### K2HMASKBIT
Specify the setting value of K2HASH. As default 8. (See [K2HASH](https://k2hash.antpick.ax/) )
##### K2HCMASKBIT
Specify the setting value of K2HASH. As default 4. (See [K2HASH](https://k2hash.antpick.ax/) )
##### K2HMAXELE
Specify the setting value of K2HASH. As default 8. (See [K2HASH](https://k2hash.antpick.ax/) )
##### K2HPAGESIZE
Specify the setting value of K2HASH. As default 128. (See [K2HASH](https://k2hash.antpick.ax/) )
##### DTORTHREADCNT
Specify the number of threads that K2HFTFUSE assigns to output processing for transferring data.  
It is 1 if omitted. (Please refer to the explanation of K2HASH)
##### DTORCTP
Specify the program for output processing for K2HFTFUSE to transfer data.  
This program requires a shared library of the K2HASH transaction program.  
For details on how to make your own K2HASH transaction program, see [K2HTPDTOR](https://k2htp_dtor.antpick.ax/).  
If omitted, K2HTPDTOR (k2htpdtor.so) is used.
