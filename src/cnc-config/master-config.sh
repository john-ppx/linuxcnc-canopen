#!/bin/bash

MASTER_ID=32
SLAVE_ID=48

# 关掉heartbeat
./canopencomm $MASTER_ID w 0x1017 0 i16 0
./canopencomm $SLAVE_ID w 0x1017 0 i16 0

# 配置同步周期
./canopencomm $MASTER_ID w 0x1005 0 u32 0x40000080
./canopencomm $MASTER_ID w 0x1006 0 i32 5000

#配置pdo数据
./canopencomm $MASTER_ID w 0x1800 1 u32 $[0x80000180+$MASTER_ID]
./canopencomm $MASTER_ID w 0x1800 1 u32 $[0x00000200+$SLAVE_ID]

./canopencomm $MASTER_ID w 0x1801 2 u8  0
./canopencomm $MASTER_ID w 0x1801 1 u32 $[0x80000280+$MASTER_ID]
./canopencomm $MASTER_ID w 0x1801 1 u32 $[0x00000300+$SLAVE_ID]

./canopencomm $MASTER_ID w 0x1802 2 u8  0
./canopencomm $MASTER_ID w 0x1802 1 u32 $[0x80000380+$MASTER_ID]
./canopencomm $MASTER_ID w 0x1802 1 u32 $[0x00000400+$SLAVE_ID]
