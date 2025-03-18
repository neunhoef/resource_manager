# Benchmark results

## 1 Thread

Results for BoundedList:
  Total duration: 5.10 seconds
  Total writes: 80000001
  Writes/sec: 15676224.68

Aggregate stats for BoundedList:
  Median latency: 30.00 ns
  Average latency: 34.76 ns
  90%ile latency: 30.00 ns
  99%ile latency: 341.00 ns
  99.9%ile latency: 862.00 ns

Results for BoundedList2:
  Total duration: 5.02 seconds
  Total writes: 83759586
  Writes/sec: 16688336.68

Aggregate stats for BoundedList2:
  Median latency: 20.00 ns
  Average latency: 30.63 ns
  90%ile latency: 30.00 ns
  99%ile latency: 300.00 ns
  99.9%ile latency: 751.00 ns

## 2 Threads

Results for BoundedList:
  Total duration: 5.00 seconds
  Total writes: 25664984
  Writes/sec: 5132857.20

Aggregate stats for BoundedList:
  Median latency: 190.00 ns
  Average latency: 241.77 ns
  90%ile latency: 551.00 ns
  99%ile latency: 1193.00 ns
  99.9%ile latency: 1793.00 ns

Results for BoundedList2:
  Total duration: 5.00 seconds
  Total writes: 28054022
  Writes/sec: 5605818.67

Aggregate stats for BoundedList2:
  Median latency: 190.00 ns
  Average latency: 189.04 ns
  90%ile latency: 231.00 ns
  99%ile latency: 611.00 ns
  99.9%ile latency: 1352.00 ns

## 4 Threads

Results for BoundedList:
  Total duration: 5.00 seconds
  Total writes: 18484492
  Writes/sec: 3696753.80

Aggregate stats for BoundedList:
  Median latency: 941.00 ns
  Average latency: 1015.85 ns
  90%ile latency: 1814.00 ns
  99%ile latency: 2906.00 ns
  99.9%ile latency: 7144.00 ns

Results for BoundedList2:
  Total duration: 5.00 seconds
  Total writes: 45429218
  Writes/sec: 9080324.28

Aggregate stats for BoundedList2:
  Median latency: 321.00 ns
  Average latency: 352.19 ns
  90%ile latency: 541.00 ns
  99%ile latency: 832.00 ns
  99.9%ile latency: 1463.00 ns

## 8 Threads

Results for BoundedList:
  Total duration: 5.00 seconds
  Total writes: 18255430
  Writes/sec: 3650825.38

Aggregate stats for BoundedList:
  Median latency: 1062.00 ns
  Average latency: 2143.54 ns
  90%ile latency: 5280.00 ns
  99%ile latency: 13916.00 ns
  99.9%ile latency: 23294.00 ns

Results for BoundedList2:
  Total duration: 5.00 seconds
  Total writes: 49491567
  Writes/sec: 9893488.14

Aggregate stats for BoundedList2:
  Median latency: 561.00 ns
  Average latency: 693.29 ns
  90%ile latency: 1263.00 ns
  99%ile latency: 2495.00 ns
  99.9%ile latency: 3857.00 ns

## 16 Threads

Results for BoundedList:
  Total duration: 5.00 seconds
  Total writes: 11527297
  Writes/sec: 2305231.35

Aggregate stats for BoundedList:
  Median latency: 2454.00 ns
  Average latency: 6891.77 ns
  90%ile latency: 19066.00 ns
  99%ile latency: 40305.00 ns
  99.9%ile latency: 60864.00 ns

Results for BoundedList2:
  Total duration: 5.00 seconds
  Total writes: 47606509
  Writes/sec: 9517945.51

Aggregate stats for BoundedList2:
  Median latency: 1192.00 ns
  Average latency: 1598.17 ns
  90%ile latency: 2975.00 ns
  99%ile latency: 6472.00 ns
  99.9%ile latency: 10199.00 ns

## 32 Threads

Results for BoundedList:
  Total duration: 5.00 seconds
  Total writes: 6419457
  Writes/sec: 1282852.97

Aggregate stats for BoundedList:
  Median latency: 20748.00 ns
  Average latency: 24866.49 ns
  90%ile latency: 48862.00 ns
  99%ile latency: 84247.00 ns
  99.9%ile latency: 118742.00 ns

Results for BoundedList2:
  Total duration: 5.00 seconds
  Total writes: 48251105
  Writes/sec: 9646694.10

Aggregate stats for BoundedList2:
  Median latency: 1893.00 ns
  Average latency: 3238.59 ns
  90%ile latency: 7224.00 ns
  99%ile latency: 20358.00 ns
  99.9%ile latency: 37530.00 ns

## 128 Threads

Results for BoundedList:
  Total duration: 5.01 seconds
  Total writes: 1754933
  Writes/sec: 350174.90

Aggregate stats for BoundedList:
  Median latency: 20799.00 ns
  Average latency: 363713.86 ns
  90%ile latency: 50214.00 ns
  99%ile latency: 10034657.00 ns
  99.9%ile latency: 67065733.00 ns

Results for BoundedList2:
  Total duration: 5.01 seconds
  Total writes: 46257140
  Writes/sec: 9234355.65

Aggregate stats for BoundedList2:
  Median latency: 2254.00 ns
  Average latency: 13698.87 ns
  90%ile latency: 7163.00 ns
  99%ile latency: 18664.00 ns
  99.9%ile latency: 61695.00 ns


## Overview

BL (old): Old BoundedList implementation with atomic shared_ptr
BL (new): new BoundedList implementation with ResourceManager

Impl        Threads     Median lat  99%ile lat  99.9%ile lat    Writes/s
==========================================================================
BL (old)    1           30          341         862             15676225
BL (old)    2           190         1193        1793            5132857
BL (old)    4           941         2906        7144            3696754
BL (old)    8           1062        13916       23294           3650825
BL (old)    16          2454        40305       60864           2305231
BL (old)    32          20748       84247       118742          1282853
BL (old)    128         20799       10034657    67065733        350175
---------------------------------------------------------------------------
BL (new)    1           20          30          751             16688337
BL (new)    2           190         231         611             5605819
BL (new)    4           321         832         1463            9080324
BL (new)    8           561         2495        3857            9893488
BL (new)    16          1192        6472        10199           9517946
BL (new)    32          1893        20358       37530           9646694
BL (new)    128         2254        18664       61695           9234355

