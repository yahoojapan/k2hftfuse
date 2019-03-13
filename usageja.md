---
layout: contents
language: ja
title: Usage
short_desc: File Transaction by FUSE based file system
lang_opp_file: usage.html
lang_opp_word: To English
prev_url: detailsja.html
prev_string: Details
top_url: indexja.html
top_string: TOP
next_url: buildja.html
next_string: Build
---

# 使い方
ここでは、**K2HFTFUSE** のインストールと簡単な動作確認について説明します。

# 1. 利用環境構築

## K2HFTFUSEインストール
**K2HFTFUSE** をご利用の環境にインストールするには、2つの方法があります。  
ひとつは、[packagecloud.io](https://packagecloud.io/)から **K2HFTFUSE** のパッケージをダウンロードし、インストールする方法です。  
もうひとつは、ご自身で **K2HFTFUSE** をソースコードからビルドし、インストールする方法です。  
これらの方法について、以下に説明します。

### パッケージを使ったインストール
**K2HFTFUSE** は、誰でも利用できるように[packagecloud.io - AntPickax stable repository](https://packagecloud.io/antpickax/stable/)で[パッケージ](https://packagecloud.io/app/antpickax/stable/search?q=k2hftfuse)を公開しています。  
**K2HFTFUSE** のパッケージは、Debianパッケージ、RPMパッケージの形式で公開しています。  
お使いのOSによりインストール方法が異なりますので、以下の手順を確認してインストールしてください。  

#### Debian(Stretch) / Ubuntu(Bionic Beaver)
```
$ sudo apt-get update -y
$ sudo apt-get install curl -y
$ curl -s https://packagecloud.io/install/repositories/antpickax/stable/script.deb.sh | sudo bash
$ sudo apt-get install k2hftfuse
```
開発者向けパッケージをインストールする場合は、以下のパッケージをインストールしてください。
```
$ sudo apt-get install k2hftfuse-dev
```

#### Fedora28 / CentOS7.x(6.x)
```
$ sudo yum makecache
$ sudo yum install curl -y
$ curl -s https://packagecloud.io/install/repositories/antpickax/stable/script.rpm.sh | sudo bash
$ sudo yum install k2hftfuse
```
開発者向けパッケージをインストールする場合は、以下のパッケージをインストールしてください。
```
$ sudo yum install k2hftfuse-devel
```

#### 上記以外のOS
上述したOS以外をお使いの場合は、パッケージが準備されていないため、直接インストールすることはできません。  
この場合には、後述の[ソースコード](https://github.com/yahoojapan/k2hftfuse)からビルドし、インストールするようにしてください。

### ソースコードからビルド・インストール
**K2HFTFUSE** を[ソースコード](https://github.com/yahoojapan/k2hftfuse)からビルドし、インストールする方法は、[ビルド](https://k2hftfuse.antpick.ax/buildja.html)を参照してください。

## OS環境設定
**k2HFTFUSE**は、[FUSE (Filesystem in Userspace)](https://github.com/libfuse/libfuse)を利用したFUSEクライアントアプリケーションです。  
このため、あらかじめシステム（OS）でFUSEが利用できるように設定しておく必要があります。  
以下に設定例を示します。詳細は、[FUSE (Filesystem in Userspace)](https://github.com/libfuse/libfuse)などを参照してください。  

### /etc/fuse.conf
root以外で**K2HFTFUSE**を利用する場合には、このファイルに以下の設定を記述します。
```
user_allow_other
```
### /dev/fuse
root以外で**K2HFTFUSE**を利用する場合には、このデバイスファイルにパーミッションを与える必要があります。
```
$ sudo chmod -f a+rw /dev/fuse
```
### /bin/fusermount
root以外で**K2HFTFUSE**を利用する場合には、このファイルにパーミッションを与える必要があります。
```
$ sudo chmod -f o+rx /bin/fusermount
```

# 2. 動作確認
## サンプルコンフィグレーション
K2HFTFUSEとK2HFTFUSESVRの利用するコンフィグレーションのサンプルを示します。

### 多段転送をしない場合

#### 転送元ホスト
##### K2HFTFUSE（転送を行わない場合）
- INI形式  
[k2hftfuse_test_slave.ini]({{ site.github.repository_url }}/blob/master/tests/k2hftfuse_test_slave.ini)
- YAML形式  
[k2hftfuse_test_slave.yaml]({{ site.github.repository_url }}/blob/master/tests/k2hftfuse_test_slave.yaml)
- JSON形式  
[k2hftfuse_test_slave.json]({{ site.github.repository_url }}/blob/master/tests/k2hftfuse_test_slave.json)
- JSON文字列  
[test_json_string.data]({{ site.github.repository_url }}/blob/master/tests/test_json_string.data) ファイルの中の "K2HFTFUSE_TEST_SLAVE_JSON=" の以降の文字列

##### CHMPX
同じ設定ファイルを使用して、K2HFTFUSEプログラムを起動する前に、CHMPXを起動します。

#### 終端ホスト
##### K2HFTFUSESVR（転送を行わない場合）
- INI形式  
[k2hftfuse_test_server.ini]({{ site.github.repository_url }}/blob/master/tests/k2hftfuse_test_server.ini)
- YAML形式  
[k2hftfuse_test_server.yaml]({{ site.github.repository_url }}/blob/master/tests/k2hftfuse_test_server.yaml)
- JSON形式  
[k2hftfuse_test_server.json]({{ site.github.repository_url }}/blob/master/tests/k2hftfuse_test_server.json)
- JSON文字列  
[test_json_string.data]({{ site.github.repository_url }}/blob/master/tests/test_json_string.data) ファイルの中の "K2HFTFUSE_TEST_SLAVE_JSON=" の以降の文字列

##### CHMPX
同じ設定ファイルを使用して、K2HFTFUSEプログラムを起動する前に、CHMPXを起動します。

### 多段転送をする場合

#### 転送元ホスト
##### K2HFTFUSE（転送を行う場合＝転送を行わない場合と同じです）
- INI形式  
[k2hftfuse_test_slave.ini]({{ site.github.repository_url }}/blob/master/tests/k2hftfuse_test_slave.ini)
- YAML形式  
[k2hftfuse_test_slave.yaml]({{ site.github.repository_url }}/blob/master/tests/k2hftfuse_test_slave.yaml)
- JSON形式  
[k2hftfuse_test_slave.json]({{ site.github.repository_url }}/blob/master/tests/k2hftfuse_test_slave.json)
- JSON文字列  
[test_json_string.data]({{ site.github.repository_url }}/blob/master/tests/test_json_string.data) ファイルの中の "K2HFTFUSE_TEST_SLAVE_JSON=" の以降の文字列

##### CHMPX
同じ設定ファイルを使用して、K2HFTFUSEプログラムを起動する前に、CHMPXを起動します。

#### 中継ホスト
##### K2HFTFUSESVR（転送を行う場合の中継ホスト向け）
- INI形式  
[k2hftfuse_test_trans_server.ini]({{ site.github.repository_url }}/blob/master/tests/k2hftfuse_test_trans_server.ini)
- YAML形式  
[k2hftfuse_test_trans_server.yaml]({{ site.github.repository_url }}/blob/master/tests/k2hftfuse_test_trans_server.yaml)
- JSON形式  
[k2hftfuse_test_trans_server.json]({{ site.github.repository_url }}/blob/master/tests/k2hftfuse_test_trans_server.json)
- JSON文字列  
[test_json_string.data]({{ site.github.repository_url }}/blob/master/tests/test_json_string.data) ファイルの中の "K2HFTFUSE_TEST_TRANS_SERVER_JSON=" の以降の文字列

##### CHMPX（転送元との接続）
同じ設定ファイルを使用して、K2HFTFUSEプログラムを起動する前に、CHMPXを起動します。

##### CHMPX（終端との接続）
- INI形式  
[k2hftfusesvr_test_trans_slave.ini]({{ site.github.repository_url }}/blob/master/tests/k2hftfusesvr_test_trans_slave.ini)
- YAML形式  
[k2hftfusesvr_test_trans_slave.yaml]({{ site.github.repository_url }}/blob/master/tests/k2hftfusesvr_test_trans_slave.yaml)
- JSON形式  
[k2hftfusesvr_test_trans_slave.json]({{ site.github.repository_url }}/blob/master/tests/k2hftfusesvr_test_trans_slave.json)
- JSON文字列  
[test_json_string.data]({{ site.github.repository_url }}/blob/master/tests/test_json_string.data) ファイルの中の "K2HFTFUSESVR_TEST_TRANS_SLAVE_JSON=" の以降の文字列

#### 終端ホスト
##### K2HFTFUSESVR（転送を行う場合の終端ホスト向け）
- INI形式  
[k2hftfusesvr_test_trans_server.ini]({{ site.github.repository_url }}/blob/master/tests/k2hftfusesvr_test_trans_server.ini)
- YAML形式  
[k2hftfusesvr_test_trans_server.yaml]({{ site.github.repository_url }}/blob/master/tests/k2hftfusesvr_test_trans_server.yaml)
- JSON形式  
[k2hftfusesvr_test_trans_server.json]({{ site.github.repository_url }}/blob/master/tests/k2hftfusesvr_test_trans_server.json)
- JSON文字列  
[test_json_string.data]({{ site.github.repository_url }}/blob/master/tests/test_json_string.data) ファイルの中の "K2HFTFUSESVR_TEST_TRANS_SERVER_JSON=" の以降の文字列

##### CHMPX
同じ設定ファイルを使用して、K2HFTFUSEプログラムを起動する前に、CHMPXを起動します。


## 簡単な動作確認
実際に、ログファイルを例にしてファイル内容を転送してみます。
以下の例では、K2HFTFUSEでApacheのログファイルを転送するサンプルを以下に示します。

### 前提
foo\[0-9\].yahoo.co.jp という10台のホスト上で、Apacheを使用してウェブサービスを実行します。
Apacheはログファイルとして各ホストの/home/apache/logsディレクトリにaccess.logとerror.logを出力します。
このサンプルではこれらのログファイルを1台のホスト bar.yahoo.co.jp に集約します。

#### 注意
access.logとerror.log以外のファイルは/home/apache/logsディレクトリに出力されないものとします。  

### 設定ファイル

- Apacheの動作しているホストで利用する設定ファイルとして、slave.yaml（YAML形式）ファイルを準備します。

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
# BYTELIMIT     maximum bytes for one data length(default 0, means no limit)
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
        # TARGET        target directory path
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
        # TARGET        target file path
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

- 次にログを集約するホストで利用する設定ファイルとして、server.yaml（YAML形式）ファイルを準備します。

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

### 起動

#### まず、ログを集約するホストの準備をします。
- 通信ミドルウエアCHMPXを起動します。
```
$ chmpx -conf server.yaml
```

- 起動時にMQ（Posix MQ関連）のエラーがでる場合には、ホストで利用できるMQ数を大きくします。  
_下記で指定しているMQ数は余裕を持った数字です。特権ユーザーにて設定してください。_
```
# echo 1024 > /proc/sys/fs/mqueue/msg_max
```

- K2HFTFUSESVRプログラムを起動します。
```
$ k2hftfusesvr -conf server.yaml
```

#### 次は、Apacheの動作しているホストでプログラムを起動します。
- 通信ミドルウエアCHMPXを起動します。
```
$ chmpx -conf slave.yaml
```

- K2HFTFUSEプログラムを起動し、マウントします。（今回はフォアグラウンドで起動しています）
```
$ k2hftfuse /home/apache/logs -o conf=slave.yaml -f
```

#### マウントできているか確認します。
- dfコマンドで確認します。
```
$ df
Filesystem      1K-blocks    Used  Available Use% Mounted on
/dev/****        41152832 1736572   37617420   5% /
none                    4       0          4   0% /sys/fs/cgroup
none                 5120       0       5120   0% /run/lock
tmpfs             2024060       0    2024060   0% /run/shm
k2hftfuse      1073741824       0 1073741824   0% /home/apache/logs
```

#### Apacheを起動して、ログを出力させ、集約するホスト上でファイル内容が転送されているか確認してみてください。
- tailコマンドで確認します。
```
$ tail -f /home/apache/all/log/unify.log
```

#### Apacheを起動していなくても、Apache側（送信側）のホスト上で以下のようにすることで"TEST STRING"が上記のファイルに転送されていることを確認できます。
- Apacheを使わないで確認します。
```
$ echo "TEST STRING" > /home/apache/logs/logs/access.log
```

#### 以上のようにして、ファイル内容が転送できていることが確認できます。  
K2HFTFUSEの優れている点は、転送元（プログラム）の変更をする必要がなく、設定により集約ホスト、経路を決定でき、確実で高速なファイル/メッセージの転送ができる点です。  
また、転送先のホストでは、任意のプログラムを起動しておき、独自の処理が簡単にできます。  
設定ファイルをいろいろと変更してみて、試してみてください。
