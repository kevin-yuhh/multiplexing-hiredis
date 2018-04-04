/*
 * Copyright (c) 2009-2011, Salvatore Sanfilippo <antirez at gmail dot com>
 * Copyright (c) 2010-2014, Pieter Noordhuis <pcnoordhuis at gmail dot com>
 * Copyright (c) 2015, Matt Stancliff <matt at genges dot com>,
 *                     Jan-Erik Rediger <janerik at fnordig dot com>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "fmacros.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <ctype.h>
#include <pthread.h>
#include <limits.h>
#include <semaphore.h> 
#include <fcntl.h>

#include "hiredis.h"
#include "net.h"
#include "sds.h"
#include "dict.c"

#include "list.h"
#include "async.h"
#include "adapters/ae.h"

#define UNUSED(V) ((void) V)

/* added  by kevin  */
#define EVENT_SIZE 1024
#define WAIT_TIME_OUT 5
#define QUEUE_SIZE 1024
#define MAX_GET_REQ_NUM 100

typedef struct redisClient {
    aeEventLoop *aeloop;   /* ae loop */
    list *reconn_list;
    list *free_ac_list;
    pthread_mutex_t mutex; /* global mutex */
    pthread_mutex_t reconn_mutex; /* global mutex */
    const char *pe;
    int timeout;

} redisClient;

/* Global vars */             
struct redisClient client = { NULL, NULL,NULL, PTHREAD_MUTEX_INITIALIZER, PTHREAD_MUTEX_INITIALIZER, "", WAIT_TIME_OUT}; /* server global state */

int redisAsyncCommandHelper(redisAsyncContext *ac, redisCallbackFn *fn, void *privdata, const char *cmd, size_t len);
void redisAsyncCopyErrorHelper(redisAsyncContext *ac);
void xRedisAsyncFree(redisAsyncContext *ac);
redisAsyncContext *redisAsyncInitializeHelper(redisContext *c); 

typedef struct cmdInfo {
    const char* cmd;
    int len;
    redisReply* reply;
    int reference;
    sem_t reply_sem;
}cmdInfo;
/*
typedef struct reqData {
    redisAsyncContext* ac;
    redisCallbackFn *fn;
    const char* cmd;
    int len;
    redisReply* reply;
    int session_id;
    uint64_t req_time;

    pthread_mutex_t mutex;
    pthread_cond_t cond;
}reqData;
*/
/* end added */
static unsigned int callbackSessionHash(const void *key) {
    unsigned int hash = 5381;
    hash = ((hash << 5) + hash) + (int)key; /* hash * 33 + c */
    return hash;
}

static int callbackSessionKeyCompare(void *privdata, const void *key1, const void *key2) {
    ((void) privdata);

    const int l1 = (int)key1;
    const int l2 = (int)key2;
    return l1 == l2;
}

static dictType callbackSessionDict = {
    callbackSessionHash,
    NULL,
    NULL,
    callbackSessionKeyCompare,
    NULL,
    NULL
    //callbackSessionValDestructor
};

static redisReply *createReplyObject(int type);
static void *createStringObject(const redisReadTask *task, char *str, size_t len);
static void *createArrayObject(const redisReadTask *task, int elements);
static void *createIntegerObject(const redisReadTask *task, long long value);
static void *createNilObject(const redisReadTask *task);
static int __xRedisReconnectConnectNonBlock(xRedisContext *xc); 

static int isValidIP(const char *str)
{
    int i, a[4];
    char end;
    if( sscanf(str, "%d.%d.%d.%d%c", &a[0], &a[1], &a[2], &a[3], &end) != 4 ) {
        return 0;
    }

    for(i=0; i<4; i++) if (a[i] < 0 || a[i] > 255) {
        return 0;
    }
    return 1;
}

void replyCallback(redisAsyncContext *ac, void *r, void *privdata) {
    UNUSED(ac);

    redisReply *reply = (redisReply*)r;
    if (privdata == NULL ) {
        return;
    } 

    cmdInfo *ci = privdata;
    if ( __sync_sub_and_fetch(&ci->reference, 1)<= 0) {
        sem_destroy(&ci->reply_sem);
        free(ci);
        freeReplyObject(reply);
        return;
    }

    ci->reply = reply;
    sem_post(&ci->reply_sem);
    return;
}

void connectedCallback(redisAsyncContext *ac, int status) {
    
    pthread_mutex_lock(&ac->xc->mutex);
    if (status != REDIS_OK) {
        LOG("ac:%p Error: %s err:%d\n",ac,ac->c.errstr, ac->c.err);
    } else {
        __sync_lock_test_and_set(&ac->xc->status,REDIS_CONNECTED);
        LOG("Connected... fd=%d xc:%p\n", ac->c.fd, ac->xc);
    }
    pthread_cond_signal(&ac->xc->cond);
    pthread_mutex_unlock(&ac->xc->mutex);
}

void disconnectedCallback(redisAsyncContext *ac, int status) {
    if (status != REDIS_OK) {
        LOG("-Error: %s status:%d\n",ac->errstr, status);

        xRedisContext *xc = ac->xc;
        pthread_mutex_lock(&xc->mutex);
        /* clear cmd_list */
        cmdInfo *ci = NULL;
        listNode *node = listFirst(ac->cmd_list);
        listNode *next_node = node;
        while (node != NULL) {
            ci = listNodeValue(node); 
            next_node = node->next;
            listDelNode(ac->cmd_list, node);
            if (__sync_sub_and_fetch(&ci->reference, 1) == 0) {
                sem_destroy(&ci->reply_sem);
                free(ci);
            }
            node = next_node;
        }
        __sync_lock_test_and_set(&xc->status,REDIS_DISCONNECTING);
        pthread_mutex_unlock(&xc->mutex);
        
        pthread_mutex_lock(&client.mutex);
        if (ac->status == FREED_CONTEXT) {
            pthread_mutex_unlock(&client.mutex);
            return ;    
        }
        pthread_mutex_unlock(&client.mutex);

        pthread_mutex_lock(&client.reconn_mutex);
        if (listSearchKey(client.reconn_list,xc) == NULL) {
            LOG("Add reconn xc:%p\n", xc);
            listAddNodeTail(client.reconn_list,xc);
        }
        pthread_mutex_unlock(&client.reconn_mutex);
        LOG("Disconnected... add to reconn list fd=%d ac:%p Error:%s\n",ac->c.fd,ac, ac->errstr);
    } else {
        LOG("Disconnected...fd=%d xc:%p\n",ac->c.fd,ac->xc);
    }
}
/* Default set of functions to build the reply. Keep in mind that such a
 * function returning NULL is interpreted as OOM. */
static redisReplyObjectFunctions defaultFunctions = {
    createStringObject,
    createArrayObject,
    createIntegerObject,
    createNilObject,
    freeReplyObject
};

/* Create a reply object */
static redisReply *createReplyObject(int type) {
    redisReply *r = calloc(1,sizeof(*r));

    if (r == NULL)
        return NULL;

    r->type = type;
    return r;
}

/* Free a reply object */
void freeReplyObject(void *reply) {
    redisReply *r = reply;
    size_t j;

    if (r == NULL)
        return;

    switch(r->type) {
    case REDIS_REPLY_INTEGER:
        break; /* Nothing to free */
    case REDIS_REPLY_ARRAY:
        if (r->element != NULL) {
            for (j = 0; j < r->elements; j++)
                if (r->element[j] != NULL)
                    freeReplyObject(r->element[j]);
            free(r->element);
        }
        break;
    case REDIS_REPLY_ERROR:
    case REDIS_REPLY_STATUS:
    case REDIS_REPLY_STRING:
        if (r->str != NULL)
            free(r->str);
        break;
    }
    free(r);
}

static void *createStringObject(const redisReadTask *task, char *str, size_t len) {
    redisReply *r, *parent;
    char *buf;

    r = createReplyObject(task->type);
    if (r == NULL)
        return NULL;

    buf = malloc(len+1);
    if (buf == NULL) {
        freeReplyObject(r);
        return NULL;
    }

    assert(task->type == REDIS_REPLY_ERROR  ||
           task->type == REDIS_REPLY_STATUS ||
           task->type == REDIS_REPLY_STRING);

    /* Copy string value */
    memcpy(buf,str,len);
    buf[len] = '\0';
    r->str = buf;
    r->len = len;

    if (task->parent) {
        parent = task->parent->obj;
        assert(parent->type == REDIS_REPLY_ARRAY);
        parent->element[task->idx] = r;
    }
    return r;
}

static void *createArrayObject(const redisReadTask *task, int elements) {
    redisReply *r, *parent;

    r = createReplyObject(REDIS_REPLY_ARRAY);
    if (r == NULL)
        return NULL;

    if (elements > 0) {
        r->element = calloc(elements,sizeof(redisReply*));
        if (r->element == NULL) {
            freeReplyObject(r);
            return NULL;
        }
    }

    r->elements = elements;

    if (task->parent) {
        parent = task->parent->obj;
        assert(parent->type == REDIS_REPLY_ARRAY);
        parent->element[task->idx] = r;
    }
    return r;
}

static void *createIntegerObject(const redisReadTask *task, long long value) {
    redisReply *r, *parent;

    r = createReplyObject(REDIS_REPLY_INTEGER);
    if (r == NULL)
        return NULL;

    r->integer = value;

    if (task->parent) {
        parent = task->parent->obj;
        assert(parent->type == REDIS_REPLY_ARRAY);
        parent->element[task->idx] = r;
    }
    return r;
}

static void *createNilObject(const redisReadTask *task) {
    redisReply *r, *parent;

    r = createReplyObject(REDIS_REPLY_NIL);
    if (r == NULL)
        return NULL;

    if (task->parent) {
        parent = task->parent->obj;
        assert(parent->type == REDIS_REPLY_ARRAY);
        parent->element[task->idx] = r;
    }
    return r;
}

/* Return the number of digits of 'v' when converted to string in radix 10.
 * Implementation borrowed from link in redis/src/util.c:string2ll(). */
static uint32_t countDigits(uint64_t v) {
  uint32_t result = 1;
  for (;;) {
    if (v < 10) return result;
    if (v < 100) return result + 1;
    if (v < 1000) return result + 2;
    if (v < 10000) return result + 3;
    v /= 10000U;
    result += 4;
  }
}

/* Helper that calculates the bulk length given a certain string length. */
static size_t bulklen(size_t len) {
    return 1+countDigits(len)+2+len+2;
}

int redisvFormatCommand(char **target, const char *format, va_list ap) {
    const char *c = format;
    char *cmd = NULL; /* final command */
    int pos; /* position in final command */
    sds curarg, newarg; /* current argument */
    int touched = 0; /* was the current argument touched? */
    char **curargv = NULL, **newargv = NULL;
    int argc = 0;
    int totlen = 0;
    int error_type = 0; /* 0 = no error; -1 = memory error; -2 = format error */
    int j;

    /* Abort if there is not target to set */
    if (target == NULL)
        return -1;

    /* Build the command string accordingly to protocol */
    curarg = sdsempty();
    if (curarg == NULL)
        return -1;

    while(*c != '\0') {
        if (*c != '%' || c[1] == '\0') {
            if (*c == ' ') {
                if (touched) {
                    newargv = realloc(curargv,sizeof(char*)*(argc+1));
                    if (newargv == NULL) goto memory_err;
                    curargv = newargv;
                    curargv[argc++] = curarg;
                    totlen += bulklen(sdslen(curarg));

                    /* curarg is put in argv so it can be overwritten. */
                    curarg = sdsempty();
                    if (curarg == NULL) goto memory_err;
                    touched = 0;
                }
            } else {
                newarg = sdscatlen(curarg,c,1);
                if (newarg == NULL) goto memory_err;
                curarg = newarg;
                touched = 1;
            }
        } else {
            char *arg;
            size_t size;

            /* Set newarg so it can be checked even if it is not touched. */
            newarg = curarg;

            switch(c[1]) {
            case 's':
                arg = va_arg(ap,char*);
                size = strlen(arg);
                if (size > 0)
                    newarg = sdscatlen(curarg,arg,size);
                break;
            case 'b':
                arg = va_arg(ap,char*);
                size = va_arg(ap,size_t);
                if (size > 0)
                    newarg = sdscatlen(curarg,arg,size);
                break;
            case '%':
                newarg = sdscat(curarg,"%");
                break;
            default:
                /* Try to detect printf format */
                {
                    static const char intfmts[] = "diouxX";
                    static const char flags[] = "#0-+ ";
                    char _format[16];
                    const char *_p = c+1;
                    size_t _l = 0;
                    va_list _cpy;

                    /* Flags */
                    while (*_p != '\0' && strchr(flags,*_p) != NULL) _p++;

                    /* Field width */
                    while (*_p != '\0' && isdigit(*_p)) _p++;

                    /* Precision */
                    if (*_p == '.') {
                        _p++;
                        while (*_p != '\0' && isdigit(*_p)) _p++;
                    }

                    /* Copy va_list before consuming with va_arg */
                    va_copy(_cpy,ap);

                    /* Integer conversion (without modifiers) */
                    if (strchr(intfmts,*_p) != NULL) {
                        va_arg(ap,int);
                        goto fmt_valid;
                    }

                    /* Double conversion (without modifiers) */
                    if (strchr("eEfFgGaA",*_p) != NULL) {
                        va_arg(ap,double);
                        goto fmt_valid;
                    }

                    /* Size: char */
                    if (_p[0] == 'h' && _p[1] == 'h') {
                        _p += 2;
                        if (*_p != '\0' && strchr(intfmts,*_p) != NULL) {
                            va_arg(ap,int); /* char gets promoted to int */
                            goto fmt_valid;
                        }
                        goto fmt_invalid;
                    }

                    /* Size: short */
                    if (_p[0] == 'h') {
                        _p += 1;
                        if (*_p != '\0' && strchr(intfmts,*_p) != NULL) {
                            va_arg(ap,int); /* short gets promoted to int */
                            goto fmt_valid;
                        }
                        goto fmt_invalid;
                    }

                    /* Size: long long */
                    if (_p[0] == 'l' && _p[1] == 'l') {
                        _p += 2;
                        if (*_p != '\0' && strchr(intfmts,*_p) != NULL) {
                            va_arg(ap,long long);
                            goto fmt_valid;
                        }
                        goto fmt_invalid;
                    }

                    /* Size: long */
                    if (_p[0] == 'l') {
                        _p += 1;
                        if (*_p != '\0' && strchr(intfmts,*_p) != NULL) {
                            va_arg(ap,long);
                            goto fmt_valid;
                        }
                        goto fmt_invalid;
                    }

                fmt_invalid:
                    va_end(_cpy);
                    goto format_err;

                fmt_valid:
                    _l = (_p+1)-c;
                    if (_l < sizeof(_format)-2) {
                        memcpy(_format,c,_l);
                        _format[_l] = '\0';
                        newarg = sdscatvprintf(curarg,_format,_cpy);

                        /* Update current position (note: outer blocks
                         * increment c twice so compensate here) */
                        c = _p-1;
                    }

                    va_end(_cpy);
                    break;
                }
            }

            if (newarg == NULL) goto memory_err;
            curarg = newarg;

            touched = 1;
            c++;
        }
        c++;
    }

    /* Add the last argument if needed */
    if (touched) {
        newargv = realloc(curargv,sizeof(char*)*(argc+1));
        if (newargv == NULL) goto memory_err;
        curargv = newargv;
        curargv[argc++] = curarg;
        totlen += bulklen(sdslen(curarg));
    } else {
        sdsfree(curarg);
    }

    /* Clear curarg because it was put in curargv or was free'd. */
    curarg = NULL;

    /* Add bytes needed to hold multi bulk count */
    totlen += 1+countDigits(argc)+2;

    /* Build the command at protocol level */
    cmd = malloc(totlen+1);
    if (cmd == NULL) goto memory_err;

    pos = sprintf(cmd,"*%d\r\n",argc);
    for (j = 0; j < argc; j++) {
        pos += sprintf(cmd+pos,"$%zu\r\n",sdslen(curargv[j]));
        memcpy(cmd+pos,curargv[j],sdslen(curargv[j]));
        pos += sdslen(curargv[j]);
        sdsfree(curargv[j]);
        cmd[pos++] = '\r';
        cmd[pos++] = '\n';
    }
    assert(pos == totlen);
    cmd[pos] = '\0';

    free(curargv);
    *target = cmd;
    return totlen;

format_err:
    error_type = -2;
    goto cleanup;

memory_err:
    error_type = -1;
    goto cleanup;

cleanup:
    if (curargv) {
        while(argc--)
            sdsfree(curargv[argc]);
        free(curargv);
    }

    sdsfree(curarg);

    /* No need to check cmd since it is the last statement that can fail,
     * but do it anyway to be as defensive as possible. */
    if (cmd != NULL)
        free(cmd);

    return error_type;
}

/* Format a command according to the Redis protocol. This function
 * takes a format similar to printf:
 *
 * %s represents a C null terminated string you want to interpolate
 * %b represents a binary safe string
 *
 * When using %b you need to provide both the pointer to the string
 * and the length in bytes as a size_t. Examples:
 *
 * len = redisFormatCommand(target, "GET %s", mykey);
 * len = redisFormatCommand(target, "SET %s %b", mykey, myval, myvallen);
 */
int redisFormatCommand(char **target, const char *format, ...) {
    va_list ap;
    int len;
    va_start(ap,format);
    len = redisvFormatCommand(target,format,ap);
    va_end(ap);

    /* The API says "-1" means bad result, but we now also return "-2" in some
     * cases.  Force the return value to always be -1. */
    if (len < 0)
        len = -1;

    return len;
}

/* Format a command according to the Redis protocol using an sds string and
 * sdscatfmt for the processing of arguments. This function takes the
 * number of arguments, an array with arguments and an array with their
 * lengths. If the latter is set to NULL, strlen will be used to compute the
 * argument lengths.
 */
int redisFormatSdsCommandArgv(sds *target, int argc, const char **argv,
                              const size_t *argvlen)
{
    sds cmd;
    unsigned long long totlen;
    int j;
    size_t len;

    /* Abort on a NULL target */
    if (target == NULL)
        return -1;

    /* Calculate our total size */
    totlen = 1+countDigits(argc)+2;
    for (j = 0; j < argc; j++) {
        len = argvlen ? argvlen[j] : strlen(argv[j]);
        totlen += bulklen(len);
    }

    /* Use an SDS string for command construction */
    cmd = sdsempty();
    if (cmd == NULL)
        return -1;

    /* We already know how much storage we need */
    cmd = sdsMakeRoomFor(cmd, totlen);
    if (cmd == NULL)
        return -1;

    /* Construct command */
    cmd = sdscatfmt(cmd, "*%i\r\n", argc);
    for (j=0; j < argc; j++) {
        len = argvlen ? argvlen[j] : strlen(argv[j]);
        cmd = sdscatfmt(cmd, "$%u\r\n", len);
        cmd = sdscatlen(cmd, argv[j], len);
        cmd = sdscatlen(cmd, "\r\n", sizeof("\r\n")-1);
    }

    assert(sdslen(cmd)==totlen);

    *target = cmd;
    return totlen;
}

void redisFreeSdsCommand(sds cmd) {
    sdsfree(cmd);
}

/* Format a command according to the Redis protocol. This function takes the
 * number of arguments, an array with arguments and an array with their
 * lengths. If the latter is set to NULL, strlen will be used to compute the
 * argument lengths.
 */
int redisFormatCommandArgv(char **target, int argc, const char **argv, const size_t *argvlen) {
    char *cmd = NULL; /* final command */
    int pos; /* position in final command */
    size_t len;
    int totlen, j;

    /* Abort on a NULL target */
    if (target == NULL)
        return -1;

    /* Calculate number of bytes needed for the command */
    totlen = 1+countDigits(argc)+2;
    for (j = 0; j < argc; j++) {
        len = argvlen ? argvlen[j] : strlen(argv[j]);
        totlen += bulklen(len);
    }

    /* Build the command at protocol level */
    cmd = malloc(totlen+1);
    if (cmd == NULL)
        return -1;

    pos = sprintf(cmd,"*%d\r\n",argc);
    for (j = 0; j < argc; j++) {
        len = argvlen ? argvlen[j] : strlen(argv[j]);
        pos += sprintf(cmd+pos,"$%zu\r\n",len);
        memcpy(cmd+pos,argv[j],len);
        pos += len;
        cmd[pos++] = '\r';
        cmd[pos++] = '\n';
    }
    assert(pos == totlen);
    cmd[pos] = '\0';

    *target = cmd;
    return totlen;
}

void redisFreeCommand(char *cmd) {
    free(cmd);
}

void __redisSetError(redisContext *c, int type, const char *str) {
    size_t len;

    c->err = type;
    if (str != NULL) {
        len = strlen(str);
        len = len < (sizeof(c->errstr)-1) ? len : (sizeof(c->errstr)-1);
        memcpy(c->errstr,str,len);
        c->errstr[len] = '\0';
    } else {
        /* Only REDIS_ERR_IO may lack a description! */
        assert(type == REDIS_ERR_IO);
        __redis_strerror_r(errno, c->errstr, sizeof(c->errstr));
    }
}

redisReader *redisReaderCreate(void) {
    return redisReaderCreateWithFunctions(&defaultFunctions);
}

static redisContext *redisContextInit(void) {
    redisContext *c;

    c = calloc(1,sizeof(redisContext));
    if (c == NULL)
        return NULL;

    c->err = 0;
    c->errstr[0] = '\0';
    c->obuf = sdsempty();
    c->reader = redisReaderCreate();
    c->tcp.host = NULL;
    c->tcp.source_addr = NULL;
    c->unix_sock.path = NULL;
    c->timeout = NULL;

    if (c->obuf == NULL || c->reader == NULL) {
        redisFree(c);
        return NULL;
    }

    return c;
}

void redisFree(redisContext *c) {
    if (c == NULL)
        return;
    if (c->fd > 0)
        close(c->fd);
    if (c->obuf != NULL)
        sdsfree(c->obuf);
    if (c->reader != NULL)
        redisReaderFree(c->reader);
    if (c->tcp.host)
        free(c->tcp.host);
    if (c->tcp.source_addr)
        free(c->tcp.source_addr);
    if (c->unix_sock.path)
        free(c->unix_sock.path);
    if (c->timeout)
        free(c->timeout);
    free(c);
}

int redisFreeKeepFd(redisContext *c) {
    int fd = c->fd;
    c->fd = -1;
    redisFree(c);
    return fd;
}

int redisReconnect(redisContext *c) {
    c->err = 0;
    memset(c->errstr, '\0', strlen(c->errstr));

    if (c->fd > 0) {
        close(c->fd);
    }

    sdsfree(c->obuf);
    redisReaderFree(c->reader);

    c->obuf = sdsempty();
    c->reader = redisReaderCreate();

    if (c->connection_type == REDIS_CONN_TCP) {
        return redisContextConnectBindTcp(c, c->tcp.host, c->tcp.port,
                c->timeout, c->tcp.source_addr);
    } else if (c->connection_type == REDIS_CONN_UNIX) {
        return redisContextConnectUnix(c, c->unix_sock.path, c->timeout);
    } else {
        /* Something bad happened here and shouldn't have. There isn't
           enough information in the context to reconnect. */
        __redisSetError(c,REDIS_ERR_OTHER,"Not enough information to reconnect");
    }

    return REDIS_ERR;
}

/* Connect to a Redis instance. On error the field error in the returned
 * context will be set to the return value of the error function.
 * When no set of reply functions is given, the default set will be used. */
redisContext *redisConnect(const char *ip, int port) {
    redisContext *c;

    c = redisContextInit();
    if (c == NULL)
        return NULL;

    c->flags |= REDIS_BLOCK;
    redisContextConnectTcp(c,ip,port,NULL);
    return c;
}

redisContext *redisConnectWithTimeout(const char *ip, int port, const struct timeval tv) {
    redisContext *c;

    c = redisContextInit();
    if (c == NULL)
        return NULL;

    c->flags |= REDIS_BLOCK;
    redisContextConnectTcp(c,ip,port,&tv);
    return c;
}

redisContext *redisConnectNonBlock(const char *ip, int port) {
    redisContext *c;

    c = redisContextInit();
    if (c == NULL)
        return NULL;

    c->flags &= ~REDIS_BLOCK;
    redisContextConnectTcp(c,ip,port,NULL);
    return c;
}

redisContext *redisConnectBindNonBlock(const char *ip, int port,
                                       const char *source_addr) {
    redisContext *c = redisContextInit();
    c->flags &= ~REDIS_BLOCK;
    redisContextConnectBindTcp(c,ip,port,NULL,source_addr);
    return c;
}

redisContext *redisConnectBindNonBlockWithReuse(const char *ip, int port,
                                                const char *source_addr) {
    redisContext *c = redisContextInit();
    c->flags &= ~REDIS_BLOCK;
    c->flags |= REDIS_REUSEADDR;
    redisContextConnectBindTcp(c,ip,port,NULL,source_addr);
    return c;
}

redisContext *redisConnectUnix(const char *path) {
    redisContext *c;

    c = redisContextInit();
    if (c == NULL)
        return NULL;

    c->flags |= REDIS_BLOCK;
    redisContextConnectUnix(c,path,NULL);
    return c;
}

redisContext *redisConnectUnixWithTimeout(const char *path, const struct timeval tv) {
    redisContext *c;

    c = redisContextInit();
    if (c == NULL)
        return NULL;

    c->flags |= REDIS_BLOCK;
    redisContextConnectUnix(c,path,&tv);
    return c;
}

redisContext *redisConnectUnixNonBlock(const char *path) {
    redisContext *c;

    c = redisContextInit();
    if (c == NULL)
        return NULL;

    c->flags &= ~REDIS_BLOCK;
    redisContextConnectUnix(c,path,NULL);
    return c;
}

redisContext *redisConnectFd(int fd) {
    redisContext *c;

    c = redisContextInit();
    if (c == NULL)
        return NULL;

    c->fd = fd;
    c->flags |= REDIS_BLOCK | REDIS_CONNECTED;
    return c;
}

/* Set read/write timeout on a blocking socket. */
int redisSetTimeout(redisContext *c, const struct timeval tv) {
    if (c->flags & REDIS_BLOCK)
        return redisContextSetTimeout(c,tv);
    return REDIS_ERR;
}

/* Enable connection KeepAlive. */
int redisEnableKeepAlive(redisContext *c) {
    if (redisKeepAlive(c, REDIS_KEEPALIVE_INTERVAL) != REDIS_OK)
        return REDIS_ERR;
    return REDIS_OK;
}

/* Use this function to handle a read event on the descriptor. It will try
 * and read some bytes from the socket and feed them to the reply parser.
 *
 * After this function is called, you may use redisContextReadReply to
 * see if there is a reply available. */
int redisBufferRead(redisContext *c) {
    char buf[1024*16];
    int nread;

    /* Return early when the context has seen an error. */
    if (c->err)
        return REDIS_ERR;

    nread = read(c->fd,buf,sizeof(buf));
    if (nread == -1) {
        if ((errno == EAGAIN && !(c->flags & REDIS_BLOCK)) || (errno == EINTR)) {
            /* Try again later */
        } else {
            __redisSetError(c,REDIS_ERR_IO,NULL);
            return REDIS_ERR;
        }
    } else if (nread == 0) {
        __redisSetError(c,REDIS_ERR_EOF,"Server closed the connection");
        return REDIS_ERR;
    } else {
        if (redisReaderFeed(c->reader,buf,nread) != REDIS_OK) {
            __redisSetError(c,c->reader->err,c->reader->errstr);
            return REDIS_ERR;
        }
    }
    return REDIS_OK;
}

/* Write the output buffer to the socket.
 *
 * Returns REDIS_OK when the buffer is empty, or (a part of) the buffer was
 * successfully written to the socket. When the buffer is empty after the
 * write operation, "done" is set to 1 (if given).
 *
 * Returns REDIS_ERR if an error occurred trying to write and sets
 * c->errstr to hold the appropriate error string.
 */
int redisBufferWrite(redisContext *c, int *done) {
    int nwritten;

    /* Return early when the context has seen an error. */
    if (c->err)
        return REDIS_ERR;

    if (sdslen(c->obuf) > 0) {
        nwritten = write(c->fd,c->obuf,sdslen(c->obuf));
        if (nwritten == -1) {
            if ((errno == EAGAIN && !(c->flags & REDIS_BLOCK)) || (errno == EINTR)) {
                /* Try again later */
            } else {
                __redisSetError(c,REDIS_ERR_IO,NULL);
                return REDIS_ERR;
            }
        } else if (nwritten > 0) {
            if (nwritten == (signed)sdslen(c->obuf)) {
                sdsfree(c->obuf);
                c->obuf = sdsempty();
            } else {
                sdsrange(c->obuf,nwritten,-1);
            }
        }
    }
    if (done != NULL) *done = (sdslen(c->obuf) == 0);
    return REDIS_OK;
}

/* Internal helper function to try and get a reply from the reader,
 * or set an error in the context otherwise. */
int redisGetReplyFromReader(redisContext *c, void **reply) {
    if (redisReaderGetReply(c->reader,reply) == REDIS_ERR) {
        __redisSetError(c,c->reader->err,c->reader->errstr);
        return REDIS_ERR;
    }
    return REDIS_OK;
}

int redisGetReply(redisContext *c, void **reply) {
    int wdone = 0;
    void *aux = NULL;

    /* Try to read pending replies */
    if (redisGetReplyFromReader(c,&aux) == REDIS_ERR)
        return REDIS_ERR;

    /* For the blocking context, flush output buffer and read reply */
    if (aux == NULL && c->flags & REDIS_BLOCK) {
        /* Write until done */
        do {
            if (redisBufferWrite(c,&wdone) == REDIS_ERR)
                return REDIS_ERR;
        } while (!wdone);

        /* Read until there is a reply */
        do {
            if (redisBufferRead(c) == REDIS_ERR)
                return REDIS_ERR;
            if (redisGetReplyFromReader(c,&aux) == REDIS_ERR)
                return REDIS_ERR;
        } while (aux == NULL);
    }

    /* Set reply object */
    if (reply != NULL) *reply = aux;
    return REDIS_OK;
}


/* Helper function for the redisAppendCommand* family of functions.
 *
 * Write a formatted command to the output buffer. When this family
 * is used, you need to call redisGetReply yourself to retrieve
 * the reply (or replies in pub/sub).
 */
int __redisAppendCommand(redisContext *c, const char *cmd, size_t len) {
    sds newbuf;

    newbuf = sdscatlen(c->obuf,cmd,len);
    if (newbuf == NULL) {
        __redisSetError(c,REDIS_ERR_OOM,"Out of memory");
        return REDIS_ERR;
    }

    c->obuf = newbuf;
    return REDIS_OK;
}

int redisAppendFormattedCommand(redisContext *c, const char *cmd, size_t len) {

    if (__redisAppendCommand(c, cmd, len) != REDIS_OK) {
        return REDIS_ERR;
    }

    return REDIS_OK;
}

int redisvAppendCommand(redisContext *c, const char *format, va_list ap) {
    char *cmd;
    int len;

    len = redisvFormatCommand(&cmd,format,ap);
    if (len == -1) {
        __redisSetError(c,REDIS_ERR_OOM,"Out of memory");
        return REDIS_ERR;
    } else if (len == -2) {
        __redisSetError(c,REDIS_ERR_OTHER,"Invalid format string");
        return REDIS_ERR;
    }

    if (__redisAppendCommand(c,cmd,len) != REDIS_OK) {
        free(cmd);
        return REDIS_ERR;
    }

    free(cmd);
    return REDIS_OK;
}

int redisAppendCommand(redisContext *c, const char *format, ...) {
    va_list ap;
    int ret;

    va_start(ap,format);
    ret = redisvAppendCommand(c,format,ap);
    va_end(ap);
    return ret;
}

int redisAppendCommandArgv(redisContext *c, int argc, const char **argv, const size_t *argvlen) {
    sds cmd;
    int len;

    len = redisFormatSdsCommandArgv(&cmd,argc,argv,argvlen);
    if (len == -1) {
        __redisSetError(c,REDIS_ERR_OOM,"Out of memory");
        return REDIS_ERR;
    }

    if (__redisAppendCommand(c,cmd,len) != REDIS_OK) {
        sdsfree(cmd);
        return REDIS_ERR;
    }

    sdsfree(cmd);
    return REDIS_OK;
}

/* Helper function for the redisCommand* family of functions.
 *
 * Write a formatted command to the output buffer. If the given context is
 * blocking, immediately read the reply into the "reply" pointer. When the
 * context is non-blocking, the "reply" pointer will not be used and the
 * command is simply appended to the write buffer.
 *
 * Returns the reply when a reply was successfully retrieved. Returns NULL
 * otherwise. When NULL is returned in a blocking context, the error field
 * in the context will be set.
 */
static void *__redisBlockForReply(redisContext *c) {
    void *reply;

    if (c->flags & REDIS_BLOCK) {
        if (redisGetReply(c,&reply) != REDIS_OK)
            return NULL;
        return reply;
    }
    return NULL;
}

void *redisvCommand(redisContext *c, const char *format, va_list ap) {
    if (redisvAppendCommand(c,format,ap) != REDIS_OK)
        return NULL;
    return __redisBlockForReply(c);
}

void *redisCommand(redisContext *c, const char *format, ...) {
    va_list ap;
    va_start(ap,format);
    void *reply = redisvCommand(c,format,ap);
    va_end(ap);
    return reply;
}

void *redisCommandArgv(redisContext *c, int argc, const char **argv, const size_t *argvlen) {
    if (redisAppendCommandArgv(c,argc,argv,argvlen) != REDIS_OK)
        return NULL;
    return __redisBlockForReply(c);
}

static void freeXredisContext(xRedisContext *xc) {
    if (xc == NULL) {
        return;
    }
    if (xc->ip != NULL) {
        free(xc->ip);
    }
    //close(xc->task_fd[1]);
    //close(xc->task_fd[0]);
    free(xc);
}
/* multiplex redis client call interface */
void clearClient(void) {
    aeDeleteEventLoop(client.aeloop);

    pthread_mutex_lock(&client.reconn_mutex);
    if (client.reconn_list != NULL) {
        listNode *node = listFirst(client.reconn_list);
        listNode *next_node = node;
        xRedisContext* xc;
        while (node != NULL) {
            xc = listNodeValue(node); 
            xRedisAsyncFree(xc->ctx);
            xc->ctx = NULL;
            freeXredisContext(xc);
            
            next_node = node->next;
            listDelNode(client.reconn_list, node);
            node = next_node;
        }
    }
    listRelease(client.reconn_list);
    client.reconn_list = NULL;
    pthread_mutex_unlock(&client.reconn_mutex);

    pthread_mutex_lock(&client.mutex);
    if (client.free_ac_list != NULL) {
        listNode *node = listFirst(client.free_ac_list);
        listNode *next_node = node;
        redisAsyncContext* ac;
        while (node != NULL) {
            ac = listNodeValue(node); 
            xRedisAsyncFree(ac);
            next_node = node->next;
            listDelNode(client.free_ac_list, node);
            node = next_node;
        }
    }
    listRelease(client.free_ac_list);
    client.free_ac_list = NULL;
    pthread_mutex_unlock(&client.mutex);

    client.aeloop = NULL;
    return;
}

void* startAeLoop(void* arg) {
    UNUSED(arg);

    aeMain(client.aeloop);
    LOG(" Quit aeMain\n");

    pthread_mutex_lock(&client.mutex);
    clearClient();
    pthread_mutex_unlock(&client.mutex);

    return NULL;
}

void* reconnectDis(void* arg) {
    UNUSED(arg);

    LOG("Start reconnectThread \n");
    int ret = 0;
    while (!client.aeloop->stop) {
        pthread_mutex_lock(&client.reconn_mutex);
        if (listLength(client.reconn_list)) {
            listNode *node = listFirst(client.reconn_list);
            while (node != NULL) {
                xRedisContext* xc = listNodeValue(node); 
                listNode *next_node = listNextNode(node);
                if (xc->remain_wait_reconn == 0) {
                    pthread_mutex_lock(&xc->mutex);
                    ret = __xRedisReconnectConnectNonBlock(xc);
                    pthread_mutex_unlock(&xc->mutex);
                    if (ret != REDIS_OK) {  //connect error
                        LOG(" ip=%s:%d reconnect error\n", xc->ip,xc->port);
                        xc->wait_time_reconn = 2 * xc->wait_time_reconn;
                        xc->remain_wait_reconn = xc->wait_time_reconn;
                    } else {  //connect ok
                        LOG(" ip=%s:%d reconnect Success\n", xc->ip,xc->port);
                        listDelNode(client.reconn_list,node);

                        xc->wait_time_reconn = 1;
                        xc->remain_wait_reconn = 0;
                    }
                } else {
                    xc->remain_wait_reconn--;
                }
                node = next_node;
            }
        }
        pthread_mutex_unlock(&client.reconn_mutex);
        usleep(100000);
    }
    return NULL;
}


int clientCron(struct aeEventLoop *eventLoop, long long id, void *clientData) {
    UNUSED(eventLoop);
    UNUSED(id);
    UNUSED(clientData);
    return 1;
}

/* del all disconnected req */
/*
void clearDisconReq(void) {
    if ( listLength(client.disconn_list) > 0) {
        listNode *req_node = listFirst(client.req_list);
        while ( req_node != NULL) {
            reqData *req = listNodeValue(req_node);
            listNode *next_node = listNextNode(req_node);
            if (req != NULL && listSearchKey(client.disconn_list,req->ac) != NULL) {
                //del dict session
                dictDelete(client.sessions,req->session_id);
                //del req list
                listDelNode(client.req_list,req_node); 
            }
            req_node = next_node;
        }
        listEmpty(client.disconn_list);
    }
}
*/

void taskDeal(struct aeEventLoop *eventLoop,int fd, void *privdata, int mask) {
    UNUSED(eventLoop);
    UNUSED(fd);
    UNUSED(mask);

    cmdInfo *ci = NULL;
    xRedisContext *xc = privdata;

    int get_num = 0;
    char buf[1024]; 
    get_num = read(xc->task_fd[0], buf,1024);
    if (get_num < 1 ) {
        get_num = 1024;
    }

    pthread_mutex_lock(&xc->mutex);
    
    if (__sync_add_and_fetch(&xc->status, 0) != REDIS_CONNECTED) {
        pthread_mutex_unlock(&xc->mutex);
        return ;
    } 
    redisAsyncContext *ac = xc->ctx;

    while(listLength(ac->cmd_list) > 0) {
        listNode *node = listFirst(ac->cmd_list);
        if ( node != NULL) {
            ci = listNodeValue(node); 
            listDelNode(ac->cmd_list, node);
            if (ci == NULL) {
                continue;
            }
     //       pthread_mutex_unlock(&xc->mutex);
            redisAsyncCommandHelper(xc->ctx,replyCallback,ci,ci->cmd,ci->len);
            if (--get_num <= 0) {
      //          pthread_mutex_lock(&xc->mutex);
                break;
            }
       //     pthread_mutex_lock(&xc->mutex);
        } else {
           break; 
        }
    }
    pthread_mutex_unlock(&xc->mutex);
    return ;
}

void freeAsyncContextDeal(void) {
    int status;
    int get_num = 0;
    pthread_mutex_lock(&client.mutex);
    while(listLength(client.free_ac_list) > 0) {
        listNode *node = listFirst(client.free_ac_list);
        if ( node != NULL) {
            redisAsyncContext *ac = listNodeValue(node);
            listDelNode(client.free_ac_list, node);
            if (ac == NULL) {
                continue;
            }
            xRedisAsyncFree(ac);
        }
    }
    pthread_mutex_unlock(&client.mutex);
    return;
}

void beforeSleepDeal(struct aeEventLoop *eventLoop) {
    UNUSED(eventLoop);

    freeAsyncContextDeal();
    return ;
}


int __initMpClient(void) {
    pthread_mutex_lock(&client.mutex);
    if (client.aeloop == NULL) {
        client.aeloop = aeCreateEventLoop(EVENT_SIZE);
        if (client.aeloop == NULL) {
            LOG("Create aeEventLoop Error\n");
            pthread_mutex_unlock(&client.mutex);
            return -1;
        }
        aeCreateTimeEvent(client.aeloop,1,clientCron,NULL,NULL);

        if (client.free_ac_list == NULL) {
            client.free_ac_list = listCreate();
        }

        aeSetBeforeSleepProc(client.aeloop,beforeSleepDeal);

        if (client.reconn_list == NULL) {
            client.reconn_list = listCreate();
        }

                pthread_t p_id;
        if (pthread_create(&p_id, NULL,startAeLoop, NULL) != 0) {
            LOG("Create thread startAeLoop error:%d\n", errno);

            if (!client.aeloop->stop) {
                aeStop(client.aeloop);
            }

            clearClient();
            pthread_mutex_unlock(&client.mutex);
            return -2;
        }

        pthread_t recon_id;
        if (pthread_create(&recon_id, NULL,reconnectDis, NULL) != 0) {
            LOG("Create thread reconnectThread error:%d\n", errno);
            if (!client.aeloop->stop) {
                aeStop(client.aeloop);
            }

            clearClient();
            pthread_mutex_unlock(&client.mutex);
            return -3;
        }
    }
    pthread_mutex_unlock(&client.mutex);

    return 0;
}

int __initAttachContext(redisAsyncContext* ac) {
    if (ac == NULL) {
        return -1;
    }
    pthread_mutex_lock(&client.mutex);
    redisAeAttach(client.aeloop, ac);

    redisAsyncSetConnectCallback(ac,connectedCallback);
    redisAsyncSetDisconnectCallback(ac,disconnectedCallback);

    aeCreateFileEvent(client.aeloop,ac->xc->task_fd[0], AE_READABLE, taskDeal, ac->xc);

    pthread_mutex_unlock(&client.mutex);
   
    return 0;
}

int __initAttachContextNoLock(redisAsyncContext* ac) {
    if (ac == NULL) {
        return -1;
    }
    redisAeAttach(client.aeloop, ac);

    redisAsyncSetConnectCallback(ac,connectedCallback);
    redisAsyncSetDisconnectCallback(ac,disconnectedCallback);
   
    aeCreateFileEvent(client.aeloop,ac->xc->task_fd[0], AE_READABLE, taskDeal, ac->xc);
    return 0;
}
int attachAsyncNoLock(redisAsyncContext *ac,const struct timeval tv) {
    if (__initAttachContext(ac) != 0) {
        LOG(" __initAttachContext error\n");
        return -1;
    }
    struct timespec time_out;
    clock_gettime(CLOCK_REALTIME, &time_out);  
    time_out.tv_sec += tv.tv_sec;  
    time_out.tv_nsec += tv.tv_usec * 1000;  

    pthread_cond_timedwait(&ac->xc->cond,&ac->xc->mutex, &time_out);

    if (__sync_add_and_fetch(&ac->xc->status, 0) != REDIS_CONNECTED) {
        //redis connnect create error
        LOG("connect fd:%d  error(%d) :%s\n",ac->c.fd, ac->c.err,ac->c.errstr);
        return -3;
    }
    return REDIS_OK;
}
int attachAsync(redisAsyncContext *ac,const struct timeval tv) {
    pthread_mutex_lock(&ac->xc->mutex);
    if (__initAttachContext(ac) != 0) {
        pthread_mutex_unlock(&ac->xc->mutex);
        LOG(" __initAttachContext error\n");
        return -1;
    }
    struct timespec time_out;
    clock_gettime(CLOCK_REALTIME, &time_out);  
    time_out.tv_sec += tv.tv_sec;  
    time_out.tv_nsec += tv.tv_usec * 1000;  

    pthread_cond_timedwait(&ac->xc->cond,&ac->xc->mutex, &time_out);

    if (__sync_add_and_fetch(&ac->xc->status, 0) != REDIS_CONNECTED) {
        //redis connnect create error
        LOG("connect fd:%d  error(%d) :%s\n",ac->c.fd, ac->c.err,ac->c.errstr);
        pthread_mutex_unlock(&ac->xc->mutex);
        return -3;
    }
    pthread_mutex_unlock(&ac->xc->mutex);
    return REDIS_OK;
}

static xRedisContext *xRedisContextInit(const char *ip, int port) {
    xRedisContext *xc;
    xc = malloc(sizeof(xRedisContext));
    xc->ip = strdup(ip);
    xc->port = port;
    xc->wait_time_reconn = 1;
    xc->remain_wait_reconn = 0;

    /* pipe task_fd and set nonblock */
    pipe(xc->task_fd);
    int flags = fcntl(xc->task_fd[0], F_GETFL, 0);
    fcntl(xc->task_fd[0], F_SETFL, flags | O_NONBLOCK); 
    flags = fcntl(xc->task_fd[1], F_GETFL, 0);
    fcntl(xc->task_fd[1], F_SETFL, flags | O_NONBLOCK); 

    pthread_mutex_init(&xc->mutex,NULL);
    pthread_cond_init(&xc->cond,NULL);

    return xc;
}



static void __xRedisFree(xRedisContext *xc) {
    if (xc == NULL) {
       return ; 
    }

    if (xc->ctx != 0) {
        redisFree(xc->ctx);
        xc->ctx = NULL;
    }

    freeXredisContext(xc);
    return;
}

xRedisContext *__xRedisConnectNonBlockTimeOut(const char *ip, int port,const struct timeval tv) {
    if (ip == NULL || port < 0 || isValidIP(ip)== 0) {
        return NULL;
    }

    int ret = __initMpClient();
    if (ret != 0) {
        LOG(" Init Client error return:%d\n", ret);
        return NULL;
    }

    xRedisContext *xc;
    xc = xRedisContextInit(ip, port);
    if (xc == NULL) {
        LOG(" Error init redis context\n");
        return NULL;
    }

    xc->ctx = redisContextInit();
    redisContext *c = xc->ctx;
    if (c == NULL) {
        freeXredisContext(xc);
        return NULL;
    }

    c->flags &= ~REDIS_BLOCK;
    __sync_lock_test_and_set(&xc->status,0);
    redisContextConnectTcp(c,ip,port,NULL);

    redisAsyncContext* ac;
    ac = redisAsyncInitializeHelper(c);
    if (ac == NULL) {
        __xRedisFree(xc);
        return NULL;
    }   
    redisAsyncCopyErrorHelper(ac);
    ac->xc = xc;
    xc->ctx = ac;

    ret = attachAsync(ac,tv); 
    if (ret != REDIS_OK) {
        freeXredisContext(xc);
        xc = NULL;
    }
    return xc;
}

xRedisContext *xRedisConnect(const char *ip, int port) {

    struct timeval tv;
    tv.tv_sec = client.timeout;
    tv.tv_usec = 0;

    return xRedisConnectWithTimeout(ip, port, tv);
}

xRedisContext *xRedisConnectWithTimeout(const char *ip, int port, const struct timeval tv) {
    return  __xRedisConnectNonBlockTimeOut(ip, port,tv);
}

xRedisContext *xRedisConnectNonBlock(const char *ip, int port) {
    struct timeval tv;
    tv.tv_sec = client.timeout;
    tv.tv_usec = 0;

    return  __xRedisConnectNonBlockTimeOut(ip, port,tv);
}

xRedisContext *xRedisConnectBindNonBlock(const char *ip, int port,
                                       const char *source_addr) {
    if (ip == NULL || port < 0 || isValidIP(ip)== 0) {
        return NULL;
    }
    int ret = __initMpClient();
    if ( ret != 0) {
        LOG(" Init Client error return:%d\n", ret);
        return NULL;
    }

    xRedisContext *xc;
    xc = xRedisContextInit(ip, port);
    if (xc == NULL) {
        LOG(" Error init redis context\n");
        return NULL;
    }

    xc->ctx = redisContextInit();
    redisContext *c = xc->ctx;
    if (c == NULL) {
        freeXredisContext(xc);
        return NULL;
    }

    __sync_lock_test_and_set(&xc->status,0);
    c->flags &= ~REDIS_BLOCK;
    redisContextConnectBindTcp(c,ip,port,NULL,source_addr);

    redisAsyncContext* ac;
    ac = redisAsyncInitializeHelper(c);
    if (ac == NULL) {
        __xRedisFree(xc);
        return NULL;
    }   
    redisAsyncCopyErrorHelper(ac);
    ac->xc = xc;
    xc->ctx = ac;

    struct timeval tv;
    tv.tv_sec = client.timeout;
    tv.tv_usec = 0;

    ret = attachAsync(ac,tv);
    if (ret != REDIS_OK) {
        freeXredisContext(xc);
        xc = NULL;
    }

    return xc;
}

static int __xRedisReconnectConnectNonBlock(xRedisContext *xc) {

    if (xc == NULL) {
        return REDIS_ERR;
    }

    //LOG("------------ip:%s port:%d \n",xc->ip, xc->port);
    
    xc->ctx = redisContextInit();
    redisContext *c = xc->ctx;
    if (c == NULL) {
        return REDIS_ERR ;
    }

    c->flags &= ~REDIS_BLOCK;
    redisContextConnectTcp(c,xc->ip,xc->port,NULL);

    struct timeval tv;
    tv.tv_sec = client.timeout;
    tv.tv_usec = 0;

    redisAsyncContext* ac;
    ac = redisAsyncInitializeHelper(c);
    if (ac == NULL) {
        redisFree(xc->ctx);
        xc->ctx = NULL;
        return REDIS_ERR; 
    }   
    redisAsyncCopyErrorHelper(ac);
    ac->xc = xc;
    xc->ctx = ac;
 
    int ret = 0;
    ret = attachAsyncNoLock(ac,tv); 
    if (ret != REDIS_OK) {
        //xRedisAsyncFree(ac);
        xc->ctx = NULL;
        return ret;
    }
    return REDIS_OK;
}

int xRedisReconnectConnectNonBlock(xRedisContext *xc) {

    if (xc == NULL) {
        return REDIS_ERR;
    }

    //LOG("------------ip:%s port:%d \n",xc->ip, xc->port);
    
    if (xc->ctx != NULL) {
        xRedisAsyncFree(xc->ctx);
    }
   
    xc->ctx = redisContextInit();
    redisContext *c = xc->ctx;
    if (c == NULL) {
        return REDIS_ERR ;
    }

    __sync_lock_test_and_set(&xc->status,0);
    c->flags &= ~REDIS_BLOCK;
    redisContextConnectTcp(c,xc->ip,xc->port,NULL);

    struct timeval tv;
    tv.tv_sec = client.timeout;
    tv.tv_usec = 0;

    redisAsyncContext* ac;
    ac = redisAsyncInitializeHelper(c);
    if (ac == NULL) {
        redisFree(xc->ctx);
        xc->ctx = NULL;
        return REDIS_ERR;
    }   
    redisAsyncCopyErrorHelper(ac);
    ac->xc = xc;
    xc->ctx = ac;
 
    int ret = 0;
    ret = attachAsync(ac,tv); 
    if (ret != REDIS_OK) {
        xc->ctx = NULL;
        return REDIS_ERR;
    }
    return REDIS_OK;
}

xRedisContext *xRedisConnectBindNonBlockWithReuse(const char *ip, int port,
        const char *source_addr) {
    if (ip == NULL || port < 0 || isValidIP(ip)== 0) {
        return NULL;
    }
    int ret = __initMpClient();
    if ( ret != 0) {
        LOG(" Init Client error return:%d\n", ret);
        return NULL;
    }

    xRedisContext *xc;
    xc = xRedisContextInit(ip, port);
    if (xc == NULL) {
        LOG(" Error init redis context\n");
        return NULL;
    }

    xc->ctx = redisContextInit();
    redisContext *c = xc->ctx;
    if (c == NULL) {
        freeXredisContext(xc);
        return NULL;
    }

    __sync_lock_test_and_set(&xc->status,0);
    c->flags &= ~REDIS_BLOCK;
    c->flags |= REDIS_REUSEADDR;
    redisContextConnectBindTcp(c,ip,port,NULL,source_addr);

    redisAsyncContext* ac;
    ac = redisAsyncInitializeHelper(c);
    if (ac == NULL) {
        __xRedisFree(xc);
        return NULL;
    }   
    redisAsyncCopyErrorHelper(ac);
    ac->xc = xc;
    xc->ctx = ac;

    struct timeval tv;
    tv.tv_sec = client.timeout;
    tv.tv_usec = 0;

    ret = attachAsync(ac,tv);
    if (ret != REDIS_OK) {
        freeXredisContext(xc);
        xc = NULL;
    }
        
    return xc;
}

int xRedisReconnect(xRedisContext *xc) {
    return xRedisReconnectConnectNonBlock(xc);
}

void* __xRedisCommand(xRedisContext *xc, const char *cmd, size_t len) {
    if (client.aeloop->stop) {
        LOG("aeclient.aeloop Quit No connection can use\n");
        return NULL;
    }

    if (__sync_sub_and_fetch(&xc->status,0) != REDIS_CONNECTED) {
        return NULL;
    }
    

    struct cmdInfo *ci = malloc(sizeof(struct cmdInfo)); 
    ci->cmd = cmd; 
    ci->len = len;
    ci->reply = NULL;
    ci->reference = 1;
    sem_init(&ci->reply_sem, 1, 0);

    pthread_mutex_lock(&xc->mutex);
    redisAsyncContext *ac = xc->ctx;
    if (ac == NULL) {
        pthread_mutex_unlock(&xc->mutex);
        free(ci);
        return NULL;
    }
    __sync_add_and_fetch(&ci->reference, 1);
    if (__sync_sub_and_fetch(&xc->status,0) != REDIS_CONNECTED) {
        sem_destroy(&ci->reply_sem);
        free(ci);
        pthread_mutex_unlock(&xc->mutex);
        return NULL;
    }
    listAddNodeTail(ac->cmd_list,ci);
    pthread_mutex_unlock(&xc->mutex);

    write(xc->task_fd[1],client.pe,1); 

    if (ci->reply == NULL ) { 
        struct timespec wait_time_out;
        clock_gettime(CLOCK_REALTIME, &wait_time_out);  
        wait_time_out.tv_sec += client.timeout;  
        sem_timedwait(&ci->reply_sem, &wait_time_out);
    } 

    redisReply* reply = ci->reply;
    int ref = __sync_sub_and_fetch(&ci->reference, 1);

    if (ref <= 0) {
        sem_destroy(&ci->reply_sem);
        free(ci);
    } else {
        LOG("REQ timeout no response %p xc:%p\n", ac,xc);
        listNode* node = listSearchKey(ac->cmd_list,ci);
        if (node != NULL) {
            listDelNode(ac->cmd_list,node);
        }

        if (__sync_sub_and_fetch(&xc->status,0) != REDIS_CONNECTED) {
            sem_destroy(&ci->reply_sem);
            free(ci);
        }
    }
    return reply;
}

void *xRedisvCommand(xRedisContext *c, const char *format, va_list ap) {
    if (c == NULL) {
        return NULL;
    }

    char *cmd;
    int len;

    len = redisvFormatCommand(&cmd,format,ap);

    if (len == -1) {
        __redisSetError(c,REDIS_ERR_OOM,"Out of memory");
        return NULL;
    } else if (len == -2) {
        __redisSetError(c,REDIS_ERR_OTHER,"Invalid format string");
        return NULL;
    }

    void *reply = __xRedisCommand(c,cmd,len);
    free(cmd);

	return reply;
}

void *xRedisCommand(xRedisContext *c, const char *format, ...) {
    if (c == NULL) {
        return NULL;
    }
    va_list ap;
    void *reply = NULL;
    va_start(ap,format);
    reply = xRedisvCommand(c,format,ap);
    va_end(ap);
    return reply;
}

void *xRedisCommandArgv(xRedisContext *c, int argc, const char **argv, const size_t *argvlen) {
    if (c == NULL) {
        return NULL;
    }
    sds cmd;
    int len;

    len = redisFormatSdsCommandArgv(&cmd,argc,argv,argvlen);
    if (len == -1) {
        __redisSetError(c,REDIS_ERR_OOM,"Out of memory");
        return NULL;
    }

    void *reply = __xRedisCommand(c,cmd,len);
    sdsfree(cmd);

	return reply;
}


void xStopClient(void) {
    pthread_mutex_lock(&client.mutex);
    if (client.aeloop != NULL) {
        if (!client.aeloop->stop) {
            aeStop(client.aeloop);
        }
    }
    pthread_mutex_unlock(&client.mutex);
    return;
}

void xRedisFree(xRedisContext *xc) {
    int need_wait = 0;
    pthread_mutex_lock(&client.mutex);
    if ( xc->ctx != NULL) {
        __sync_add_and_fetch(&((redisAsyncContext*)xc->ctx)->status,FREED_CONTEXT); //主动释放
        listAddNodeTail(client.free_ac_list,xc->ctx);
        need_wait = 1;
    }
    pthread_mutex_unlock(&client.mutex);

    pthread_mutex_lock(&client.reconn_mutex);
    listNode *node = listSearchKey(client.reconn_list,xc); 
    if (node != NULL) {
        listDelNode(client.reconn_list,node);
    }
    pthread_mutex_unlock(&client.reconn_mutex);

    if (need_wait) {
        pthread_mutex_lock(&xc->mutex);
        pthread_cond_wait(&xc->cond,&xc->mutex);
        pthread_mutex_unlock(&xc->mutex);
    }
    freeXredisContext(xc);
    return;
}

void xSetTimeout(int timeout) {
    client.timeout = timeout;
}

