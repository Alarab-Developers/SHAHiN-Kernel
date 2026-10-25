#ifndef ARP_AST_H
#define ARP_AST_H

#include "محرك_الذاكرة/بوابة_الذاكرة.h"
#include "القلب/المكتبات/المكتبات.h"

/* =========================================================
 * أنواع عُقد AST
 * ========================================================= */
typedef enum
{
    AST_NUMBER = 0,
    AST_STRING,
    AST_VARIABLE,
    AST_FUNCTION_CALL,
    AST_ADD, AST_SUB, AST_MUL, AST_DIV,
    AST_MOD,
    AST_SHL,
    AST_SHR,
    AST_BIT_AND,
    AST_BIT_OR,
    AST_BIT_XOR,
    AST_NEG,
    AST_ASSIGN,
    AST_CMP_EQ,
    AST_CMP_NEQ,
    AST_CMP_LT,
    AST_CMP_LTE,
    AST_CMP_GT,
    AST_CMP_GTE,
    AST_AND,       /* && */
    AST_OR,        /* || */
    AST_INDEX,     /* ← جديد: مصفوفة(فهرس) */
    AST_ARRAY_LIT  /* ← جديد: {a, b, c} */
} ast_node_type_t;

/* =========================================================
 * أنواع ARP
 * ========================================================= */
typedef enum
{
    ARP_TYPE_UNKNOWN = 0,
    ARP_TYPE_INT,
    ARP_TYPE_FLOAT,
    ARP_TYPE_DOUBLE,
    ARP_TYPE_CHAR,
    ARP_TYPE_TEXT,
    ARP_TYPE_BOOL,
    ARP_TYPE_PTR,
    ARP_TYPE_ARRAY  
} arp_type_t;

/* =========================================================
 * arp_array_t — تمثيل المصفوفة
 * ========================================================= */
struct arp_array;
typedef struct arp_array arp_array_t;

/* =========================================================
 * arp_value_t — القيمة الوحيدة
 * ========================================================= */
typedef struct
{
    arp_type_t type;
    union
    {
        int64_t      i64;
        float        f32;
        double       f64;
        uint32_t     cp;
        char        *text;
        uint8_t      b;
        uintptr_t    ptr;
        arp_array_t *array;   /* ← جديد */
    } as;
} arp_value_t;

/* =========================================================
 * عقدة AST
 * ========================================================= */
typedef struct ast_node ast_node_t;

struct ast_node
{
    ast_node_type_t type;

    union
    {
        double number;
        char  *text;
        char  *name;

        struct
        {
            char *name;
            ast_node_t **args;
            int arg_count;
        } function_call;

        struct { ast_node_t *left, *right; } binary;
        struct { ast_node_t *operand; } unary;

        struct
        {
            char *name;
            ast_node_t *value;
        } assignment;

        /* AST_INDEX: target[index...] */
        struct
        {
            char       *name;         /* اسم المتغير */
            ast_node_t **indices;     /* فهارس (1 أو 2) */
            int          index_count;
        } array_index;

        /* AST_ARRAY_LIT: {a, b, c} أو {{...},{...}} */
        struct
        {
            ast_node_t **elements;
            int          element_count;
        } array_lit;
    };
};

/* =========================================================
 * واجهة arp_array_t
 * ========================================================= */
arp_array_t *arp_array_create(arp_type_t elem_type, int dims);
void         arp_array_free(arp_array_t *a);
int          arp_array_dim_count(const arp_array_t *a);
int          arp_array_dim_size(const arp_array_t *a, int dim);
int          arp_array_set(arp_array_t *a, const int *indices, int nidx,
                           const arp_value_t *value);
int          arp_array_get(const arp_array_t *a, const int *indices, int nidx,
                           arp_value_t *out);
int          arp_array_fill_from_literal(arp_array_t *a,
                                         const arp_value_t *flat_values,
                                         int total);

/* =========================================================
 * واجهة arp_value_t
 * ========================================================= */
void  arp_value_init(arp_value_t *v, arp_type_t t);
void  arp_value_free(arp_value_t *v);
int   arp_value_copy(arp_value_t *dst, const arp_value_t *src);

void  arp_value_make_int(arp_value_t *v, int64_t x);
void  arp_value_make_float(arp_value_t *v, float x);
void  arp_value_make_double(arp_value_t *v, double x);
void  arp_value_make_char(arp_value_t *v, uint32_t cp);
void  arp_value_make_bool(arp_value_t *v, int b);
void  arp_value_make_ptr(arp_value_t *v, uintptr_t p);
int   arp_value_make_text(arp_value_t *v, const char *s, int len);
int   arp_value_make_array(arp_value_t *v, arp_array_t *arr); /* ← جديد */

int   arp_value_to_i64 (const arp_value_t *v, int64_t  *out);
int   arp_value_to_f32 (const arp_value_t *v, float    *out);
int   arp_value_to_f64 (const arp_value_t *v, double   *out);
int   arp_value_to_text(const arp_value_t *v, char    **out);

int   arp_value_coerce(const arp_value_t *src, arp_value_t *dst);
int   arp_value_is_assignable(arp_type_t declared, arp_type_t actual);

int arp_value_add(const arp_value_t *a, const arp_value_t *b, arp_value_t *out);
int arp_value_sub(const arp_value_t *a, const arp_value_t *b, arp_value_t *out);
int arp_value_mul(const arp_value_t *a, const arp_value_t *b, arp_value_t *out);
int arp_value_div(const arp_value_t *a, const arp_value_t *b, arp_value_t *out);
int arp_value_mod(const arp_value_t *a, const arp_value_t *b, arp_value_t *out);
int arp_value_shl(const arp_value_t *a, const arp_value_t *b, arp_value_t *out);
int arp_value_shr(const arp_value_t *a, const arp_value_t *b, arp_value_t *out);
int arp_value_bit_and(const arp_value_t *a, const arp_value_t *b, arp_value_t *out);
int arp_value_bit_or (const arp_value_t *a, const arp_value_t *b, arp_value_t *out);
int arp_value_bit_xor(const arp_value_t *a, const arp_value_t *b, arp_value_t *out);
int arp_value_neg(const arp_value_t *a, arp_value_t *out);
int arp_value_cmp(int op, const arp_value_t *a, const arp_value_t *b, int *out);
int arp_value_truthy(const arp_value_t *v);

/* أسماء الأنواع */
arp_type_t arp_type_from_keyword(const char *kw);
int         arp_is_type_keyword(const char *kw);
const char *arp_type_name(arp_type_t t);

/* بناء AST */
ast_node_t *ast_number(double value);
ast_node_t *ast_string(const char *text);
ast_node_t *ast_variable(const char *name);
ast_node_t *ast_function_call(const char *name, ast_node_t **args, int arg_count);
ast_node_t *ast_binary(ast_node_type_t type, ast_node_t *left, ast_node_t *right);
ast_node_t *ast_unary(ast_node_type_t type, ast_node_t *operand);
ast_node_t *ast_assignment(const char *name, ast_node_t *value);
ast_node_t *ast_index(const char *name, ast_node_t **indices, int nidx);  
ast_node_t *ast_array_lit(ast_node_t **elems, int n);                     

void ast_free(ast_node_t *node);
const char *ast_type_name(ast_node_type_t type);
/* تهيئة مصفوفة كاملة من الأبعاد + القيم المسطّحة */
int arp_array_init(arp_array_t *a, int ndims, const int *dims,
                   const arp_value_t *flat_values, int total);

int  arp_array_retain(arp_array_t *a);
void arp_array_release(arp_array_t *a);
#endif /* ARP_AST_H */
