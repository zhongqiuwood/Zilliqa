#!/bin/bash

#ZILLIQA_TOP=`dirname $0`
#ZILLIQA_BUILD_TOP=/Users/oak/go/src/github.com/zhongqiuwood/Zilliqa/build
ZILLIQA_TOP=/Users/oak/go/src/github.com/zhongqiuwood/Zilliqa

# clean up persistent storage
rm -rf ${ZILLIQA_BUILD_TOP}/local_run/node*

cp ${ZILLIQA_TOP}/tests/Zilliqa/test_zilliqa_local.py ${ZILLIQA_BUILD_TOP}/tests/Zilliqa/test_zilliqa_local.py

#sudo sysctl net.core.somaxconn=102400;
#sudo sysctl net.core.netdev_max_backlog=65536;
#sudo sysctl net.ipv4.tcp_tw_reuse=1;
#sudo sysctl -w net.ipv4.tcp_rmem='65536 873800 1534217728';
#sudo sysctl -w net.ipv4.tcp_wmem='65536 873800 1534217728';
#sudo sysctl -w net.ipv4.tcp_mem='65536 873800 1534217728';

NUM_SETUP=2
let NUM_START=${NUM_SETUP}/2
let NUM_START_PLUS_ONE=${NUM_START}+1

echo "NUM_START=${NUM_START}"
echo "NUM_SETUP=${NUM_SETUP}"
echo "NUM_START_PLUS_ONE=${NUM_START_PLUS_ONE}"


echo "=========================================="
echo "stop..."
python ${ZILLIQA_BUILD_TOP}/tests/Zilliqa/test_zilliqa_local.py stop

echo "=========================================="
echo "setup..."
python ${ZILLIQA_BUILD_TOP}/tests/Zilliqa/test_zilliqa_local.py setup ${NUM_SETUP}


echo "=========================================="
echo "start..."
python ${ZILLIQA_BUILD_TOP}/tests/Zilliqa/test_zilliqa_local.py start ${NUM_START}

sleep 6


echo "=========================================="
echo "sendcmd..."
for ds in `seq 1 ${NUM_SETUP}`
    do
        python ${ZILLIQA_BUILD_TOP}/tests/Zilliqa/test_zilliqa_local.py sendcmd $ds 01000000000000000000000000000100007F00001389
    done
sleep 6

echo "=========================================="
echo "startpow1..."
for node in `seq ${NUM_START_PLUS_ONE} ${NUM_SETUP}`
    do
        python ${ZILLIQA_BUILD_TOP}/tests/Zilliqa/test_zilliqa_local.py startpow1 $node ${NUM_SETUP} 0000000000000000000000000000000000000000000000000000000000000001 03 2b740d75891749f94b6a8ec09f086889066608e4418eda656c93443e8310750a e8cc9106f8a28671d91e2de07b57b828934481fadf6956563b963bb8e5c266bf
    done
sleep 6


echo "=========================================="
echo "sendtxn..."
for port in `seq 1 ${NUM_SETUP}`
do
    python ${ZILLIQA_BUILD_TOP}/tests/Zilliqa/test_zilliqa_local.py sendtxn 500$port
done




#for ds in {1..${NUM_SETUP}}
#do
#    python ${ZILLIQA_BUILD_TOP}/tests/Zilliqa/test_zilliqa_local.py sendcmd $ds 01000000000000000000000000000100007F00001389
#done
#sleep 10

# PoW1 submission should be multicasted to all DS committee members
#for node in {${NUM_START_PLUS_ONE}..${NUM_SETUP}}
#do
#    python ${ZILLIQA_BUILD_TOP}/tests/Zilliqa/test_zilliqa_local.py startpow1 $node ${NUM_SETUP} 0000000000000000000000000000000000000000000000000000000000000001 03 2b740d75891749f94b6a8ec09f086889066608e4418eda656c93443e8310750a e8cc9106f8a28671d91e2de07b57b828934481fadf6956563b963bb8e5c266bf
#done
#for port in {01..0${NUM_SETUP}}
#do
#    python ${ZILLIQA_BUILD_TOP}/tests/Zilliqa/test_zilliqa_local.py sendtxn 50$port
#done