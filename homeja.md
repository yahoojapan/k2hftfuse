---
layout: contents
language: ja
title: Overview
short_desc: File Transaction by FUSE based file system
lang_opp_file: home.html
lang_opp_word: To English
prev_url: 
prev_string: 
top_url: indexja.html
top_string: TOP
next_url: featureja.html
next_string: Feature
---

# K2HFTFUSE
**K2HFTFUSE**（**F**ile **T**ransaction by **FUSE** based file system）とは、[FUSE（Filesystem in Userspace）](https://github.com/libfuse/libfuse)によるユーザースペースでのマウント機能を利用したファイル/メッセージ転送システムです。

K2HFTFUSEは、確実で高速なファイル/メッセージ転送を低コストで実現するために開発されたシステムです。  
K2HFTFUSEは、仮想ファイルシステムを提供し、マウントしたディレクトリにファイルを書き込むだけで利用できます。  
既存プログラムの出力ファイルのディレクトリをK2HFTFUSEでマウントするだけで、既存プログラムの変更なしにファイル/メッセージ転送ができます。

ファイル転送の一例として、ログファイルを転送する場合、既存のプログラムを変更することなく、ログを高速かつ確実に集約できます。  
類似したシステム（プログラム）では、[fluentd](http://www.fluentd.org/)があります。
fluentdと比較しても、高速であり、確実に転送を行うことができるシステムです。  
K2HFTFUSEは、テキストファイルへ1行分のデータを書き込んだタイミングで、そのデータを転送します。  
この他、転送する内容は、テキストだけではなく、バイナリデータ、メッセージ（1データを1メッセージとする）の転送ができます。
また、転送するデータに対して、任意のフィルタリング、任意の処理を行うこともできます。

### 背景
ウェブサービスに限らず、ログファイルやデータなどを集約することは昨今当たり前のように必要となっています。
大規模なログなどの集約を行うケースなどでは、[Kafka](https://kafka.apache.org/)（+[Storm](http://storm.apache.org/)）で構築されたシステムが用いられていることと思われますが、小規模であれば簡単にfluentdで構築されたシステムもあるかと思います。
K2HFTFUSE は、同時公開された基礎ライブラリ/システム（[FULLOCK](https://fullock.antpick.ax/indexja.html), [K2HASH](https://k2hash.antpick.ax/indexja.html), [CHMPX](https://chmpx.antpick.ax/indexja.html), [K2HTPDTOR](https://k2htp_dtor.antpick.ax/indexja.html)）を利用して、fluentdのように簡単に、そしてKafka + Stormのように高速で確実な転送ができないかという試みで開発されました。  
libfuse（FUSE）をベースに基礎ライブラリ/システムを使った結果、期待以上の高パフォーマンスで可用性、スケーラビリティのあるシステムとなっています。

## K2HFTFUSEの構成
まず、以下にK2HFTFUSEを利用したデータ（ファイル/メッセージ）転送の概要図を示します。

![Overview](images/k2hftfuse_overview.png)

上記に示すように、K2HFTFUSEシステムは、libfuse（FUSE）を利用したFUSE派生プログラムになっています。  
K2HFTFUSEプログラムが転送したデータ（ファイル/メッセージ）は、K2HFTFUSESVRプログラムで受信することができ、転送先で集約できます。  
転送するデータがテキストを対象とする（ログファイルなど）場合、テキストファイルであれば、1行単位で転送が行われます。

FUSEを利用することにより、転送側のプログラムの変更は必要なく、出力ファイル（ログファイル）などのディレクトリをFUSEによりマウントするだけで、転送ができるようになっています。  
これにより、既存システムへの導入に対しての障壁はなく、fluentdと同様に簡単に導入できます。

K2HFTFUSEでは、複数の転送元と複数の転送先を任意に構成できます。  
複数の転送元からのデータ（ファイル/メッセージ）を1つの転送元サーバーに集約できます。  
また、複数の転送先サーバーを準備して、分散転送させることも、同報転送もできます。  
この通信制御は、今回同時公開されたシステム（CHMPX）により実現される機能となっています。

K2HFTFUSEの名前に含まれている"K2H"はK2HASHを意味しています。K2HASHは独自のKVS（NoSQL）ライブラリであり、このK2HASHを利用したシステムでもあります。  
このK2HASHライブラリを利用することで、転送するデータ（ファイル/メッセージ）のキューイングを実現しています。  
これにより、通信断、転送先サーバーの高負荷などによる転送遅延が発生したとしても、転送データそのものはキューイングされ、ロストする確立を低下してくれています。
