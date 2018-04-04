#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
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

void* test_set_cmd1(void *arg) {
    xRedisContext *c;
    redisReply *reply;
    con_config *cfg = arg;

    test("redisConnect");
    c = xRedisConnect(cfg->hostname, cfg->port);

    test("Test SADD ");
    reply = xRedisCommand(c,"SADD bbs %s", "discuz.net");
    if (reply != NULL) {
        test_cond( reply->integer == 1)
            freeReplyObject(reply);
    } else {
        test(" error\n"); 
    }

    test("Test SCARD ");
    reply = xRedisCommand(c," scard bbs");
    if (reply != NULL) {
        test_cond( reply->integer == 1)
            freeReplyObject(reply);
    } else {
       test(" error\n"); 
    }

    test("Test sdiff");
    reply = xRedisCommand(c,"sadd peter  bet_man start_war 2012");
    freeReplyObject(reply);
    reply = xRedisCommand(c,"sadd joe fast_five 2012");
    freeReplyObject(reply);

    reply = xRedisCommand(c,"sdiff peter joe ");
    if (reply != NULL) {
        print_array(reply);
        test_cond(reply->elements > 0)
            freeReplyObject(reply);
    } else {
       test(" error\n"); 
    }

    test("Test sdiffstore");
    reply = xRedisCommand(c,"sdiffstore pj peter joe");
    if (reply != NULL) {
        test_cond( reply->integer == 2)
            freeReplyObject(reply);
    } else {
        printf(" error\n"); 
    }

    test("Test SINTER ");
    reply = xRedisCommand(c,"sinter peter joe");
    if (reply != NULL) {
        print_array(reply);
        printf(" PASSED\n");
            freeReplyObject(reply);
    } else {
        printf(" error\n"); 
    }

    test("Test SINTERSTORE ");
    reply = xRedisCommand(c,"sinterstore pjs peter joe");
    if (reply != NULL) {
        test_cond( reply->integer == 1);
        freeReplyObject(reply);
    } else {
        printf(" error\n"); 
    }

    test("Test SISMEMBER");
    reply = xRedisCommand(c,"sismember peter bet_man");
    if (reply != NULL) {
        test_cond( reply->integer == 1);
        freeReplyObject(reply);
    } else {
        printf(" error\n"); 
    }

    test("Test SMEMBERS ");
    reply = xRedisCommand(c,"smembers peter");
    if (reply != NULL) {
        print_array(reply);
        test_cond( reply->elements > 0)
            freeReplyObject(reply);
    } else {
        printf(" error\n"); 
    }

    test("Test SMOVE ");
    reply = xRedisCommand(c,"SMOVE peter joe bet_man");
    if (reply != NULL) {
        test_cond( reply->integer == 1);
        freeReplyObject(reply);
    } else {
        printf(" error\n"); 
    }

    test("Test SPOP ");
    reply = xRedisCommand(c, "spop peter");
    freeReplyObject(reply);
    reply = xRedisCommand(c,"smembers peter");
    if (reply != NULL) {
        test_cond( reply->elements > 0)
        printf(" reply->str :%s\n", reply->str);
            freeReplyObject(reply);
    } else {
        printf(" error\n"); 
    }

    xRedisFree(c);
    return 0;

}

void* test_set_cmd2(void *arg) {
    xRedisContext *c;
    redisReply *reply;
    con_config *cfg = arg;

    c = xRedisConnect(cfg->hostname, cfg->port);

    test(" Test SRANDMEMBER");
    int i = 0;
    for (i = 0;i < 100; ++i) {
        reply = xRedisCommand(c,"SADD test1 %d", i);
        if (reply != NULL) {
            freeReplyObject(reply);
        }
    }

    test("Test SRANDMEMBER");
    reply = xRedisCommand(c,"SRANDMEMBER test1");
    if (reply != NULL) {
        test_cond(strlen(reply->str) > 0);
        freeReplyObject(reply);
    }

    test("Test SREM");
    reply = xRedisCommand(c,"SREM test1 1 2 3");
    if (reply != NULL) {
        test_cond(reply->integer == 3);
        freeReplyObject(reply);
    }

    test("Test SUNION");
    reply = xRedisCommand(c, "sadd songs %s", "wangfei");
    freeReplyObject(reply);
    reply = xRedisCommand(c, "sadd mysongs %s", "jakson");
    freeReplyObject(reply);
    reply = xRedisCommand(c, "sunion songs mysongs");
    print_array(reply);
    test_cond(reply->elements == 2);
    freeReplyObject(reply);

    test("Test SUNIONSTORE");
    reply = xRedisCommand(c, "sunionstore sm songs mysongs");
    test_cond(reply->integer == 2);
    freeReplyObject(reply);
    reply = xRedisCommand(c, "smembers sm");
    print_array(reply);

    test("Test SSCAN");
    reply = xRedisCommand(c, "sscan test1 0");
    if(reply->type == REDIS_REPLY_ARRAY)
    {
        if(reply->elements == 0)
        {
            test_cond(0);
        }
        else
        {
            redisReply ** siteCounters=reply->element[1]->element;   
            int i = 0;
            for(; i<reply->element[1]->elements; i++)
            {       
                printf("%s ",siteCounters[i]->str);
            }
            test_cond(1);
        }
    }

    xRedisFree(c);
    return 0;
}

int main(int argc, char **argv) {
    unsigned int j;
    redisReply *reply;
    xRedisContext *c;
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
        test(" error\n"); 
    }


    pthread_t pid[100];
    pthread_create(&pid[0], NULL, test_set_cmd1, &cfg);
    pthread_create(&pid[1], NULL, test_set_cmd2, &cfg);

    for (j =0; j < 2; ++j) {
        pthread_join(pid[j], &status);
    }
 
    xRedisFree(c);
    xStopClient();
    return 0;
}

