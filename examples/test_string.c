#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include "hiredis.h"

/* The following lines make up our testing "framework" :) */
static int tests = 0, fails = 0;
#define test(_s) { printf("#%02d ", ++tests); printf(_s); }
#define test_cond(_c) if(_c) printf("\033[0;32mPASSED\033[0;0m\n"); else {printf("\033[0;31mFAILED\033[0;0m\n"); fails++;}

typedef struct con_config {
    char *hostname;
    int port;
} con_config;

void print_array(redisReply *reply) {
    int j = 0;
    if (reply->type == REDIS_REPLY_ARRAY) {
        for (j = 0; j < reply->elements; j++) {
            printf("%u) %s\n", j, reply->element[j]->str);
        }
    }
    return ;
}

int test_string_cmd(con_config cfg) {
    xRedisContext *c;
    redisReply *reply;

    test("redisConnect");
    c = xRedisConnect(cfg.hostname, cfg.port);

    reply = xRedisCommand(c,"FLUSHDB");
    if (reply != NULL) {
        test_cond(reply->type == REDIS_REPLY_STATUS &&
                strcasecmp(reply->str,"OK") == 0)
            freeReplyObject(reply);
    } else {
        printf("%d FLUSHDB error\n", __LINE__); 
    }
    
    test("String SET command");
    reply = xRedisCommand(c,"SET str1 test");
    if (reply != NULL) {
        test_cond(reply->type == REDIS_REPLY_STATUS &&
                strcasecmp(reply->str,"OK") == 0)
            freeReplyObject(reply);
    }
    
    test("String GET command");
    reply = xRedisCommand(c,"GET str1");
    if (reply != NULL) {
        test_cond( strcasecmp(reply->str,"test") == 0)
            freeReplyObject(reply);
    }

    test("String append command\n");
    reply = xRedisCommand(c,"append str1 test2");
    if (reply != NULL) {
            freeReplyObject(reply);
    }   

    test("String INCR command ");
    reply = xRedisCommand(c,"INCR incr1");
    if (reply != NULL) {
        test_cond( reply->integer == 1)
            freeReplyObject(reply);
    }

    test("String INCRBY command ");
    reply = xRedisCommand(c,"INCRBY incr1 2");
    if (reply != NULL) {
        test_cond( reply->integer == 3)
            freeReplyObject(reply);
    }

    test("String DECR command ");
    reply = xRedisCommand(c,"DECR incr1");
    if (reply != NULL) {
        test_cond( reply->integer == 2)
            freeReplyObject(reply);
    }

    test("String DECRBY command ");
    reply = xRedisCommand(c,"DECRBY incr1 2");
    if (reply != NULL) {
        test_cond( reply->integer == 0)
            freeReplyObject(reply);
    }

    test("test String setbit command ");
    reply = xRedisCommand(c,"SETBIT bit 10086 1");
    if (reply != NULL) {
            printf("reply->integer :%d\n", reply->integer);
            test_cond(reply->integer == 0)
            freeReplyObject(reply);
    }

    test("test String getbit command ");
    reply = xRedisCommand(c,"getbit bit 10086");
    if (reply != NULL) {
        test_cond(reply->integer == 1)
            freeReplyObject(reply);
    }

    test("String setex command ");
    reply = xRedisCommand(c," setex cache_user_id 60 10086");
    if (reply != NULL) {
        test_cond(strcasecmp(reply->str, "OK") == 0)
            freeReplyObject(reply);
    }

    test("String TTL command ");
    reply = xRedisCommand(c,"TTL cache_user_id");
    if (reply != NULL) {
        test_cond(reply->integer > 0)
            freeReplyObject(reply);
    }

    test("String setnx command ");
    reply = xRedisCommand(c," setnx cache_user_id");
    if (reply != NULL) {
        test_cond(reply->integer == 0)
            freeReplyObject(reply);
    }

    test("String setrange command ");
    reply = xRedisCommand(c," SET greeting %s",  "hello world");
    freeReplyObject(reply);
    reply = xRedisCommand(c, "SETRANGE greeting 6 %s","Redis");
    freeReplyObject(reply);
    reply = xRedisCommand(c, "get greeting"); 
    if (reply != NULL) {
        test_cond(strcasecmp(reply->str, "hello Redis") == 0)
            freeReplyObject(reply);
    }

    test("String getrange command ");
    reply = xRedisCommand(c,"getrange greeting 0 4");
    if (reply != NULL) {
        test_cond(strcasecmp(reply->str,"hello") == 0)
            freeReplyObject(reply);
    }

    test("String STRLEN command ");
    reply = xRedisCommand(c,"strlen cache_user_id");
    if (reply != NULL) {
        test_cond(reply->integer == 5)
            freeReplyObject(reply);
    }

    test("String STRLEN command ");
    reply = xRedisCommand(c,"strlen cache_user_id");
    if (reply != NULL) {
        test_cond(reply->integer == 5)
            freeReplyObject(reply);
    }

    test("Test mset");
    reply = xRedisCommand(c, "MSET date %s time %s weather %s","2012.3.30","11:00 a.m." ,"sunny");
    if(reply != NULL) {
        test_cond(strcasecmp(reply->str, "OK") == 0);
        freeReplyObject(reply);
    }

    test("Test msetnx");
    reply = xRedisCommand(c, "MSETNX date %s  nosql MongoDB key-value-store redis", "322323");
    if(reply != NULL) {
        print_array(reply);
        test_cond(reply->integer = 0);
        freeReplyObject(reply);
    }

    test("Test bitcount");
    reply = xRedisCommand(c, "SETBIT bitss 0 1");
    freeReplyObject(reply);
    reply = xRedisCommand(c, "bitcount bitss");
    if(reply != NULL) {
        print_array(reply);
        test_cond(reply->integer = 1);
        freeReplyObject(reply);
    }

    test("Test bitop");
    reply = xRedisCommand(c, "bitop and and_re bitss");
    if(reply != NULL) {
        test_cond(reply->integer = 1);
        freeReplyObject(reply);
    }

    test("Test PSETEX");
    reply = xRedisCommand(c, "PSETEX psetex 100000  test");
    if(reply != NULL) {
        printf("###################reply-str:%s\n", reply->str);
        test_cond(strcasecmp(reply->str, "OK"));
        freeReplyObject(reply);
    }

    test("Test INCRBYFLOAT");
    reply = xRedisCommand(c, "set mykey 3");
    if (reply != NULL) {
        freeReplyObject(reply);
    }
    reply = xRedisCommand(c, "incrbyfloat mykey 1.1");
    if(reply != NULL) {
        test_cond(strcasecmp(reply->str, "4.1"));
        freeReplyObject(reply);
    }


    

    xRedisFree(c);
    return 0;
}

void* test_string_write(void *arg)
{   
    xRedisContext *c = arg;
    redisReply *reply;
    //con_config *cfg = arg;

    test("redisConnect");
    //c = xRedisConnect(cfg->hostname, cfg->port);

    test("String SET command");
    reply = xRedisCommand(c,"SET str1 test");
    if (reply != NULL) {
        test_cond(reply->type == REDIS_REPLY_STATUS &&
                strcasecmp(reply->str,"OK") == 0)
            freeReplyObject(reply);
    }
    
    test("String append command\n");
    reply = xRedisCommand(c,"append str2 test2");
    if (reply != NULL) {
            freeReplyObject(reply);
    }   

    int i = 0;
    for (i = 0; i < 10000; ++i) {
        test("1111111111111111111String INCR command ");
        reply = xRedisCommand(c,"INCR incr1");
        if (reply != NULL) {
            test_cond( reply->integer > 0)
                freeReplyObject(reply);
        }
        usleep(100);
    }

    test("String INCRBY command ");
    reply = xRedisCommand(c,"INCRBY incr2 2");
    if (reply != NULL) {
        test_cond( reply->integer == 2)
            freeReplyObject(reply);
    }

    test("String DECR command ");
    reply = xRedisCommand(c,"DECR decr1");
    if (reply != NULL) {
        test_cond( reply->integer == -1)
            freeReplyObject(reply);
    }

    test("String DECRBY command ");
    reply = xRedisCommand(c,"DECRBY decr2 2");
    if (reply != NULL) {
        test_cond( reply->integer == -2)
            freeReplyObject(reply);
    }

    test("test String setbit command ");
    reply = xRedisCommand(c,"SETBIT bit 10086 1");
    if (reply != NULL) {
            printf("reply->integer :%d\n", reply->integer);
            test_cond(reply->integer == 0)
            freeReplyObject(reply);
    }

    test("String setex command ");
    reply = xRedisCommand(c," setex cache_user_id 60 10086");
    if (reply != NULL) {
        test_cond(strcasecmp(reply->str, "OK") == 0)
            freeReplyObject(reply);
    }

    test("String setnx command ");
    reply = xRedisCommand(c," setnx cache_user_id");
    if (reply != NULL) {
        test_cond(reply->integer == 0)
            freeReplyObject(reply);
    }

    test("String setrange command ");
    reply = xRedisCommand(c," SET greeting %s",  "hello world");
    freeReplyObject(reply);
    reply = xRedisCommand(c, "SETRANGE greeting 6 %s","Redis");
    freeReplyObject(reply);

    test("String STRLEN command ");
    reply = xRedisCommand(c,"strlen cache_user_id");
    if (reply != NULL) {
        test_cond(reply->integer > 0)
            freeReplyObject(reply);
    }

    test("Test mset");
    reply = xRedisCommand(c, "MSET date %s time %s weather %s","2012.3.30","11:00 a.m." ,"sunny");
    if(reply != NULL) {
        test_cond(strcasecmp(reply->str, "OK") == 0);
        freeReplyObject(reply);
    }

    test("Test msetnx");
    reply = xRedisCommand(c, "MSETNX date %s  nosql MongoDB key-value-store redis", "322323");
    if(reply != NULL) {
        print_array(reply);
        printf("reply->integer:%d\n", reply->integer);
        test_cond(reply->integer == 0);
        freeReplyObject(reply);
    }

    test("Test bitcount");
    reply = xRedisCommand(c, "SETBIT bitss 0 1");
    freeReplyObject(reply);
    

    test("Test bitop");
    reply = xRedisCommand(c, "bitop and and_re bitss");
    if(reply != NULL) {
        test_cond(reply->integer = 1);
        freeReplyObject(reply);
    }

//    xRedisFree(c);
    return NULL;
}

void* test_string_read(void *arg)
{
    xRedisContext *c = arg;
    redisReply *reply;
    //con_config *cfg = arg;

    /*
    test("redisConnect");
    c = xRedisConnect(cfg->hostname, cfg->port);
    */

    sleep(1);

    test("String GET command");
    reply = xRedisCommand(c,"GET str1");
    if (reply != NULL) {
        test_cond( strcasecmp(reply->str,"test") == 0)
            freeReplyObject(reply);
    }

    test("String GET command");
    reply = xRedisCommand(c,"GET str2");
    if (reply != NULL) {
        test_cond( strcasecmp(reply->str,"test2") == 0)
            freeReplyObject(reply);
    }


    int i = 0;
    for (i = 0; i < 10000; ++i) {
        test("222222222222222222String INCR command");
        reply = xRedisCommand(c,"GET incr1");
        if (reply != NULL) {
            test_cond( strlen(reply->str) > 0)
                freeReplyObject(reply);
        }
        usleep(100);
    }

    reply = xRedisCommand(c,"GET incr2");
    if (reply != NULL) {
        test_cond( strcasecmp(reply->str,"2") == 0)
            freeReplyObject(reply);
    }

    test("String DECR command ");
    reply = xRedisCommand(c,"GET decr1");
    if (reply != NULL) {
        test_cond( strcasecmp(reply->str,"-1") == 0)
            freeReplyObject(reply);
    }

    test("String DECRBY command ");
    reply = xRedisCommand(c,"GET decr2");
    if (reply != NULL) {
        test_cond( strcasecmp(reply->str,"-2") == 0)
            freeReplyObject(reply);
    }

    test("test String getbit command ");
    reply = xRedisCommand(c,"getbit bit 10086");
    if (reply != NULL) {
        test_cond(reply->integer == 1)
            freeReplyObject(reply);
    }

    test("String TTL command ");
    reply = xRedisCommand(c,"TTL cache_user_id");
    if (reply != NULL) {
        test_cond(reply->integer > 0)
            freeReplyObject(reply);
    }

    test("String STRLEN command ");
    reply = xRedisCommand(c,"strlen cache_user_id");
    if (reply != NULL) {
        test_cond(reply->integer == 5)
            freeReplyObject(reply);
    }

    test("String setrange command ");
    reply = xRedisCommand(c, "get greeting"); 
    if (reply != NULL) {
        test_cond(strcasecmp(reply->str, "hello Redis") == 0)
            freeReplyObject(reply);
    }

    test("String getrange command ");
    reply = xRedisCommand(c,"getrange greeting 0 4");
    if (reply != NULL) {
        test_cond(strcasecmp(reply->str,"hello") == 0)
            freeReplyObject(reply);
    }

    test("Test mget");
    reply = xRedisCommand(c, "MGET date time weather");
    if (reply != NULL) {
        test_cond(reply->elements == 3)
            freeReplyObject(reply);
    }

    reply = xRedisCommand(c, "bitcount bitss");
    if(reply != NULL) {
        print_array(reply);
        test_cond(reply->integer == 1);
        freeReplyObject(reply);
    }

    test("Test PSETEX");
    reply = xRedisCommand(c, "PSETEX psetex 100000  test");
    if(reply != NULL) {
        printf("-----------------reply-str:%s\n", reply->str);
        test_cond(strcasecmp(reply->str, "ok") == 0);
        freeReplyObject(reply);
    }

    test("Test PTTL");
    reply = xRedisCommand(c, "PTTL psetex");
    if(reply != NULL) {
        printf(" reply->integer:%d\n", reply->integer);
        test_cond(reply->integer > 0);
        freeReplyObject(reply);
    }

//    xRedisFree(c);
    return NULL;
}

int main(int argc, char **argv) {
    unsigned int j;
    xRedisContext *c;
    redisReply *reply;
    void *status;
    con_config cfg;
    cfg.hostname = (argc > 1) ? argv[1] : "127.0.0.1";
    cfg.port = (argc > 2) ? atoi(argv[2]) : 6379;

    test("redisConnect");
    c = xRedisConnect(cfg.hostname, cfg.port);

    reply = xRedisCommand(c,"FLUSHDB");
    if (reply != NULL) {
        test_cond(reply->type == REDIS_REPLY_STATUS &&
                strcasecmp(reply->str,"OK") == 0)
            freeReplyObject(reply);
    } else {
        printf("%d FLUSHDB error\n", __LINE__); 
    }
    
    //test_string_cmd(cfg);


    pthread_t p_id[100];
    pthread_create(&p_id[0],NULL, test_string_write, c);
    pthread_create(&p_id[1],NULL, test_string_read, c);
    int i = 0;
    for (i = 0; i < 2; ++i) {
        pthread_join(p_id[i], &status);
    }
    //test_string_write(cfg);
    //test_string_read(cfg);

    xRedisFree(c);
    xStopClient();
    return 0;
}

