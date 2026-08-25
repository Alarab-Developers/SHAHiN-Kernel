#ifndef انواع_H
#define انواع_H

/* أحجام صحيحة ثابتة */
typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long      uint64_t;

typedef signed char        int8_t;
typedef signed short       int16_t;
typedef signed int         int32_t;
typedef signed long        int64_t;

/* أنواع التعامل مع الأحجام والعناوين */
typedef unsigned long      size_t;
typedef signed long        ssize_t;

typedef unsigned long      uintptr_t;
typedef signed long        intptr_t;

/* قيمة المؤشر الفارغ */
#ifndef NULL
#define NULL ((void *)0)
#endif

/* حساب إزاحة عضو داخل struct */
#define offsetof(type, member) \
    ((size_t)&(((type *)0)->member))

#endif
