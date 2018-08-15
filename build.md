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

# Building
The build method for K2HFTFUSE and K2HFTFUSESVR is explained below.

## 1. Install prerequisites before compiling
- Debian / Ubuntu  
```
$ sudo aptitude update
$ sudo aptitude install git autoconf autotools-dev gcc g++ make gdb dh-make fakeroot dpkg-dev devscripts libtool pkg-config libssl-dev libyaml-dev libfuse-dev
```

- Fedora / CentOS  
```
$ sudo yum install git autoconf automake gcc libstdc++-devel gcc-c++ make libtool openssl-devel libyaml-devel fuse fuse-devel
```

## 2. Building and installing FULLOCK
```
$ git clone https://github.com/yahoojapan/fullock.git
$ cd fullock
$ ./autogen.sh
$ ./configure --prefix=/usr
$ make
$ sudo make install
```

## 3. Building and installing K2HASH
```
$ git clone https://github.com/yahoojapan/k2hash.git
$ cd k2hash
$ ./autogen.sh
$ ./configure --prefix=/usr
$ make
$ sudo make install
```

## 4. Building and installing CHMPX
```
$ git clone https://github.com/yahoojapan/chmpx.git
$ cd chmpx
$ ./autogen.sh
$ ./configure --prefix=/usr
$ make
$ sudo make install
```

## 5. Building and installing K2HTPDTOR
```
$ git clone https://github.com/yahoojapan/k2htp_dtor.git
$ cd k2htp_dtor
$ ./autogen.sh
$ ./configure --prefix=/usr
$ make
$ sudo make install
```

## 6. Clone source codes from Github
```
$ git clone git@github.com:yahoojapan/k2hftfuse.git
```

## 7. Building and installing K2HFTFUSE
```
$ ./autogen.sh
$ ./configure --prefix=/usr
$ make
$ sudo make install
```
