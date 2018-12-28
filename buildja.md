---
layout: contents
language: ja
title: Build
short_desc: File Transaction by FUSE based file system
lang_opp_file: build.html
lang_opp_word: To English
prev_url: usageja.html
prev_string: Usage
top_url: indexja.html
top_string: TOP
next_url: 
next_string: 
---

# ビルド

この章は3つの部分で構成されています。

* ローカル開発用にK2HFTFUSEとK2HFTFUSESVRをセットアップする方法
* ソースコードからK2HFTFUSEとK2HFTFUSESVRを構築する方法
* K2HFTFUSEとK2HFTFUSESVRのインストール方法

## 1. ビルド環境の構築

K2HFTFUSEとK2HFTFUSESVRは、主に[FULLOCK](https://fullock.antpick.ax/indexja.html), [K2HASH](https://k2hash.antpick.ax/indexja.html), [CHMPX](https://chmpx.antpick.ax/indexja.html) および [K2HTPDTOR](https://k2hthdtor.antpick.ax/indexja.html)に依存します。それぞれの依存ライブラリとヘッダファイルは、K2HFTFUSEとK2HFTFUSESVRをビルドするために必要です。それらをインストールする方法は2つあります。好きなものを選ぶことができます。

* [GitHub](https://github.com/yahoojapan)  を使う  
依存ライブラリのソースコードとヘッダファイルをインストールします。あなたはそれらをビルドしてインストールします。
* [packagecloud.io](https://packagecloud.io/antpickax/stable/) を使用する  
依存ライブラリのパッケージとヘッダファイルをインストールします。あなたはそれらをインストールするだけです。ライブラリはすでに構築されています。

### 1.1. [GitHub](https://github.com/yahoojapan)  から各依存ライブラリとヘッダファイルをインストールする

詳細については以下の文書を読んでください。

* [FULLOCK](https://fullock.antpick.ax/buildja.html)
* [K2HASH](https://k2hash.antpick.ax/buildja.html)  
* [CHMPX](https://chmpx.antpick.ax/buildja.html)  
* [K2HTPDTOR](https://k2htpdtor.antpick.ax/buildja.html)  

### 1.2.  packagecloud.ioから各依存ライブラリとヘッダファイルをインストールします。

このセクションでは、[packagecloud.io](https://packagecloud.io/antpickax/stable/) から各依存ライブラリとヘッダーファイルをインストールする方法を説明します。

注：前のセクションで各依存ライブラリと[GitHub](https://github.com/yahoojapan)  からのヘッダーファイルをインストールした場合は、このセクションを読み飛ばしてください。

DebianStretchまたはUbuntu（Bionic Beaver）をお使いの場合は、以下の手順に従ってください。
```bash
$ sudo apt-get update -y
$ sudo apt-get install curl -y
$ curl -s https://packagecloud.io/install/repositories/antpickax/stable/script.deb.sh \
    | sudo bash
$ sudo apt-get install autoconf autotools-dev gcc g++ make gdb libtool pkg-config \
    libyaml-dev libfullock-dev k2hash-dev chmpx-dev libfuse-dev -y
$ sudo apt-get install git -y
```

Fedora28またはCentOS7.x（6.x）ユーザーの場合は、以下の手順に従ってください。
```bash
$ sudo yum makecache
$ sudo yum install curl -y
$ curl -s https://packagecloud.io/install/repositories/antpickax/stable/script.rpm.sh \
    |スードバッシュ
$ sudo yumインストールautoconf automake gcc gcc-c ++ gdb make libtool pkgconfig \
    libyaml-devel libfullock-devel k2ハッシュ-devel chmpx-devel fuse fuse-devel -y
$ sudo yum install git -y
```

## 2. [GitHub](https://github.com/yahoojapan/k2hftfuse)  からソースコードを複製する

[GitHub](https://github.com/yahoojapan/k2hftfuse)  からK2HFTFUSEとK2HFTFUSESVRのソースコードをダウンロードしてください。

```bash
$ git clone https://github.com/yahoojapan/k2hftfuse.git
```

## 3. ビルドしてインストールする

以下のステップに従ってK2HFTFUSEとK2HFTFUSESVRをビルドしてインストールしてください。 K2HFTFUSEとK2HFTFUSESVRを構築するためにGNU Automakeを使います。

```bash
$ cd k2hftfuse
$ sh autogen.sh
$ ./configure --prefix = / usr
$ make
$ sudo make install
```

K2HFTFUSEとK2HFTFUSESVRを正常にインストールすると、k2hftfuseのヘルプテキストが表示されます。
```bash
$ k2hftfuse -h
[Usage] k2hftfuse
 You can run k2hftfuse by two way, one is manual and the other is using mount command.

 * run k2hftfuse manually
     k2hftfuse [mount point] [k2hftfuse or fuse options]
   for example:
     $ k2hftfuse /mnt/k2hfs -o allow_other,nodev,nosuid,_netdev,dbglevel=err,conf=/etc/k2hftfuse.conf -f -d &

 * using mount/umount/fusermount
   makes following formatted line in fstab:
     k2hftfuse [mount point] fuse [k2hftfuse or fuse options]
   for example:
     k2hftfuse /mnt/k2hfs fuse allow_other,nodev,nosuid,_netdev,dbglevel=err,conf=/etc/k2hftfuse.conf 0 0
   you can run k2hftfuse by mount command.
     $ mount /mnt/k2hfs
     $ umount /mnt/k2hfs
     $ fusermount -d /mnt/k2hfs

 * k2hftfuse options:
     -h(--help)                   print help
     -v(--version)                print version
     -d(--debug)                  set debug level "msg", this option is common with FUSE
     -o umask=<number>            set umask with FUSE
     -o uid=<number>              set uid with FUSE
     -o gid=<number>              set gid with FUSE
     -o dbglevel={err|wan|msg}    set debug level affect only k2hftfuse
     -o conf=<configration file>  specify configration file(.ini .yaml .json) for k2hftfuse and all sub system
     -o json=<json string>        specify json string as configration for k2hftfuse and all sub system
     -o enable_run_chmpx          run chmpx slave process
     -o disable_run_chmpx         do not run chmpx slave process(default)
     -o chmpxlog=<log file path>  chmpx log file path when k2hftfuse run chmpx

 * k2hftfuse environments:
     K2HFTCONFFILE                specify configration file(.ini .yaml .json) instead of conf option.
     K2HFTJSONCONF                specify json string as configration instead of json option.

 Please see man page - k2hftfuse(1) for more details.
 ```
