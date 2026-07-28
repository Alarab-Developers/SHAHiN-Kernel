#ifndef IPC_H
#define IPC_H

#include <stdint.h>

typedef struct process process_t;

#define IPC_MAX_MESSAGES 64
#define IPC_DATA_SIZE    32

typedef enum {
    IPC_START_PROCESS,
    IPC_SLEEP,
    IPC_LAUNCH_APP,
    IPC_EVENT
} IPC_Type;

typedef struct {
    int      sender;
    int      receiver;
    IPC_Type type;
    uint32_t size;
    uint8_t  data[IPC_DATA_SIZE];
} Message;

typedef struct {
    Message messages[IPC_MAX_MESSAGES];
    int head;
    int tail;
    int count;
} ipc_mailbox_t;

#define IPC_MAX_SERVICES 32
#define IPC_SERVICE_NAME 32

typedef struct {
    char name[IPC_SERVICE_NAME];
    int  pid;
} ipc_service_t;

/* ========================================================= */
/* API                                                        */
/* ========================================================= */

void ipc_init_process(process_t* p);

int ipc_send(int target_pid, Message* msg);
int ipc_receive(Message* out_msg);

int ipc_register_service(const char* name);
int ipc_find_service(const char* name);
int ipc_send_service(const char* name, Message* msg);
int ipc_launch_app(const char* app_name);

/*
 * تُستدعى من timer handler أو scheduler
 * لمعالجة طلبات تشغيل التطبيقات المعلّقة
 * من kernel context آمن
 */
void ipc_process_pending_launches(void);

#endif
