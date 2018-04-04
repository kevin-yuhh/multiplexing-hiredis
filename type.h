/******************************************************
# DESC    : self defined data type
# AUTHOR  : Alex Stocks
# VERSION : 1.0
# LICENCE : Apache License 2.0
# EMAIL   : alexstocks@foxmail.com
# MOD     : 2017-10-26 17:18
# FILE    : type.h
******************************************************/

#ifndef __TYPE_H
#define __TYPE_H

#include <assert.h>
#include <stdlib.h>
#include <string.h>        // memcmp, memmove

#define HOST_LEN 64
#define BOOL     int
#define FALSE    0
#define TRUE     1


#define nil                           NULL
#define nil_str                       ""

#define LESS                          (1e-6f)
#define ZERO                          LESS
#define ABS_ZERO                      0
#define LEAST                         (1e-9f)
#define MINUS_LESS                    (-1e-6f)
#define PTR_2_NUM(_ptr_)              ((unsigned long int)(void*)(_ptr_))
#define NUM_2_PTR(_num_)              ((void*)(unsigned long int)(_num_))
#define IS_ZR(_value)                 ((_value) == ABS_ZERO)
#define IS_NZR(_value)                ((_value) != ABS_ZERO)

#define IS_FZ(_value) ({                                  \
    float __zr__ = (float)(_value);                       \
    (MINUS_LESS + 1.0f - 1.0f < (__zr__))                 \
    && ((__zr__)  < LESS + 1.0f - 1.0f);                  \
})

#define IS_NFZ(_value) ({                                 \
    float __zr__ = (float)(_value);                       \
    (__zr__) < (MINUS_LESS + 1.0f - 1.0f)                 \
    || (LESS + 1.0f - 1.0f) < (__zr__);                   \
})

#define IS_MN(_value)                 ((_value) < 0)
#define IS_NMN(_value)                (0 <= (_value))
#define IS_PN(_value)                 (0 < (_value))
#define IS_NPN(_value)                ((_value) <= 0)
#define IS_GT(_value, min)            ((min) < (_value))
#define IS_GE(_value, min)            ((min) <= (_value))
#define IS_EQ(_value, _eq)            ((_value) == (_eq))
#define IS_NEQ(_value, _eq)           ((_value) != (_eq))
#define IS_NL(ptr)                    ((ptr) == NULL)
#define IS_NNL(ptr)                   ((ptr) != NULL)
#define IS_LT(_value, max)            ((_value) < (max))
#define IS_LE(_value, max)            ((_value) <= (max))
#define IS_BT(_value, min, max) ({                 \
    IS_GT(_value, min) && IS_LT(_value,  max);     \
})
#define IS_BE(_value, min, max) ({                 \
    IS_GE(_value, min) && IS_LE(_value, max);      \
})
#define IS_NBT(_value, min, max) ({                \
    IS_LE(_value, min) || IS_GE(_value, max);      \
})
#define IS_NBE(_value, min, max) ({                \
    IS_LT(_value, min) || IS_GT(_value, max);      \
})

#define PTR_OFFSET(ptr0, ptr1) ({                  \
    (IS_LT((void*)(ptr0), (void*)(ptr1)) ?               \
    ((void*)(ptr1) - (void*)(ptr0)) :                    \
    ((void*)(ptr0) - (void*)(ptr1)));                    \
})

#define SIZE_K(x)                    ((x) << 10)
#define SIZE_M(x)                    ((x) << 20)
#define SIZE_G(x)                    ((x) << 30)
#define MAX_BUF_LEN                  SIZE_K(1)
#define SET_ZERO(ptr, size)          memset(ptr, 0, size)

#define ALLOC(ptr, size) ({                       \
    ptr = (typeof(ptr))(malloc(size));            \
})
#define REALLOC(ptr, size) ({                     \
    void* buf = realloc(ptr, size);               \
    ptr = (buf != NULL) ?                         \
        ((typeof(ptr))(buf)) : ptr;               \
    buf;                                          \
})
#define DEALLOC(ptr) ({ free(ptr); ptr = nil;  })

#define ALIGN_SIZE(size, unit_size)              \
    (((size) + (unit_size)-1) & ~((unit_size) - 1))
#define ARRAY_SIZE(array) (sizeof(array) / sizeof(array[0]))
//@t type, @a array, @o offset
#define ARRAY_ELEM_PTR(t, a, o) ((t)(a) + o)
#define ELEM_PTR(type, arr, len) ((type)((u1*)(arr) + len))
#define ELEM_PRE(type, arr, len) ((type)((u1*)(arr) - len))

#define CMP(dst, src, len) ({                    \
    int __cmp_flag__ = -2;                       \
    if (IS_PN((unsigned int)(len))) {            \
        __cmp_flag__ = memcmp(                   \
                            (void*)(dst),        \
                            (void*)(src),        \
                            (size_t)(len)        \
                            );                   \
    }                                            \
    __cmp_flag__;                                \
})

#define CPY(dst, src, len) ({                    \
    int __cpy_flag__ = -1;                       \
    if (IS_NPN(len)) {                           \
        __cpy_flag__ = 0;                        \
    } else {                                     \
        memmove(                                 \
                (void*)(dst),                    \
                (void*)(src),                    \
                (size_t)(len)                    \
                );                               \
        __cpy_flag__ = 0;                        \
    }                                            \
    __cpy_flag__;                                \
})

#define TO_STR(var)                  #var

#ifdef  offsetof
#undef  offsetof
#endif
#if 4 <= __GNUC__
#define offsetof(type, member)        __builtin_offsetof(type, member)
#else
#define offsetof(stt, mem)           (uw)((&((typeof(stt)*)0)->mem))
#endif

#ifndef container_of
#define container_of(p, t, m) ({                 \
    const typeof(((t*)0)->m)* __mptr__ =         \
            (const typeof(((t*)0)->m)*)(p);      \
    (t*)((n1*)__mptr__ - offsetof(t, m));        \
})
#endif
#define st_mem_size(stt, mem)        sizeof(((typeof(stt)*)0)->mem)

//////////////////////////////////////////
//public fuction typedef
//////////////////////////////////////////

#endif

