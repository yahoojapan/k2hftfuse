---
layout: contents
language: ja
title: Performance
short_desc: File Transaction by FUSE based file system
lang_opp_file: performance.html
lang_opp_word: To English
prev_url: 
prev_string: 
top_url: featureja.html
top_string: Feature
next_url: 
next_string: 
---

# K2HFTFUSE のパフォーマンス測定結果について
----

## 目次  
### [パフォーマンス測定結果](#result001)
- [パフォーマンス結果（ボトルネックとなる環境なし）](#result001)
- [1Gbps ネットワークでの測定（ネットワークのボトルネック環境）](#result002)
- [HDD 使用環境での測定（ファイル・デバイスのボトルネック環境）](#result003)

### [他プロダクトとの比較測定結果](#resultothers)
- [測定結果](#result004)

### [まとめ](#summary)
- [K2HFTFUSE の性能について](#summary01)
- [K2HFTFUSE が効果的なユースケース](#summary03)
- [おわりに](#summary04)
  
### [Appendix](#appendix00)
- [測定方法の決め方について](#pretest)
- [計測スクリプト](#appendix02)
- [K2HFTFUSE 設定ファイル(受信側)](#appendix03)
- [K2HFTFUSE 設定ファイル(送信側)](#appendix04)

----

## <a name="result">パフォーマンス測定結果</a>

### <a name="result001">パフォーマンス結果（ボトルネックとなる環境なし）</a>
10 Gbps ネットワークと SSD を使用し、H/Wによるボトルネックが無い環境で転送量の計測を行いました。

#### 測定した内容
3パターンのデータファイルを用意しました。
- long: 4096 Byte x 1千万レコード / middle: 1024 Byte x 1千万レコード / short: 10 Byte x 1億レコード  

このファイルをクライアント3台からサーバー1台に送信してデータ転送量を計測します。  
  
![図3](images/k2hftfuse_performance_003.png)


#### 使用した H/Wスペック
- Xeon E7-4850 / 4CPU
- 128GB MEM
- SAS 300GB x2 / RAID1 x 1 vol
- Network 10 Gbps  

#### 使用した環境
- OS は ubuntu 14.04 です。  
- ネットワーク環境は 10Gbps で用意しました。  
- K2HFTFUSE は OSS 版 1.0.23 を使用しています。

#### 測定方法
- 3台から同時に送信を始めます。 （手動なので完全に同時ではありません。）
- 受信側で受け取ったデータを1つのファイルに集約し、10秒ごとにファイルサイズのチェックを行って、この差分から転送量を計算します。
- 送信開始20秒前後からの転送量が安定した時点から100秒間の平均値を取り、これを3回行った平均値を取ります。

#### データの投入方法
- ```cat [データファイル] > [転送対象ディレクトリ]``` で、投入しました。

#### データの集約方法
- 設定ファイルで ```[K2HFTFUSESVR]FILE_UNIFY``` を指定して1ファイルに集約しました。

#### パフォーマンス測定結果

![図1](images/k2hftfuse_performance_001.png)
  
転送量は 最大で 250MB となりました。(4096 Byte レコード送信時)  
  

|レコード長 | 平均転送レコード数（秒） | 平均転送バイト数（秒）  |
|        --:|                       --:|                      --:|
|    10 Byte|                 937,779  |                 8.9 MB/S|
| 1024 Byte |                 219,578  |               214.4 MB/S|
| 4096 Byte |                 64,943   |               253.7 MB/S|
  
----
<!-- ---------------------------------------------------------------------------------- -->

### <a name="result002">1Gbps ネットワークでの測定（ネットワークのボトルネック環境）</a>

上記と同様に転送量の計測を目的としたテストを 1Gbps ネットワークで行った際の結果です。  
この結果はネットワーク性能が不足している場合の挙動になります。  

#### テスト環境

- Xeon D-1541 2.1GHz 8コア,16スレッド  
- 32GB MEM  
- SATA SSD 400GB x1 / RAID 無  
- Network 1 Gbps  

#### テスト方法

転送量の計測は、10秒ごとに集約したファイルのライン数をカウントして計算値を出しました。  
計測が終わるとファイルを削除して、作り直されたファイルに書き込んでいきます。  

#### テスト結果

![図7](images/k2hftfuse_performance_007.png)
  
転送量は 100MB 付近で最大となり、long サイズと middle サイズで差分があまり見られませんでした。  
  
外部からモニタリングツールで確認した結果、 実際に回線速度の上限に達していることが確認できました。
よって、この計測ではネットワーク上限での挙動を見ることができていますが、本来の性能は計測できていません。

#### まとめ

- 1Gbps ネットワークで K2HFTFUSE を使用した場合、ネットワークの限界以上の転送能力に留意して使用する必要があります。
  これは適切なウエートや送信頻度などで調整できると思います。

----
<!-- ---------------------------------------------------------------------------------- -->

### <a name="result003">HDD 使用環境での測定（ファイル・デバイスのボトルネック環境）</a>

上記と同様に転送量の計測を目的としたテストを HDD で行った際の結果です。  
この結果はファイル・デバイスの性能が不足している場合の挙動になります。  

#### テスト環境

- Xeon E5-2630L 2.00GHz / 2CPU  
- 64GB MEM  
- SAS 300GB x2 / RAID1 x 1 vol
- Network 10 Gbps  

#### テスト方法

10秒ごとにファイルサイズをチェックして、10秒前のサイズとの差分から転送量を計算しました。

#### テスト結果

![図11](images/k2hftfuse_performance_011.png)
  
転送量は 150MB 付近で最大となり、long サイズと middle サイズで差分があまり見られませんでした。  
計測後にさらに転送を継続した際にパフォーマンスが急激に下がる現象が発生したので、転送完了までの推移を確認したのが下のグラフです。

![図4](images/k2hftfuse_performance_004.png)

同時にモニタリングツールで見てみると、計測中にフリーメモリがキャッシュに割り当てられて徐々に減少し、枯渇したところで遅くなる事がわかりました。
このグラフの時は150秒付近で最高速が出なくなり、700秒付近ではフリーメモリがほぼ無くなった状態になっていました。  
  
この計測では SSD ではなく HDD を使っているので、受信したデータを書き込む際にディスクの速度が追いつかず、最終的にメモリを使い果したと想像できます。
出力先を /dev/null にしてネットワーク帯域の使用状況を見てみると、転送完了まで途中で速度低下することなく推移したので HDD がボトルネックであると確認できました。 
  
#### まとめ
- 転送量がディスク書き込み速度を上回るとメモリを使い果した後にパフォーマンスが低下します。
- HDD で K2HFTFUSE を利用する場合には、書き込み速度（転送量）を意識して運用をする必要があります。上限に達した場合には OS 全体のパフォーマンスへの影響が考えられます。

----
## <a name="resultothers">他プロダクトとの比較測定結果</a>

同じユースケースで他のプロダクトを使用してみるとどうなるか、 fluentd と kafka で比較してみました。

今回パフォーマンス測定をしたプロダクトとバージョンです。
- fluentd 0.14.15
- kafka 2.10-0.10.2.0
- K2HFTFUSE 1.0.23

#### データの投入方法
- fluentd  
  ```cat [データファイル] > [転送対象ファイル]``` で、投入しました。
- kafka  
  1パーティションのtopicを作成し、  
  ```cat [データファイル] | kafka-console-producer.sh``` で、投入しました。  
  そのままでは timeout 多発だったので メッセージ内容を見ながら ```--request-timeout-ms``` を調整しています。
- K2HFTFUSE  
  ```cat [データファイル] > [転送対象ディレクトリ]``` で、投入しました。

#### データの集約方法
- fluentd  
  設定ファイルで ```type file``` とすると自動でファイルがローテーションされ、最新ファイルへのシンボリックリンクが作成される仕組みになっています。
  このままでは他のプロダクトと同じ条件での計測が難しいので、```type stdout``` として画面出力したものをファイルにリダイレクトして1ファイルに集約します。
- kafka  
  ```kafka-console-consumer.sh``` の出力をファイルにリダイレクトして使用しました。
- K2HFTFUSE  
  設定ファイルで ```[K2HFTFUSESVR]FILE_UNIFY``` を指定して1ファイルに集約しました。  
  通常の生ログ（RAW）での出力に加え、fluentd に合わせた JSON 出力の結果も計測しました。


#### <a name="result004">パフォーマンス測定結果</a>
----
10 Byte レコード  
![図11](images/k2hftfuse_performance_101.png)

- K2HFTFUSE（JSON）が最も良いパフォーマンスとなりました。  
  これは、転送バイト数の計測を受信サーバーで行っており、JSON 形式の場合は JSON に変換後のサイズで計測されているためです。（K2HFTFUSE（JSON）とfluentdの計測の場合）

----
1024 Byte レコード  
![図12](images/k2hftfuse_performance_102.png)

- K2HFTFUSE（RAW）が最も良いパフォーマンスとなりました。　
----
4096 Byte レコード  

![図13](images/k2hftfuse_performance_103.png)
- K2HFTFUSE（RAW）が最も良いパフォーマンスとなりました。　
----
#### 他プロダクトとの比較測定結果まとめ

- 今回のユースケースでは、 どのサイズのデータでも K2HFTFUSE が一番良いパフォーマンスとなりました。
- 10 Byte レコードと比較すると、kafka はレコードサイズの大きなデータの方がパフォーマンスが良いという傾向でした。

----
### <a name="summary">まとめ</a>

#### <a name="summary01">K2HFTFUSE の性能について</a>

K2HFTFUSE の性能は、10Gbps のネットワークと高速なストレージを準備して初めて上限に達しました。
それ以下の環境で利用する際には、K2HFTFUSE の限界よりも先に、ネットワークやディスク書き込み速度が上限に達します。
よって、既存の大多数のシステム上で K2HFTFUSE の限界を気にせず有効に使うことができます。

#### <a name="summary03">K2HFTFUSE が効果的なユースケース</a>
今回は、K2HFTFUSE の想定しているユースケースのひとつである、ログ転送を主目的としたパフォーマンスの調査をしました。
結果として、ログ転送において想定以上のパフォーマンスを確認できました。
この結果から、

- 大量のデータが書き込まれるログを転送する。
- 多数のファイルに吐かれるログを転送、集約する。
- 転送データの加工をする。（今回はJSON形式で試行）
- データ加工を簡単にプロトタイプとして試行する。

などといったユースケースが効果的だと思います。  

また、他のプロダクトと比較したときの、
- 稼働後にサーバーのスケール（追加や削除）を頻繁に行うケース。
- 最小限のマニュアルでシステム運用チームに引き渡す。  
  
という特長を活かせる場面も多いと思います。

#### <a name="summary04">おわりに</a>
今回は比較対象として同じような機能を持っている fluentd と kafkaと比較してみましたが、私はこの2つのプロダクトのプロフェッショナルではありません。
そのため、これらのプロダクトについて精通している方がチューニングして計測した場合はもっと良い結果になると思います。
同じユースケースでチューニングした結果があればぜひとも公開していただければと思います。
また、他のユースケースでの計測結果があれば K2HFTFUSE でも検査をしてみたいと思います。

  
  
----
## <a name="appendix00">Appendix</a>

今回の計測方法や、計測に使用した設定ファイルです。

----

<!-- ---------------------------------------------------------------------------------- -->
<!-- ---------------------------------------------------------------------------------- -->
<!-- ---------------------------------------------------------------------------------- -->
### <a name="pretest">測定方法の決め方について</a>
測定方法に至るまでの準備や試行錯誤を簡単に紹介します。

#### インストール
弊社内の開発用サーバーとネットワークを手配して各プロダクトのインストールと、チュートリアルにある疎通確認までを行いました。  
fluentd と kafka は高速化のためのチューンを特に行わずに使用しています。

#### 予備テスト 
データファイルとサーバー構成を変えながら計測しました。
- 送り側1台、受け側1台で小規模なデータを流してみる  
  100 Byte x 1千万レコードの送信開始から送信終了までの時間を測定しました。  
- データサイズを変えてみる  
  レコード数を1億、レコード長を 32 / 256 / 1024 Byte の 3パターンで用意し、それぞれが受信側でファイルを書き終わるまでの時間を3回測定して平均値を取りました。  
- 複数台から送信してみる  
  レコード長 32 Byte のデータを使って送信側を 2台、3台と増やしてみます。  

この結果を参考にして専用環境でテストする際の計測方法やデータサイズをあらためて検討しました。  

- テストデータ長  
  今までは最大 1024 Byteで計測していましたが、もっと大きなサイズを準備しておけば多くのケースの指標になると考え、本番の計測では最大長を 4096 Byteにする事に決めました。
- 計測方法  
  純粋な転送能力を測定したいので受信側で集約したファイルを、10秒ごとの行数カウントとファイル削除を行い理論値を計算することにしました。 これによって送信側の影響を取り除くことを試みています。  
- 計測区間  
  それぞれのプロダクトで試したところ、転送を開始から安定的に速度が出るのに20秒程度かかっていましたので、ここからを計測区間とすることにしました。
  

----
### <a name="appendix02">計測スクリプト</a>

サーバ側で集約したファイルは以下のスクリプトを使って書き込みバイト数を計算しました。

```
#!/bin/bash
pre=0
while :
do
	date=`date`
	now=`ls -l log.txt | awk '{print $5;}'`
	num=`expr $now - $pre`

	echo -n "$date "
	echo $num

	pre=$now
	sleep 10
done
```

----
### <a name="appendix03">K2HFTFUSE 設定ファイル（受信側）</a>
  
今回の測定で使用した設定ファイルです。  

  
```
# k2hftfuse server side
#
# GLOBAL SECTION
#
[GLOBAL]
FILEVERSION			= 1
DATE				= Thu, 03 Sep 2015 17:27:28 +0900
GROUP				= K2HFUSETEST
MODE				= SERVER
DELIVERMODE			= random
MAXCHMPX			= 5
REPLICA				= 0
MAXMQSERVER			= 64
MAXMQCLIENT			= 64
MQPERATTACH			= 1
MAXQPERSERVERMQ			= 2
MAXQPERCLIENTMQ			= 32
MAXMQPERCLIENT			= 8
MAXHISTLOG			= 0
PORT				= 8020
CTLPORT				= 8021
SELFCTLPORT			= 8021
RWTIMEOUT			= 10000
RETRYCNT			= 1000
CONTIMEOUT			= 500000
MQRWTIMEOUT			= 20000
MQRETRYCNT			= 2000
MQACK				= no
DOMERGE				= on
AUTOMERGE			= on
MERGETIMEOUT			= 0
SOCKTHREADCNT			= 8
MQTHREADCNT			= 4
MAXSOCKPOOL			= 8
SOCKPOOLTIMEOUT			= 0
SSL				= no
K2HFULLMAP			= on
K2HMASKBIT			= 8
K2HCMASKBIT			= 7
K2HMAXELE			= 32
K2HPAGESIZE			= 5000

#
# SERVER NODES SECTION
#
[SVRNODE]
NAME				= xxxx.yahoo.co.jp
SSL				= no

#
# SLAVE NODES SECTION
#
[SLVNODE]
NAME				= [.]*
CTLPORT				= 8022

[K2HFTFUSESVR]
TYPE				= file
FILE_BASEDIR			= /tmp/k2hftfusesvr
FILE_TIMEFORM			= "%F %T.%-"
#FORMAT				= "%T +0900 temotest': {'message':'%l','host':'%H'}\n" # for JSON format
FORMAT				= "%L"
FILE_UNIFY			= log/unify.log
```
----
### <a name="appendix04">K2HFTFUSE 設定ファイル（送信側）</a>
  
今回の測定で使用した設定ファイルです。  

  
```
# k2hftfuse slave side
#
# GLOBAL SECTION
#
[GLOBAL]
FILEVERSION			= 1
DATE				= Thu, 03 Sep 2015 17:27:28 +0900
GROUP				= K2HFUSETEST
MODE				= SLAVE
DELIVERMODE			= random
MAXCHMPX			= 5
REPLICA				= 0
MAXMQSERVER			= 64
MAXMQCLIENT			= 32
MQPERATTACH			= 8
MAXQPERSERVERMQ			= 16
MAXQPERCLIENTMQ			= 1
MAXMQPERCLIENT			= 8
MAXHISTLOG			= 1000
PORT				= 8020
CTLPORT				= 8022
SELFCTLPORT			= 8022
RWTIMEOUT			= 100000
RETRYCNT			= 10000
MQRWTIMEOUT			= 50
MQRETRYCNT			= 20000
CONTIMEOUT			= 500000
MQACK				= no
DOMERGE				= on
AUTOMERGE			= on
MERGETIMEOUT			= 0
SOCKTHREADCNT			= 0
MQTHREADCNT			= 8
MAXSOCKPOOL			= 8
SOCKPOOLTIMEOUT			= 0
SSL				= no
K2HFULLMAP			= on
K2HMASKBIT			= 8
K2HCMASKBIT			= 4
K2HMAXELE			= 32

#
# SERVER NODES SECTION
#
[SVRNODE]
NAME				= xxxx.yahoo.co.jp
PORT				= 8020
CTLPORT				= 8021
SSL				= no

#
# SLAVE NODES SECTION
#
[SLVNODE]
NAME				= [.]*
CTLPORT				= 8022

#
# K2HTPDTOR
#
[K2HTPDTOR]
K2HTPDTOR_BROADCAST		= no
K2HTPDTOR_FILTER_TYPE		= DELKEY

[K2HFTFUSE]
K2HTYPE				= mem
K2HMASKBIT			= 8
K2HCMASKBIT			= 4
K2HMAXELE			= 32
K2HPAGESIZE			= 4096
DTORTHREADCNT			= 2
BINTRANS			= no
TRANSLINECNT			= 10000
TRANSTIMEUP			= 10
BYTELIMIT			= 0

#
# K2HFTFUSE_RULE_DIR( K2HFTFUSE sub rule )
#

[K2HFTFUSE_RULE_DIR]
TARGET          = senddir
TRUNS           = on
DEFAULTALL      = ALLOW
```
