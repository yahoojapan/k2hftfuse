k2hftfuse
---------
[![Build Status](https://travis-ci.org/yahoojapan/k2hftfuse.svg?branch=master)](https://travis-ci.org/yahoojapan/k2hftfuse)

k2hftfuse for file transaction by FUSE-based file system.

### Overview
k2hftfuse is file transaction system on FUSE file system with K2HASH and
K2HASH TRANSACTION PLUGIN, CHMPX.

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
  - [WIKI](https://github.com/yahoojapan/k2hftfuse/wiki)

### License
This software is released under the MIT License, see the license file.

### AntPickax
k2hftfuse is one of [AntPickax](https://yahoojapan.github.io/AntPickax/) products.

Copyright(C) 2015 Yahoo Japan corporation.
