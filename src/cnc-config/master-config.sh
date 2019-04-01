#!/bin/bash

MASTER_ID=32
SLAVE_ID=48

# 关掉heartbeat
./canopencomm $MASTER_ID w 0x1017 0 i16 0
./canopencomm $SLAVE_ID w 0x1017 0 i16 0

#配置pdo数据
./canopencomm $MASTER_ID w 0x1800 1 u32 $[0x80000180+$MASTER_ID]
./canopencomm $MASTER_ID w 0x1800 1 u32 $[0x00000200+$SLAVE_ID]

