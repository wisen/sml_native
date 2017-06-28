#ifndef __ATRACE_CONN_H_
#define __ATRACE_CONN_H_

enum atm_cmd {
    ATM_START_SYSTRACE,
    ATM_START_BGREPORT,
    ATM_START_LOGCAT,
    ATM_START_ALL,
    ATM_REINIT_LOGCAT,
    ATM_ENABLE_WRSK,
    ATM_SWITCH_ATRACE,
    ATM_MAX_CMD,
};

enum atm_rsp {
    ATM_FINISH_SYSTRACE,
    ATM_FINISH_BGREPORT,
    ATM_FINISH_LOGCAT,
    ATM_FINISH_ALL,
    ATM_MAX_RSP,
};

void mainloop(void);
int init(void);
int is_wrsk_enabled();
int write_data_toJ(enum atm_cmd cmd);
int init_write_socket();
#endif
