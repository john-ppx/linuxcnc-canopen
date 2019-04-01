/********************************************************************
* Description:  hal_mb002.c
*               This file, 'hal_mb002.c', is a example that shows 
*               how drivers for HAL components will work and serve as 
*               a mb002 for new hardware drivers.
*
* Author: John Kasunich
* License: GPL Version 2
*    
* Copyright (c) 2003 All rights reserved.
*
* Last change: 
********************************************************************/


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <termios.h>

#include "rtapi.h"                /* RTAPI realtime OS API */
#include "rtapi_app.h"                /* RTAPI realtime module decls */

#include "hal.h"                /* HAL public API decls */

/* module information */
MODULE_AUTHOR("Martin Kuhnle");
MODULE_DESCRIPTION("Test Driver for ISA-LED Board for EMC HAL");
MODULE_LICENSE("GPL");
static int count_axis = 3;
RTAPI_MP_INT(count_axis, "config axis count");
static char *dev = "/dev/ttyS0";
RTAPI_MP_STRING(dev, "config usart device");
/* static char *cfg = 0; */
/* config string
RTAPI_MP_STRING(cfg, "config string"); */

/***********************************************************************
*                STRUCTURES AND GLOBAL VARIABLES                       *
************************************************************************/

/* this structure contains the runtime data needed by the
   driver for a single port/channel
*/

typedef struct {
    hal_bit_t *estop;
    hal_float_t *pos_cmd[9];
    hal_float_t pos_old_cmd[9];

    hal_bit_t *spindleEnable;
    hal_bit_t *spindleDir;
    hal_bit_t spindleOldEnable;
    hal_bit_t spindleOldDir;
    hal_float_t *spindleRpm;
    hal_float_t spindleOldRpm;

    int fd;
} mb002_t;

/* pointer to array of mb002_t structs in shared memory, 1 per port */
static mb002_t *port_data_array;

/* other globals */
static int comp_id;                /* component ID */
static int num_ports;                /* number of ports configured */

/***********************************************************************
*                  LOCAL FUNCTION DECLARATIONS                         *
************************************************************************/
/* These is the functions that actually do the I/O
   everything else is just init code
*/
static void write_port(void *arg, long period);
static int usart_init(mb002_t *port);
static int  set_port_attr (int fd,int  baudrate, int databit, const char *stopbit, char parity, int vtime,int vmin);
static void set_baudrate (struct termios *opt, unsigned int baudrate);
static void set_stopbit (struct termios *opt, const char *stopbit);
static void set_data_bit (struct termios *opt, unsigned int databit);
static void set_parity (struct termios *opt, char parity);

/***********************************************************************
*                       INIT AND EXIT CODE                             *
************************************************************************/
int rtapi_app_main(void)
{
    char name[HAL_NAME_LEN + 1];
    int n, retval;

    /* only one port at the moment */
    num_ports = 1;
    n = 0; /* port number */
    if (count_axis > 9) count_axis = 3;


    /* STEP 1: initialise the driver */
    comp_id = hal_init("mb002");
    if (comp_id < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
            "SKELETON: ERROR: hal_init() failed\n");
        return -1;
    }

    /* STEP 2: allocate shared memory for mb002 data */
    port_data_array = hal_malloc(num_ports * sizeof(mb002_t));
    if (port_data_array == 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
            "SKELETON: ERROR: hal_malloc() failed\n");
        hal_exit(comp_id);
        return -1;
    }

    if (usart_init(port_data_array) < 0) {
        return -1;
    }

    /* STEP 3: export the pin(s) */
    for (int i = 0; i < count_axis; i++) {
        retval = hal_pin_float_newf(HAL_IN, &(port_data_array->pos_cmd[i]),
                                   comp_id, "mb002.%d.axis-%d-cmd", n, i);
        if (retval < 0) {
            rtapi_print_msg(RTAPI_MSG_ERR,
            "MB002: ERROR: axis %d cmd %d var export failed with err=%i\n", i, n,
                retval);
            hal_exit(comp_id);
            return -1;
        }

        *port_data_array->pos_cmd[i] = 0;
        port_data_array->pos_old_cmd[i] = 0;
    }

    retval = hal_pin_bit_newf(HAL_IO, &(port_data_array->estop),
            comp_id, "mb002.%d.estop", n);
    if (retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                "MB002: ERROR: estop cmd %d var export failed with err=%i\n", n,
                retval);
        hal_exit(comp_id);
        return -1;
    }
    *port_data_array->estop = 0;

    retval = hal_pin_bit_newf(HAL_IN, &(port_data_array->spindleEnable),
            comp_id, "mb002.%d.spindleEnable", n);
    if (retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                "MB002: ERROR: spindleEnable cmd %d var export failed with err=%i\n", n,
                retval);
        hal_exit(comp_id);
        return -1;
    }

    retval = hal_pin_bit_newf(HAL_IN, &(port_data_array->spindleDir),
            comp_id, "mb002.%d.spindleDir", n);
    if (retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                "MB002: ERROR: spindleDir cmd %d var export failed with err=%i\n", n,
                retval);
        hal_exit(comp_id);
        return -1;
    }

    retval = hal_pin_float_newf(HAL_IN, &(port_data_array->spindleRpm),
            comp_id, "mb002.%d.spindleRpm", n);
    if (retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                "MB002: ERROR: spindleRpm cmd %d var export failed with err=%i\n", n,
                retval);
        hal_exit(comp_id);
        return -1;
    }
    *port_data_array->spindleEnable = 0;
    *port_data_array->spindleDir = 0;
    *port_data_array->spindleRpm = 0;
    port_data_array->spindleOldEnable = 0;
    port_data_array->spindleOldDir = 0;
    port_data_array->spindleOldRpm = 0;

    /* STEP 4: export write function */
    rtapi_snprintf(name, sizeof(name), "mb002.%d.write", n);
    retval = hal_export_funct(name, write_port, &(port_data_array[n]), 0, 0,
        comp_id);
    if (retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
            "SKELETON: ERROR: port %d write funct export failed\n", n);
        hal_exit(comp_id);
        return -1;
    }

    rtapi_print_msg(RTAPI_MSG_INFO,
        "SKELETON: installed driver for %d ports\n", num_ports);
    hal_ready(comp_id);
    return 0;
}

void rtapi_app_exit(void)
{
    hal_exit(comp_id);
}

/**************************************************************
* REALTIME PORT WRITE FUNCTION                                *
**************************************************************/
static char buf[100];
static void write_port(void *arg, long period)
{
    mb002_t *port;
    unsigned char outdata;
    port = arg;
    int len = 0, spindleCmd = 0;

    //if (port->pos_old_cmd[0] != *port->pos_cmd[0]) {
        port->pos_old_cmd[0] = *port->pos_cmd[0];
        len = snprintf(buf, sizeof(buf), "jog x%.4f", port->pos_old_cmd[0]);
    //}

    //if (port->pos_old_cmd[1] != *port->pos_cmd[1]) {
        port->pos_old_cmd[1] = *port->pos_cmd[1];
        if (len == 0)
            len = snprintf(buf, sizeof(buf), "jog y%.4f", port->pos_old_cmd[1]);
        else
            len += snprintf(buf+len, sizeof(buf) - len, " y%.4f", port->pos_old_cmd[1]);
    //}

    //if (port->pos_old_cmd[2] != *port->pos_cmd[2]) {
        port->pos_old_cmd[2] = *port->pos_cmd[2];
        if (len == 0)
            len = snprintf(buf, sizeof(buf), "jog z%.4f", port->pos_old_cmd[2]);
        else
            len += snprintf(buf+len, sizeof(buf) - len, " z%.4f", port->pos_old_cmd[2]);
    //}
    if (len != 0) {
        len += snprintf(buf+len, sizeof(buf) - len, "\n");
        //len = write(port->fd, buf, len); 
        //if (len < 0) {
        //    rtapi_print_msg(RTAPI_MSG_ERR,
        //        "MB002: write buf err %d\n", len);
        //}
    }

    //len = 0;
    if (port->spindleOldEnable != *port->spindleEnable) {
        port->spindleOldEnable = *port->spindleEnable;
        len += snprintf(buf+len, sizeof(buf)-len, "spindle S%d", *port->spindleEnable);
        spindleCmd = 1;
    }

    if (port->spindleOldDir != *port->spindleDir) {
        port->spindleOldDir = *port->spindleDir;
        if (spindleCmd == 0)
            len += snprintf(buf+len, sizeof(buf)-len, "spindle D%d", *port->spindleDir);
        else
            len += snprintf(buf + len, sizeof(buf) - len, " D%d", *port->spindleDir);
        spindleCmd = 1;
    }

    if (port->spindleOldRpm != *port->spindleRpm) {
        port->spindleOldRpm = *port->spindleRpm;
        if (spindleCmd == 0)
            len += snprintf(buf+len, sizeof(buf)-len, "spindle s%.3f", *port->spindleRpm);
        else
            len += snprintf(buf + len, sizeof(buf) - len, " s%.3f", *port->spindleRpm);
        spindleCmd = 1;
    }

    if (spindleCmd != 0)
        len += snprintf(buf+len, sizeof(buf) - len, "\n");

    if (len != 0) {
        len = write(port->fd, buf, len); 
        if (len < 0) {
            rtapi_print_msg(RTAPI_MSG_ERR,
                "MB002: write buf err %d\n", len);
        }
    }
}

static int usart_init(mb002_t *port) {
    port->fd = open(dev, O_RDWR | O_NOCTTY);
    if(port->fd < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                "MB002: ERROR: open dev %s failed\n", dev);
        return -1;
    }

    if (set_port_attr (port->fd, B115200, 8, "1", 'N', 150,255) < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                "MB002: ERROR: set dev %s failed\n", dev);
        close(port->fd);
        return -1;
    }
    
    //if (write(port->fd, "echo $off\n", 10) < 0) {
    //    rtapi_print_msg(RTAPI_MSG_ERR,
    //        "MB002: write buf err");
    //}

    return 0;
}

static int  set_port_attr (int fd,int  baudrate, int  databit, const char *stopbit, char parity, int vtime,int vmin )
{
    struct termios opt;
    tcgetattr(fd, &opt);       //获取初始设置

    set_baudrate(&opt, baudrate);
    set_data_bit(&opt, databit);
    set_parity(&opt, parity);
    set_stopbit(&opt, stopbit);

    opt.c_cflag &= ~CRTSCTS;// 不使用硬件流控制
    opt.c_cflag |= CLOCAL | CREAD; //CLOCAL--忽略 modem 控制线,本地连线, 不具数据机控制功能, CREAD--使能接收标志 

    /*
       IXON--启用输出的 XON/XOFF 流控制
       IXOFF--启用输入的 XON/XOFF 流控制
       IXANY--允许任何字符来重新开始输出
       IGNCR--忽略输入中的回车
     */
    opt.c_iflag &= ~(IXON | IXOFF | IXANY);
    opt.c_oflag &= ~OPOST; //启用输出处理
    /*
       ICANON--启用标准模式 (canonical mode)。允许使用特殊字符 EOF, EOL,
       EOL2, ERASE, KILL, LNEXT, REPRINT, STATUS, 和 WERASE，以及按行的缓冲。
       ECHO--回显输入字符
       ECHOE--如果同时设置了 ICANON，字符 ERASE 擦除前一个输入字符，WERASE 擦除前一个词
       ISIG--当接受到字符 INTR, QUIT, SUSP, 或 DSUSP 时，产生相应的信号
     */
    opt.c_lflag &= ~(ICANON);//~(ICANON | ECHO | ECHOE | ISIG); 
    opt.c_cc[VMIN] = vmin; //设置非规范模式下的超时时长和最小字符数：
    opt.c_cc[VTIME] = vtime; //VTIME与VMIN配合使用，是指限定的传输或等待的最长时间

    tcflush (fd, TCIFLUSH);                    /* TCIFLUSH-- update the options and do it NOW */
    return (tcsetattr (fd, TCSANOW, &opt)); /* TCSANOW--改变立即发生 */
}

//自定义set_baudrate()函数
//使用set_baudrate()函数设置串口输入/输出波特率为115200的代码为：set_baudrate(&opt, B115200));
//通常来说，串口的输入和输出波特率都设置为同一个值,如果要分开设置,就要分别调用cfsetispeed , cfsetospeed
static void set_baudrate (struct termios *opt, unsigned int baudrate)
{
    cfsetispeed(opt, baudrate);
    cfsetospeed(opt, baudrate);
}

//自定义set_stopbit函数
//在set_stopbit()函数中，stopbit参数可以取值为：“1”（1位停止位）、“1.5”（1.5位停止位）和“2”（2位停止位）。
static void set_stopbit (struct termios *opt, const char *stopbit)
{
    if (0 == strcmp (stopbit, "1")) {
        opt->c_cflag &= ~CSTOPB;                                                           /* 1位停止位t             */
    } else if (0 == strcmp (stopbit, "1.5")) {
        opt->c_cflag &= ~CSTOPB;                                                           /* 1.5位停止位            */
    }else if (0 == strcmp (stopbit, "2")) {
        opt->c_cflag |= CSTOPB;                                                            /* 2 位停止位             */
    }else {
        opt->c_cflag &= ~CSTOPB;                                                           /* 1 位停止位             */
    }
}

//set_data_bit函数
//CSIZE--字符长度掩码。取值为 CS5, CS6, CS7, 或 CS8
static void set_data_bit (struct termios *opt, unsigned int databit)
{
    opt->c_cflag &= ~CSIZE;
    switch (databit) {
        case 8:
            opt->c_cflag |= CS8;
            break;
        case 7:
            opt->c_cflag |= CS7;
            break;
        case 6:
            opt->c_cflag |= CS6;
            break;
        case 5:
            opt->c_cflag |= CS5;
            break;
        default:
            opt->c_cflag |= CS8;
            break;
    }
}

//set_parity函数
//在set_parity函数中，parity参数可以取值为：‘N’和‘n’（无奇偶校验）、‘E’和‘e’（表示偶校验）、‘O’和‘o’（表示奇校验）。
static void set_parity (struct termios *opt, char parity)
{
    switch (parity) {
        case 'N':                                                                                   /* 无校验          */
        case 'n':
            opt->c_cflag &= ~PARENB;
            break;
        case 'E':                                                                                   /* 偶校验          */
        case 'e':
            opt->c_cflag |= PARENB;
            opt->c_cflag &= ~PARODD;
            break;
        case 'O':                                                                                   /* 奇校验           */
        case 'o':
            opt->c_cflag |= PARENB;
            opt->c_cflag |= ~PARODD;
            break;
        default:                                                                                    /* 其它选择为无校验 */
            opt->c_cflag &= ~PARENB;
            break;
    }
}


