#!/bin/bash

SDO=./canopencomm
INIVAR=/home/john/work/linuxcnc/linuxcnc/bin/inivar

MASTER_ID=32
SLAVE_ID=48

X_SCALE=$($INIVAR -sec JOINT_0 -var SCALE -ini $1 | gawk -F. '{print $1}')
Y_SCALE=$($INIVAR -sec JOINT_1 -var SCALE -ini $1 | gawk -F. '{print $1}')
Z_SCALE=$($INIVAR -sec JOINT_2 -var SCALE -ini $1 | gawk -F. '{print $1}')

# 关掉heartbeat
$SDO $MASTER_ID w 0x1017 0 i16 0
$SDO $SLAVE_ID w 0x1017 0 i16 0

# 配置同步周期
$SDO $MASTER_ID w 0x1005 0 u32 0x40000080
$SDO $MASTER_ID w 0x1006 0 i32 5000

# 配置各轴参数
$SDO $SLAVE_ID w 0x6091 0 u32 $X_SCALE
$SDO $SLAVE_ID w 0x6891 0 u32 $Y_SCALE
$SDO $SLAVE_ID w 0x7091 0 u32 $Z_SCALE

#配置pdo数据
$SDO $MASTER_ID w 0x1800 1 u32 $[0x80000180+$MASTER_ID]
$SDO $MASTER_ID w 0x1800 1 u32 $[0x00000200+$SLAVE_ID]

$SDO $MASTER_ID w 0x1801 1 u32 $[0x80000280+$MASTER_ID]
$SDO $MASTER_ID w 0x1801 1 u32 $[0x00000300+$SLAVE_ID]
$SDO $MASTER_ID w 0x1401 2 u8  0
$SDO $MASTER_ID w 0x1801 2 u8  0

$SDO $MASTER_ID w 0x1802 1 u32 $[0x80000380+$MASTER_ID]
$SDO $MASTER_ID w 0x1802 1 u32 $[0x00000400+$SLAVE_ID]
$SDO $MASTER_ID w 0x1402 2 u8  0
$SDO $MASTER_ID w 0x1802 2 u8  0

$SDO $SLAVE_ID  w 0x1800 1 u32 $[0x80000180+$SLAVE_ID]
$SDO $SLAVE_ID  w 0x1800 1 u32 $[0x00000200+$MASTER_ID]

# 复位从站数据,解决不关闭从站,重新打开linuxcnc时轴移动的问题
$SDO $SLAVE_ID stop
sleep 1
$SDO $SLAVE_ID start

