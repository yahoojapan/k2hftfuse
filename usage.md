---
layout: contents
language: en-us
title: Usage
short_desc: File Transaction by FUSE based file system
lang_opp_file: usageja.html
lang_opp_word: To Japanese
prev_url: details.html
prev_string: Details
top_url: index.html
top_string: TOP
next_url: build.html
next_string: Build
---

# Usage
After building K2HFTFUSE, you can check the operation by the following procedure.

## Sample Configuration
The following is the configuration used for K2HFTFUSE and K2HFTFUSESVR test. please refer.

### Case not relaying

#### Source host
##### K2HFTFUSE
- Configuration file formatted by INI  
[k2hftfuse_test_slave.ini]({{ site.github.repository_url }}/blob/master/tests/k2hftfuse_test_slave.ini)
- Configuration file formatted by YAML  
[k2hftfuse_test_slave.yaml]({{ site.github.repository_url }}/blob/master/tests/k2hftfuse_test_slave.yaml)
- Configuration file formatted by JSON  
[k2hftfuse_test_slave.json]({{ site.github.repository_url }}/blob/master/tests/k2hftfuse_test_slave.json)
- Configuration by string of JSON  
The character string after "K2HFTFUSE_TEST_SLAVE_JSON=" in the [test_json_string.data]({{ site.github.repository_url }}/blob/master/tests/test_json_string.data) file

##### CHMPX
It is the same configuration as K2HFTFUSE, and you need to start CHMPX before K2HFTFUSE starting.

#### Terminating host
##### K2HFTFUSESVR
- Configuration file formatted by INI  
[k2hftfuse_test_server.ini]({{ site.github.repository_url }}/blob/master/tests/k2hftfuse_test_server.ini)
- Configuration file formatted by YAML  
[k2hftfuse_test_server.yaml]({{ site.github.repository_url }}/blob/master/tests/k2hftfuse_test_server.yaml)
- Configuration file formatted by JSON  
[k2hftfuse_test_server.json]({{ site.github.repository_url }}/blob/master/tests/k2hftfuse_test_server.json)
- Configuration by string of JSON  
The character string after "K2HFTFUSE_TEST_SLAVE_JSON=" in the [test_json_string.data]({{ site.github.repository_url }}/blob/master/tests/test_json_string.data) file

##### CHMPX
It is the same configuration as K2HFTFUSESVR, and you need to start CHMPX before K2HFTFUSESVR starting.

### Case relaying

#### Source host
##### K2HFTFUSE
- Configuration file formatted by INI  
[k2hftfuse_test_slave.ini]({{ site.github.repository_url }}/blob/master/tests/k2hftfuse_test_slave.ini)
- Configuration file formatted by YAML  
[k2hftfuse_test_slave.yaml]({{ site.github.repository_url }}/blob/master/tests/k2hftfuse_test_slave.yaml)
- Configuration file formatted by JSON  
[k2hftfuse_test_slave.json]({{ site.github.repository_url }}/blob/master/tests/k2hftfuse_test_slave.json)
- Configuration by string of JSON  
The character string after "K2HFTFUSE_TEST_SLAVE_JSON=" in the [test_json_string.data]({{ site.github.repository_url }}/blob/master/tests/test_json_string.data) file

##### CHMPX
It is the same configuration as K2HFTFUSE, and you need to start CHMPX before K2HFTFUSE starting.

#### Relay host
##### K2HFTFUSESVR
- Configuration file formatted by INI  
[k2hftfuse_test_trans_server.ini]({{ site.github.repository_url }}/blob/master/tests/k2hftfuse_test_trans_server.ini)
- Configuration file formatted by YAML  
[k2hftfuse_test_trans_server.yaml]({{ site.github.repository_url }}/blob/master/tests/k2hftfuse_test_trans_server.yaml)
- Configuration file formatted by JSON  
[k2hftfuse_test_trans_server.json]({{ site.github.repository_url }}/blob/master/tests/k2hftfuse_test_trans_server.json)
- Configuration by string of JSON  
The character string after "K2HFTFUSE_TEST_TRANS_SERVER_JSON=" in the [test_json_string.data]({{ site.github.repository_url }}/blob/master/tests/test_json_string.data) file

##### CHMPX srever node to connect from source host
It is the same configuration as K2HFTFUSESVR, and you need to start CHMPX before K2HFTFUSESVR starting.

##### CHMPX slave node to connect to terminating host
- Configuration file formatted by INI  
[k2hftfusesvr_test_trans_slave.ini]({{ site.github.repository_url }}/blob/master/tests/k2hftfusesvr_test_trans_slave.ini)
- Configuration file formatted by YAML  
[k2hftfusesvr_test_trans_slave.yaml]({{ site.github.repository_url }}/blob/master/tests/k2hftfusesvr_test_trans_slave.yaml)
- Configuration file formatted by JSON  
[k2hftfusesvr_test_trans_slave.json]({{ site.github.repository_url }}/blob/master/tests/k2hftfusesvr_test_trans_slave.json)
- Configuration by string of JSON  
The character string after "K2HFTFUSESVR_TEST_TRANS_SLAVE_JSON=" in the [test_json_string.data]({{ site.github.repository_url }}/blob/master/tests/test_json_string.data) file

#### Terminating host
##### K2HFTFUSESVR
- Configuration file formatted by INI  
[k2hftfusesvr_test_trans_server.ini]({{ site.github.repository_url }}/blob/master/tests/k2hftfusesvr_test_trans_server.ini)
- Configuration file formatted by YAML  
[k2hftfusesvr_test_trans_server.yaml]({{ site.github.repository_url }}/blob/master/tests/k2hftfusesvr_test_trans_server.yaml)
- Configuration file formatted by JSON  
[k2hftfusesvr_test_trans_server.json]({{ site.github.repository_url }}/blob/master/tests/k2hftfusesvr_test_trans_server.json)
- Configuration by string of JSON  
The character string after "K2HFTFUSESVR_TEST_TRANS_SERVER_JSON=" in the [test_json_string.data]({{ site.github.repository_url }}/blob/master/tests/test_json_string.data) file

##### CHMPX
It is the same configuration as K2HFTFUSESVR, and you need to start CHMPX before K2HFTFUSESVR starting.

## Operation check
I will try to transfer the contents of the actual log file.  
In this example, we will transfer the Apache log file with K2HFTFUSE.

### Precondition
10 hosts(foo\[0-9\].yahoo.co.jp) provide web services in the Apache program.  
The access.log and error.log output by the Apache program are output to the /home/apache/logs directory of each host.  
Files other than these are not output to this directory.  
And we will aggregate log files on one server bar.yahoo.co.jp.

### Configuration

- This is the configuration file used by Apache host.  
Prepare the following slave.yaml (YAML format) file.

```
#
# CHMPX GLOBAL SETTING
#
GLOBAL:
    {
        FILEVERSION:            1,
        DATE:                   "Thu, 01 Dec 2016 00:00:00 +0900",
        GROUP:                  K2HFUSETEST,
        MODE:                   SLAVE,
        DELIVERMODE:            random,
        MAXCHMPX:               32,
        REPLICA:                0,
        MAXMQSERVER:            8,
        MAXMQCLIENT:            8,
        MQPERATTACH:            1,
        MAXQPERSERVERMQ:        2,
        MAXQPERCLIENTMQ:        8,
        MAXMQPERCLIENT:         4,
        PORT:                   18020,
        CTLPORT:                18022,
        SELFCTLPORT:            18022,
        RWTIMEOUT:              100000,
        RETRYCNT:               1000,
        CONTIMEOUT:             500000,
        MQRWTIMEOUT:            50,
        MQRETRYCNT:             100000,
        DOMERGE:                on,
        SSL:                    no,
        K2HFULLMAP:             on,
        K2HMASKBIT:             4,
        K2HCMASKBIT:            4,
        K2HMAXELE:              2
    }

#
# CHMPX SERVER NODES SECTION
#
SVRNODE:
    [
        {
            NAME:               foo[0-9].yahoo.co.jp,
            PORT:               8020,
            CTLPORT:            8021,
            SSL:                no
        }
    ]

#
# CHMPX SLAVE NODES SECTION
#
SLVNODE:
    [
        {
            NAME:               "[.]*",
            CTLPORT:            8022
        }
    ]

#
# K2HTPDTOR(K2HASH TRANSACTION PLUGIN)
#
K2HTPDTOR:
    {
        K2HTPDTOR_BROADCAST:    no,
    }

#
# K2HFTFUSE
#
# K2HTYPE       type of k2hash used by transfer mode
# K2HFILE       file path of k2hash used by transfer mode, when file type
# K2HFULLMAP    mapping type of k2hash used by transfer mode, when file type
# K2HINIT       initializing of k2hash used by transfer mode, when file type
# K2HMASKBIT    init mask bit count of k2hash used by transfer mode, when file type
# K2HCMASKBIT   collision mask bit count of k2hash used by transfer mode, when file type
# K2HMAXELE     maximum element count of k2hash used by transfer mode, when file type
# K2HPAGESIZE   page size of k2hash used by transfer mode, when file type
# DTORTHREADCNT k2hdtor thread count
# DTORCTP       custom k2hdtor plugin file name(or path)
# BINTRANS      transfer as binary data array
# EXPIRE        grant the expiration date to transfer
# TRANSLINECNT  transfer line count limit at one time(default 0)
# TRANSTIMEUP   transfer timeup limit(default 0)
# BYTELIMIT     muxium bytes for one data length(default 0, means no limit)
#
K2HFTFUSE:
    {
        K2HTYPE:                mem,
        K2HMASKBIT:             4,
        K2HCMASKBIT:            4,
        K2HMAXELE:              2,

        #
        # K2HFTFUSE_RULE_DIR( K2HFTFUSE sub rule )
        #
        # TARGET        traget directory path
        # TRUNS         enable/disable flag for transfer
        # OUTPUTFILE    enable/disable flag for put file
        # PLUGIN        plugin program path
        # DEFAULTALL    default rule as DENY or ALLOW(DENY as default)
        # ALLOW         allowed rule, rule is static string or regex. and convert rule when regex.
        # DENY          denied rule, rule is static string or regex. and convert rule when regex.
        #
        K2HFTFUSE_RULE_DIR:
        [
            {
                TARGET:             /,
                TRUNS:              off,
                OUTPUTFILE:         /dev/null,
                DEFAULTALL:         ALLOW
            },
        ],

        #
        # K2HFTFUSE_RULE( K2HFTFUSE sub rule )
        #
        # TARGET        traget file path
        # TRUNS         enable/disable flag for transfer
        # OUTPUTFILE    enable/disable flag for put file
        # PLUGIN        plugin program path
        # DEFAULTALL    default rule as DENY or ALLOW
        # ALLOW         allowed rule, rule is static string or regex. and convert rule when regex.
        # DENY          denied rule, rule is static string or regex. and convert rule when regex.
        #
        K2HFTFUSE_RULE:
        [
            {
                TARGET:             access.log,
                TRUNS:              on,
                OUTPUTFILE:         /dev/null,
                DEFAULTALL:         ALLOW
            },
            {
                TARGET:             error.log,
                TRUNS:              on,
                OUTPUTFILE:         /dev/null,
                DEFAULTALL:         ALLOW
            },
        ]
    }
```

- Next is the configuration file to be used on the host that aggregates logs.  
Prepare the server.yaml (YAML format) file.

```
#
# CHMPX GLOBAL SETTING
#
GLOBAL:
    {
        FILEVERSION:            1,
        DATE:                   "Thu, 01 Dec 2016 00:00:00 +0900",
        GROUP:                  K2HFUSETEST,
        MODE:                   SERVER,
        DELIVERMODE:            random,
        MAXCHMPX:               32,
        REPLICA:                0,
        MAXMQSERVER:            8,
        MAXMQCLIENT:            8,
        MQPERATTACH:            1,
        MAXQPERSERVERMQ:        2,
        MAXQPERCLIENTMQ:        8,
        MAXMQPERCLIENT:         4,
        PORT:                   18020,
        CTLPORT:                18021,
        SELFCTLPORT:            18021,
        RWTIMEOUT:              100000,
        RETRYCNT:               1000,
        CONTIMEOUT:             500000,
        MQRWTIMEOUT:            50,
        MQRETRYCNT:             100000,
        DOMERGE:                on,
        SSL:                    no,
        K2HFULLMAP:             on,
        K2HMASKBIT:             4,
        K2HCMASKBIT:            4,
        K2HMAXELE:              2
    }

#
# CHMPX SERVER NODES SECTION
#
SVRNODE:
    [
        {
            NAME:               foo[0-9].yahoo.co.jp,
            SSL:                no
        }
    ]

#
# CHMPX SLAVE NODES SECTION
#
SLVNODE:
    [
        {
            NAME:               "[.]*",
            CTLPORT:            8022
        }
    ]

#
# K2HFTFUSESVR
#
# TYPE              { trans | file | both }
# FILE_BASEDIR      output file base directory
# FILE_UNIFY        if puts one file for all, specify relative file path.(only file type)
# FILE_TIMEFORM     format for time data like strftime function.(only file type) + '%-': ns(decimal), %%=%
# PLUGIN            program file path for plugin
# FORMAT            output format.(only file type)
#                       %H  hostname
#                       %P  thread(process) id
#                       %F  file path
#                       %f  file name
#                       %T  time, formatted by FILE_TIMEFORM
#                       %L  output content data
#                   * If FORMAT is not specified, it means "%L".
#
# TRANSCONF         slave chmpx configuration file path.(only trans type)
# K2HTYPE           type of k2hash used by transfer mode
# K2HFILE           file path of k2hash used by transfer mode, when file type
# K2HFULLMAP        mapping type of k2hash used by transfer mode, when file type
# K2HINIT           initializing of k2hash used by transfer mode, when file type
# K2HMASKBIT        init mask bit count of k2hash used by transfer mode, when file type
# K2HCMASKBIT       collision mask bit count of k2hash used by transfer mode, when file type
# K2HMAXELE         maximum element count of k2hash used by transfer mode, when file type
# K2HPAGESIZE       page size of k2hash used by transfer mode, when file type
# DTORTHREADCNT     k2htpdtor thread count
# DTORCTP           custom transaction plugin
#
K2HFTFUSESVR:
    {
        TYPE:                   file,
        FILE_BASEDIR:           /home/apache/all,
        FILE_UNIFY:             log/unify.log,
        FILE_TIMEFORM:          "%F %T(%s %-)",
        #PLUGIN:                 /usr/local/bin/myprogram,
        FORMAT:                 "%H:%F(%P):%f[%T] %L",
        K2HMASKBIT:             4,
        K2HCMASKBIT:            4,
        K2HMAXELE:              2
    }
```

### Test started

#### First, prepare the hosts that aggregate the logs.
- Start CHMPX server node
```
$ chmpx -conf server.yaml
```

- Note  
If MQ (Posix MQ related) error occurs at startup, increase MQ available in the system.  
(The following MQ numbers are numbers with a margin, please set by the privileged user.)
```
# echo 1024 > /proc/sys/fs/mqueue/msg_max
```

- Start K2HFTFUSESVR
```
$ k2hftfusesvr -conf server.yaml
```

#### Next, start the programs on the host on which Apache is running.

- Start CHMPX slave node
```
$ chmpx -conf slave.yaml
```

- mount by K2HFTFUSE  
In the example below, K2HFTFUSE is started in the foreground.
```
$ k2hftfuse /home/apache/logs -o conf=slave.yaml -f
```

#### Check mount point normally

- Check mount point by df command
```
$ df
Filesystem      1K-blocks    Used  Available Use% Mounted on
/dev/****        41152832 1736572   37617420   5% /
none                    4       0          4   0% /sys/fs/cgroup
none                 5120       0       5120   0% /run/lock
tmpfs             2024060       0    2024060   0% /run/shm
k2hftfuse      1073741824       0 1073741824   0% /home/apache/logs
```

#### Start Apache program
Start up Apache, output the logs, and check the files to be aggregated on the aggregation server.

- Check log file by tail command
```
$ tail -f /home/apache/all/log/unify.log
```

#### Run other program instead of Apache
Even without running Apache, you can write to the file on the sending host as follows.  
In the following example, "TEST STRING" is transferred to the aggregated log file.

- Run echo command instead of Apache
```
$ echo "TEST STRING" > /home/apache/logs/logs/access.log
```

#### In this way it can be confirmed that the file contents can be transferred.
The advantage of K2HFTFUSE is that there is no need to change the transfer source (program).  
And aggregation server and route can be determined by setting, and secure and high speed file / message transfer is possible.  
In addition, you can start your external program on the transfer destination server, making your own processing very easy.  
Please change the setting file and try it out.
