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

void* test_zset_cmd1(void *arg) {
    xRedisContext *c;
    redisReply *reply;
    con_config *cfg = arg;

    test("redisConnect");
    c = xRedisConnect(cfg->hostname, cfg->port);

    test("Test ZADD ");
    reply = xRedisCommand(c,"ZADD page_rank 10 google.com 9 baidu.com 8 bing.com");
    if (reply != NULL) {
        test_cond( reply->integer == 3)
            freeReplyObject(reply);
    } else {
        test(" error\n"); 
    }

    test("Test ZCARD ");
    reply = xRedisCommand(c," zcard page_rank");
    if (reply != NULL) {
        test_cond( reply->integer == 3)
            freeReplyObject(reply);
    } else {
       test(" error\n"); 
    }

    test("Test ZCOUNT ");
    reply = xRedisCommand(c," zcount page_rank 8 9");
    if (reply != NULL) {
        test_cond( reply->integer == 2)
            freeReplyObject(reply);
    } else {
       test(" error\n"); 
    }

    test("Test ZINCRBY ");
    reply = xRedisCommand(c," zincrby page_rank 10 baidu.com ");
    if (reply != NULL) {
        test_cond( strcasecmp(reply->str,"19") == 0)
            freeReplyObject(reply);
    } else {
       test(" error\n"); 
    }

    test("Test ZRANGE ");
    reply = xRedisCommand(c," zrange page_rank 0  -1 withscores");
    if (reply != NULL) {
        test_cond( reply->elements > 0)
            freeReplyObject(reply);
    } else {
       test(" error\n"); 
    }

    test("Test ZRANGEBYSCORE ");
    reply = xRedisCommand(c," zrangebyscore page_rank -inf +inf withscores");
    if (reply != NULL) {
        test_cond( reply->elements > 0)
            freeReplyObject(reply);
    } else {
       test(" error\n"); 
    }


    test("Test ZRANK");
    reply = xRedisCommand(c," zrank page_rank baidu.com");
    if (reply != NULL) {
        test_cond( reply->integer > 0)
            freeReplyObject(reply);
    } else {
       test(" error\n"); 
    }

    test("Test ZREM");
    reply = xRedisCommand(c," zrem page_rank baidu.com");
    if (reply != NULL) {
        test_cond( reply->integer > 0)
            freeReplyObject(reply);
    } else {
       test(" error\n"); 
    }



    xRedisFree(c);
    return 0;

}

void* test_zset_cmd2(void *arg) {
    xRedisContext *c;
    redisReply *reply;
    con_config *cfg = arg;

    c = xRedisConnect(cfg->hostname, cfg->port);

    reply = xRedisCommand(c, "zadd salary 2000 jack 5000 tom 3500 peter");
    freeReplyObject(reply);

    test(" Test ZREMRANGEBYRANK");
    reply = xRedisCommand(c,"ZREMRANGEBYRANK  salary 0 1");
    if (reply != NULL) {
        test_cond(reply->integer > 0);
        freeReplyObject(reply);
    }

    reply = xRedisCommand(c, "zadd salary 2000 jack 5000 tom 3500 peter");
    freeReplyObject(reply);

    test("Test ZREMRANGEBYSCORE");
    reply = xRedisCommand(c,"ZREMRANGEBYSCORE salary 1500 3500");
    if (reply != NULL) {
        test_cond(reply->integer == 2);
        freeReplyObject(reply);
    }

    reply = xRedisCommand(c, "zadd salary 2000 jack 5000 tom 3500 peter");
    freeReplyObject(reply);


    test("Test ZREVRANGE ");
    reply = xRedisCommand(c, "ZREVRANGE salary 0 -1 WITHSCORES");
    test_cond(reply->elements > 0);
    freeReplyObject(reply);

    test("Test ZREVRANGEBYSCORE ");
    reply = xRedisCommand(c, "ZREVRANGEBYSCORE salary +inf -inf");
    test_cond(reply->elements > 0);
    freeReplyObject(reply);

    test("Test ZREVRANK");
    reply = xRedisCommand(c, "ZREVRANK salary peter");
    test_cond(reply->integer > 0);
    freeReplyObject(reply);

    test("Test ZSCORE");
    reply = xRedisCommand(c, "ZSCORE salary peter");
    test_cond(strcasecmp(reply->str, "3500") == 0);
    freeReplyObject(reply);

    reply = xRedisCommand(c, "zadd programmer 2000 peter 3500 jack 5000 tom");
    freeReplyObject(reply);
    reply = xRedisCommand(c, "zadd manager 2000 herry 3500 mary 4000 bob");
    freeReplyObject(reply);

    test("Test ZUNIONSTORE");
    reply = xRedisCommand(c, "ZUNIONSTORE salary 2 programmer manager WEIGHTS 1 3");
    test_cond(reply->integer > 0);
    freeReplyObject(reply);
 
    reply = xRedisCommand(c, "zadd mid_test 70 %s 70 %s 80 %s", "lilei", "hanmeimei", "tom");
    freeReplyObject(reply);
    reply = xRedisCommand(c, "zadd fin_test 70 %s 70 %s 80 %s", "lilei", "hanmeimei", "tom");
    freeReplyObject(reply);

    test("Test ZINTERSTORE");
    reply = xRedisCommand(c, "ZINTERSTORE sum_point 2 mid_test fin_test");
    test_cond(reply->integer > 0);
    freeReplyObject(reply);

    test("Test ZSCAN");
    reply = xRedisCommand(c, "zscan salary 0");
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
    pthread_create(&pid[0], NULL, test_zset_cmd1, &cfg);
    pthread_create(&pid[1], NULL, test_zset_cmd2, &cfg);

    for (j =0; j < 2; ++j) {
        pthread_join(pid[j], &status);
    }
 
    xRedisFree(c);
    xStopClient();
    return 0;
}

