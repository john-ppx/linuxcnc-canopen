#include "rtapi.h"
#ifdef RTAPI
#include "rtapi_app.h"
#endif
#include "rtapi_string.h"
#include "rtapi_errno.h"
#include "hal.h"
#include "rtapi_math64.h"

#include "test.h"

static int comp_id;

#ifdef MODULE_INFO
MODULE_INFO(linuxcnc, "component:ddt:Compute the derivative of the input function");
MODULE_INFO(linuxcnc, "pin:in:float:0:in::None:None");
MODULE_INFO(linuxcnc, "pin:out:float:0:out::None:None");
MODULE_INFO(linuxcnc, "funct:_:1:");
MODULE_INFO(linuxcnc, "license:GPL");
MODULE_LICENSE("GPL");
#endif // MODULE_INFO


struct __comp_state {
    struct __comp_state *_next;
    hal_float_t *in;
    hal_float_t *out;
    double old;

};
struct __comp_state *__comp_first_inst=0, *__comp_last_inst=0;

static void _(struct __comp_state *__comp_inst, long period);
static int __comp_get_data_size(void);
#undef TRUE
#define TRUE (1)
#undef FALSE
#define FALSE (0)
#undef true
#define true (1)
#undef false
#define false (0)

static int export(char *prefix, long extra_arg) {
    char buf[HAL_NAME_LEN + 1];
    int r = 0;
    int sz = sizeof(struct __comp_state) + __comp_get_data_size();
    struct __comp_state *inst = hal_malloc(sz);
    memset(inst, 0, sz);
    r = hal_pin_float_newf(HAL_IN, &(inst->in), comp_id,
        "%s.in", prefix);
    if(r != 0) return r;
    r = hal_pin_float_newf(HAL_OUT, &(inst->out), comp_id,
        "%s.out", prefix);
    if(r != 0) return r;
    rtapi_snprintf(buf, sizeof(buf), "%s", prefix);
    r = hal_export_funct(buf, (void(*)(void *inst, long))_, inst, 1, 0, comp_id);
    if(r != 0) return r;
    if(__comp_last_inst) __comp_last_inst->_next = inst;
    __comp_last_inst = inst;
    if(!__comp_first_inst) __comp_first_inst = inst;
    return 0;
}
static int default_count=1, count=0;
char *names[16] = {0,};
RTAPI_MP_INT(count, "number of ddt");
RTAPI_MP_ARRAY_STRING(names, 16, "names of ddt");
int rtapi_app_main(void) {
    int r = 0;
    int i;
    comp_id = hal_init("canopen");
    if(comp_id < 0) return comp_id;
    if(count && names[0]) {
        rtapi_print_msg(RTAPI_MSG_ERR,"count= and names= are mutually exclusive\n");
        return -EINVAL;
    }
    test_fun1();
    if(!count && !names[0]) count = default_count;
    if(count) {
        for(i=0; i<count; i++) {
            char buf[HAL_NAME_LEN + 1];
            rtapi_snprintf(buf, sizeof(buf), "ddt.%d", i);
            r = export(buf, i);
            if(r != 0) break;
       }
    } else {
        int max_names = sizeof(names)/sizeof(names[0]);
        for(i=0; (i < max_names) && names[i]; i++) {
            if (strlen(names[i]) < 1) {
                rtapi_print_msg(RTAPI_MSG_ERR, "names[%d] is invalid (empty string)\n", i);
                r = -EINVAL;
                break;
            }
            r = export(names[i], i);
            if(r != 0) break;
       }
    }
    if(r) {
        hal_exit(comp_id);
    } else {
        hal_ready(comp_id);
    }
    return r;
}

void rtapi_app_exit(void) {
    hal_exit(comp_id);
}

#undef FUNCTION
#define FUNCTION(name) static void name(struct __comp_state *__comp_inst, long period)
#undef EXTRA_SETUP
#define EXTRA_SETUP() static int extra_setup(struct __comp_state *__comp_inst, char *prefix, long extra_arg)
#undef EXTRA_CLEANUP
#define EXTRA_CLEANUP() static void extra_cleanup(void)
#undef fperiod
#define fperiod (period * 1e-9)
#undef in
#define in (0+*__comp_inst->in)
#undef out
#define out (*__comp_inst->out)
#undef old
#define old (__comp_inst->old)


FUNCTION(_) {
#line 27 "ddt.comp"
double tmp = in;
out = (tmp - old) / (period * 1e-9);
old = tmp;
    test_fun1();
}

static int __comp_get_data_size(void) { return 0; }
