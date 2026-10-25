#ifndef PROCESS_API_H
#define PROCESS_API_H

#include "القلب/المكتبات/المكتبات.h"

typedef struct process process_t;

typedef struct {

    // إنشاء عملية
    process_t* (*create)(void (*entry)());
    // بدء عملية
    void (*start)(process_t* p);

    // الحصول على العملية الحالية
    process_t* (*current)();

    // PID
    uint64_t (*get_pid)(process_t* p);

    void (*init)();

    process_t* (*create_ex)(void (*entry)(void*), void* arg);

    void (*set_app_context)(process_t* p, void* context);

    void* (*get_app_context)(process_t* p);

    // إنهاء العملية الحالية
    void (*exit)(void);


} process_api_t;

//  البوابة
extern process_api_t process_api;

#endif
