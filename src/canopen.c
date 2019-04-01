#include <stdio.h>
#include <signal.h>

#include <sys/epoll.h>

#include "rtapi.h"
#ifdef RTAPI
#include "rtapi_app.h"
#endif
#include "rtapi_string.h"
#include "rtapi_errno.h"
#include "hal.h"

#include "CANopen.h"
#include "CO_OD_storage.h"
#include "CO_Linux_tasks.h"
#include "application.h"

#include "CO_command.h"

static int comp_id;

struct __canopen_state {
    hal_bit_t *spindle_enable;
    hal_bit_t *spindle_dir;
    hal_float_t *spindle_speed;

    hal_float_t *xpos_cmd;
    hal_float_t *ypos_cmd;
    hal_float_t *zpos_cmd;
            
    bool_t syncWas;
};

struct __canopen_state *canopen_inst = 0;

static void _read(void *inst, long period);
static void _write(void *inst, long period);
static void* rt_thread(void* arg);
static pthread_t            rt_thread_id;

pthread_mutex_t             CO_CAN_VALID_mtx = PTHREAD_MUTEX_INITIALIZER;

#define TMR_TASK_INTERVAL_NS    (1000000)       /* Interval of taskTmr in nanoseconds */
#define TMR_TASK_OVERFLOW_US    (5000)          /* Overflow detect limit for taskTmr in microseconds */
#define INCREMENT_1MS(var)      (var++)         /* Increment 1ms variable in taskTmr */
#undef TRUE
#define TRUE (1)
#undef FALSE
#define FALSE (0)
#undef true
#define true (1)
#undef false
#define false (0)

static int export(char *prefix) {
    char buf[HAL_NAME_LEN + 1];
    int r = 0;

    canopen_inst = hal_malloc(sizeof(struct __canopen_state));
    if (canopen_inst == 0)
        return 1;

    r = hal_pin_float_newf(HAL_IN, &(canopen_inst->spindle_speed),
            comp_id, "%s.spindle_rpm", prefix);
    if(r != 0) return r;
    r = hal_pin_bit_newf(HAL_IN, &(canopen_inst->spindle_enable),
            comp_id, "%s.spindle_enable", prefix);
    if(r != 0) return r;
    r = hal_pin_bit_newf(HAL_IN, &(canopen_inst->spindle_dir),
            comp_id, "%s.spindle_dir", prefix);
    if(r != 0) return r;

    r = hal_pin_float_newf(HAL_IN, &(canopen_inst->xpos_cmd),
            comp_id, "%s.xpos-cmd", prefix);
    if(r != 0) return r;
    r = hal_pin_float_newf(HAL_IN, &(canopen_inst->ypos_cmd),
            comp_id, "%s.ypos-cmd", prefix);
    if(r != 0) return r;
    r = hal_pin_float_newf(HAL_IN, &(canopen_inst->zpos_cmd),
            comp_id, "%s.zpos-cmd", prefix);
    if(r != 0) return r;

    rtapi_snprintf(buf, sizeof(buf), "%s.read", prefix);
    r = hal_export_funct(buf, _read, canopen_inst, 1, 0, comp_id);
    if(r != 0) return r;
    rtapi_snprintf(buf, sizeof(buf), "%s.write", prefix);
    r = hal_export_funct(buf, _write, canopen_inst, 1, 0, comp_id);
    if(r != 0) return r;

    return 0;
}

static int rtPriority = -1;    /* Real time priority, configurable by arguments. (-1=RT disabled) */
static int mainline_epoll_fd;  /* epoll file descriptor for mainline */
static CO_OD_storage_t      odStor;             /* Object Dictionary storage object for CO_OD_ROM */
static CO_OD_storage_t      odStorAuto;         /* Object Dictionary storage object for CO_OD_EEPROM */
static char                *odStorFile_rom    = "od_storage";       /* Name of the file */
static char                *odStorFile_eeprom = "od_storage_auto";  /* Name of the file */
static int nodeId = -1;                /* Use value from Object Dictionary or set to 1..127 by arguments */
volatile sig_atomic_t CO_endProgram = 0;
volatile uint16_t           CO_timer1ms = 0U;
//static int default_count=1, count=0;
//char *names[16] = {0,};
//RTAPI_MP_INT(count, "number of ddt");
RTAPI_MP_STRING(odStorFile_rom, "file of rom data");
RTAPI_MP_STRING(odStorFile_eeprom, "file of eeprom data");

int rtapi_app_main(void) {
    int r = 0;
    int i;

    //char buffer[100];
    //getcwd(buffer,100);
    //printf("The current directoryis:%s\n",buffer);

    comp_id = hal_init("canopen");
    if(comp_id < 0) {return comp_id;
        rtapi_print_msg(RTAPI_MSG_ERR,"CANOPEN:Init Fail\n");
        return comp_id;
    }

    r = export("canopen");
    if (r)
        goto APP_EXIT;

    if (nodeId == -1)
        nodeId = OD_CANNodeID;

    if(nodeId < 1 || nodeId > 127) {
        rtapi_print_msg(RTAPI_MSG_ERR, "CANOPEN: Wrong node ID (%d)\n", nodeId);
        r = -EINVAL;goto APP_EXIT;
    }
    /* Verify, if OD structures have proper alignment of initial values */
    if(CO_OD_RAM.FirstWord != CO_OD_RAM.LastWord) {
        rtapi_print_msg(RTAPI_MSG_ERR, "Program init Error in CO_OD_RAM.\n");
        r = -EINVAL;goto APP_EXIT;
    }
    if(CO_OD_EEPROM.FirstWord != CO_OD_EEPROM.LastWord) {
        rtapi_print_msg(RTAPI_MSG_ERR, "Program init Error in CO_OD_EEPROM.\n");
        r = -EINVAL;goto APP_EXIT;
    }
    if(CO_OD_ROM.FirstWord != CO_OD_ROM.LastWord) {
        rtapi_print_msg(RTAPI_MSG_ERR, "Program init Error in CO_OD_ROM.\n");
        r = -EINVAL;goto APP_EXIT;
    }

    /* Create rt_thread */
    if(pthread_create(&rt_thread_id, NULL, rt_thread, NULL) != 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "Program init - rt_thread creation failed");
        r = -EINVAL;goto APP_EXIT;
    }

    /* Set priority for rt_thread */
    if(rtPriority > 0) {
        struct sched_param param;

        param.sched_priority = rtPriority;
        if(pthread_setschedparam(rt_thread_id, SCHED_FIFO, &param) != 0) {
            rtapi_print_msg(RTAPI_MSG_ERR, "Program init - rt_thread set scheduler failed");
            r = -EINVAL;goto APP_EXIT;
        }
    }

APP_EXIT:
    if(r) {
        hal_exit(comp_id);
    } else {
        hal_ready(comp_id);
    }
    return r;
}

void rtapi_app_exit(void) {
    CO_endProgram = 1;

    CO_command_clear();

    pthread_join(rt_thread_id, NULL);

    /* Store CO_OD_EEPROM */
    CO_OD_storage_autoSave(&odStorAuto, 0, 0);
    CO_OD_storage_autoSaveClose(&odStorAuto);

    CANrx_taskTmr_close();
    taskMain_close();
    CO_delete(0);
    hal_exit(comp_id);
}

/* Helper functions ***********************************************************/
void CO_errExit(char* msg) {
    perror(msg);
    rtapi_app_exit();
}

/* send CANopen generic emergency message */
void CO_error(const uint32_t info) {
    CO_errorReport(CO->em, CO_EM_GENERIC_SOFTWARE_ERROR, CO_EMC_SOFTWARE_INTERNAL, info);
    rtapi_print_msg(RTAPI_MSG_ERR, "CANOPEN: generic error: 0x%X\n", info);
}


#undef FUNCTION
#define FUNCTION(name) static void name(void *inst, long period)

FUNCTION(_read) {
struct __canopen_state *node = (struct __canopen_state*)inst;
    static long old_period = 0;
    static long period_ms = 0;
    static long period_us = 0;

    if (old_period != period) {
        old_period = period;
        period_ms = period / 1000000;
        period_us = period / 1000;
    }

    CO_timer1ms += period_ms;

    CO_LOCK_OD();

    if(CO->CANmodule[0]->CANnormal) {
        /* Process Sync and read inputs */
        node->syncWas = CO_process_SYNC_RPDO(CO, period_us);

        /* Further I/O or nonblocking application code may go here. */

    }

    /* Unlock */
    CO_UNLOCK_OD();
}

FUNCTION(_write) {
struct __canopen_state *node = (struct __canopen_state*)inst;
    static long old_period = 0;
    static long period_us = 0;

    if (old_period != period) {
        old_period = period;
        period_us = period / 1000;
    }


    CO_LOCK_OD();
    if(CO->CANmodule[0]->CANnormal) {
        if (*(node->spindle_enable)) {
            OD_readInput8Bit[0] |= 0x01; 
        } else {
            OD_readInput8Bit[0] &=~0x01; 
        }

        if (*(node->spindle_dir)) {
            OD_readInput8Bit[0] |= 0x02; 
        } else {
            OD_readInput8Bit[0] &=~0x02; 
        }

        OD_spindleRpm = *(node->spindle_speed);
        OD_XPositionCmd = *(node->xpos_cmd);
        OD_YPositionCmd = *(node->ypos_cmd);
        OD_ZPositionCmd = *(node->zpos_cmd);

        /* Write outputs */
        CO_process_TPDO(CO, node->syncWas, period_us);
    }

    CO_UNLOCK_OD();
}

static void* rt_thread(void* arg) {
    CO_NMT_reset_cmd_t reset = CO_RESET_NOT;
    CO_ReturnError_t odStorStatus_rom, odStorStatus_eeprom;
    bool_t firstRun = true;

       /* initialize Object Dictionary storage */
    odStorStatus_rom = CO_OD_storage_init(&odStor, (uint8_t*) &CO_OD_ROM, sizeof(CO_OD_ROM), odStorFile_rom);
    odStorStatus_eeprom = CO_OD_storage_init(&odStorAuto, (uint8_t*) &CO_OD_EEPROM, sizeof(CO_OD_EEPROM), odStorFile_eeprom);

    /* increase variable each startup. Variable is automatically stored in non-volatile memory. */
    OD_powerOnCounter++;

    while(reset != CO_RESET_APP && reset != CO_RESET_QUIT && CO_endProgram == 0) {
        CO_ReturnError_t err;

        pthread_mutex_lock(&CO_CAN_VALID_mtx);

        /* Enter CAN configuration. */
        CO_CANsetConfigurationMode(0);

        err = CO_init(0, nodeId, OD_CANBitRate);
        if(err != CO_ERROR_NO) {
            char s[120];
            snprintf(s, 120, "Communication reset - CANopen initialization failed, err=%d", err);
            CO_errExit(s);
        }

        /* initialize OD objects 1010 and 1011 and verify errors. */
        CO_OD_configure(CO->SDO[0], OD_H1010_STORE_PARAM_FUNC, CO_ODF_1010, (void*)&odStor, 0, 0U);
        CO_OD_configure(CO->SDO[0], OD_H1011_REST_PARAM_FUNC, CO_ODF_1011, (void*)&odStor, 0, 0U);
        if(odStorStatus_rom != CO_ERROR_NO) {
            CO_errorReport(CO->em, CO_EM_NON_VOLATILE_MEMORY, CO_EMC_HARDWARE, (uint32_t)odStorStatus_rom);
        }
        if(odStorStatus_eeprom != CO_ERROR_NO) {
            CO_errorReport(CO->em, CO_EM_NON_VOLATILE_MEMORY, CO_EMC_HARDWARE, (uint32_t)odStorStatus_eeprom + 1000);
        }

        /* Configure callback functions for task control */
        CO_EM_initCallback(CO->em, taskMain_cbSignal);
        CO_SDO_initCallback(CO->SDO[0], taskMain_cbSignal);
        CO_SDOclient_initCallback(CO->SDOclient, taskMain_cbSignal);

        if(firstRun) {
            firstRun = false;
            /* Configure epoll for mainline */
            mainline_epoll_fd = epoll_create(4);
            if(mainline_epoll_fd == -1)
                CO_errExit("Program init - epoll_create mainline failed");

            /* Init mainline */
            taskMain_init(mainline_epoll_fd, &OD_performance[ODA_performance_mainCycleMaxTime]);

            /* Init taskRT */
            CANrx_taskTmr_init(mainline_epoll_fd, TMR_TASK_INTERVAL_NS, &OD_performance[ODA_performance_timerCycleMaxTime]);

            OD_performance[ODA_performance_timerCycleTime] = TMR_TASK_INTERVAL_NS/1000; /* informative */


            rtapi_print_msg(RTAPI_MSG_ERR, "(nodeId=0x%02X) - starting.\n\n", nodeId);

            if(CO_command_init() != 0) {
                CO_errExit("Socket command interface initialization failed");
            }

            /* Execute optional additional application code */
            app_programStart();
        }

        /* Execute optional additional application code */
        app_communicationReset();

        /* start CAN */
        CO_CANsetNormalMode(CO->CANmodule[0]);
        pthread_mutex_unlock(&CO_CAN_VALID_mtx);

        reset = CO_RESET_NOT;

        while(reset == CO_RESET_NOT && CO_endProgram == 0) {
        /* loop for normal program execution ******************************************/
            int ready;
            struct epoll_event ev;

            ready = epoll_wait(mainline_epoll_fd, &ev, 1, -1);
            
            if(ready != 1) {
                if(errno != EINTR) {
                    CO_error(0x11100000L + errno);
                }
            } else if(CANrx_taskTmr_process(ev.data.fd)) {
                /* code was processed in the above function. Additional code process below */

                /* Detect timer large overflow */
                if(OD_performance[ODA_performance_timerCycleMaxTime] > TMR_TASK_OVERFLOW_US && rtPriority > 0) {
                    CO_errorReport(CO->em, CO_EM_ISR_TIMER_OVERFLOW, CO_EMC_SOFTWARE_INTERNAL,
                            0x22400000L | OD_performance[ODA_performance_timerCycleMaxTime]);
                }
            } else if(taskMain_process(ev.data.fd, &reset, CO_timer1ms)) {
                uint16_t timer1msDiff;
                static uint16_t tmr1msPrev = 0;

                /* Calculate time difference */
                timer1msDiff = CO_timer1ms - tmr1msPrev;
                tmr1msPrev = CO_timer1ms;

                /* code was processed in the above function. Additional code process below */

                /* Execute optional additional application code */
                app_programAsync(timer1msDiff);

                CO_OD_storage_autoSave(&odStorAuto, CO_timer1ms, 60000);
            }

            else {
                /* No file descriptor was processed. */
                CO_error(0x11200000L);
            }
        }
    }
    
    rtapi_print_msg(RTAPI_MSG_ERR, "(nodeId=0x%02X) - finished.\n\n", nodeId);
}


