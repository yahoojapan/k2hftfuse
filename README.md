k2hftfuse
---------
[![C/C++ AntPickax CI](https://github.com/yahoojapan/k2hftfuse/workflows/C/C++%20AntPickax%20CI/badge.svg)](https://github.com/yahoojapan/k2hftfuse/actions)
[![GitHub license](https://img.shields.io/badge/license-MIT-blue.svg)](https://raw.githubusercontent.com/yahoojapan/k2hftfuse/master/COPYING)
[![GitHub forks](https://img.shields.io/github/forks/yahoojapan/k2hftfuse.svg)](https://github.com/yahoojapan/k2hftfuse/network)
[![GitHub stars](https://img.shields.io/github/stars/yahoojapan/k2hftfuse.svg)](https://github.com/yahoojapan/k2hftfuse/stargazers)
[![GitHub issues](https://img.shields.io/github/issues/yahoojapan/k2hftfuse.svg)](https://github.com/yahoojapan/k2hftfuse/issues)
[![debian packages](https://img.shields.io/badge/deb-packagecloud.io-844fec.svg)](https://packagecloud.io/antpickax/stable)
[![RPM packages](https://img.shields.io/badge/rpm-packagecloud.io-844fec.svg)](https://packagecloud.io/antpickax/stable)

k2hftfuse for file transaction by FUSE-based file system.  

### Overview
k2hftfuse is file transaction system on FUSE file system with K2HASH and
K2HASH TRANSACTION PLUGIN, CHMPX.  

![K2HFTFUSE](https://k2hftfuse.antpick.ax/images/top_k2hftfuse.png)

### Usage
You can run k2hftfuse by manual and mount/umount system command(or fusermount command).
If you run it manually, you can specify some following options.
```
$ k2hftfuse /mnt/k2hfs -o allow_other,dbglevel=err,conf=/etc/k2hftfuse.conf -f -d
```

For using mount/umount command, you need to modify /etc/fstab and add following line.
You can start k2hftfuse in any way. And you can see mount information by df command.

/etc/fstab:
```
k2hftfuse /mnt/k2hfs fuse allow_other,nodev,nosuid,_netdev,dbglevel=err,conf=/etc/k2hftfuse.conf 0 0
```

Please see man k2hftfuse.

### Documents
  - [Document top page](https://k2hftfuse.antpick.ax/)
  - [Github wiki page](https://github.com/yahoojapan/k2hftfuse/wiki)
  - [About AntPickax](https://antpick.ax/)

### Packages
  - [RPM packages(packagecloud.io)](https://packagecloud.io/antpickax/stable)
  - [Debian packages(packagecloud.io)](https://packagecloud.io/antpickax/stable)

### License
This software is released under the MIT License, see the license file.

### AntPickax
k2hash is one of [AntPickax](https://antpick.ax/) products.

Copyright(C) 2015 Yahoo Japan Corporation.
