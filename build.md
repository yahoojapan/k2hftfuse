---
layout: contents
language: en-us
title: Build
short_desc: File Transaction by FUSE based file system
lang_opp_file: buildja.html
lang_opp_word: To Japanese
prev_url: usage.html
prev_string: Usage
top_url: index.html
top_string: TOP
next_url: 
next_string: 
---
# Build

This chapter consists of three parts:

* how to set up **K2HFTFUSE** and **K2HFTFUSESVR** for local development
* how to build **K2HFTFUSE** and **K2HFTFUSESVR** from the source code
* how to install **K2HFTFUSE** and **K2HFTFUSESVR**

## 1. Install prerequisites

**K2HFTFUSE** and **K2HFTFUSESVR** primarily depends on  [FULLOCK](https://fullock.antpick.ax/index.html), [K2HASH](https://k2hash.antpick.ax/index.html), [CHMPX](https://chmpx.antpick.ax/index.html) and [K2HTPDTOR](https://k2hthdtor.antpick.ax/index.html). Each dependent library and the header files are required to build **K2HFTFUSE** and **K2HFTFUSESVR**. We provide two ways to install them. You can select your favorite one.

* Use [GitHub](https://github.com/yahoojapan/)  
  Install the source code of dependent libraries and the header files. You will **build** them and install them.
* Use [packagecloud.io](https://packagecloud.io/antpickax/stable/)  
  Install packages of dependent libraries and the header files. You just install them. Libraries are already built.

### 1.1. Install each dependent library and the header files from GitHub

Read the following documents for details:  
* [FULLOCK](https://fullock.antpick.ax/build.html)
* [K2HASH](https://k2hash.antpick.ax/build.html)  
* [CHMPX](https://chmpx.antpick.ax/build.html)  
* [K2HTPDTOR](https://k2htpdtor.antpick.ax/build.html)  

### 1.2. Install each dependent library and the header files from packagecloud.io

This section instructs how to install each dependent library and the header files from [packagecloud.io](https://packagecloud.io/antpickax/stable/). 

**Note**: Skip reading this section if you have installed each dependent library and the header files from [GitHub](https://github.com/yahoojapan) in the previous section.

For DebianStretch or Ubuntu(Bionic Beaver) users, follow the steps below:
```bash
$ sudo apt-get update -y
$ sudo apt-get install curl -y
$ curl -s https://packagecloud.io/install/repositories/antpickax/stable/script.deb.sh \
    | sudo bash
$ sudo apt-get install autoconf autotools-dev gcc g++ make gdb libtool pkg-config \
    libyaml-dev libfullock-dev k2hash-dev chmpx-dev libfuse-dev -y
$ sudo apt-get install git -y
```

For Fedora28 or CentOS7.x(6.x) users, follow the steps below:
```bash
$ sudo yum makecache
$ sudo yum install curl -y
$ curl -s https://packagecloud.io/install/repositories/antpickax/stable/script.rpm.sh \
    | sudo bash
$ sudo yum install autoconf automake gcc gcc-c++ gdb make libtool pkgconfig \
    libyaml-devel libfullock-devel k2hash-devel chmpx-devel fuse fuse-devel -y
$ sudo yum install git -y
```

## 2. Clone the source code from GitHub

Download the **K2HFTFUSE** and **K2HFTFUSESVR**'s source code from [GitHub](https://github.com/yahoojapan/k2hftfuse).
```bash
$ git clone https://github.com/yahoojapan/k2hftfuse.git
```

## 3. Build and install

Just follow the steps below to build **K2HFTFUSE** and **K2HFTFUSESVR** and install it. We use [GNU Automake](https://wwwv.gnu.org/software/automake/) to build **K2HFTFUSE** and **K2HFTFUSESVR**.

```bash
$ cd k2hftfuse
$ sh autogen.sh
$ ./configure --prefix=/usr
$ make
$ sudo make install
```

After successfully installing **K2HFTFUSE** and **K2HFTFUSESVR**, you will see the k2hftfuse help text:
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
