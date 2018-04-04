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

void* test_hash_write(void* arg) {
    con_config *cfg = arg;
    xRedisContext *c;
    redisReply *reply;

    test("redisConnect");
    c = xRedisConnect(cfg->hostname, cfg->port);

    test("Test HSET ");
    reply = xRedisCommand(c,"HSET website google %s", "www.g.cn");
    if (reply != NULL) {
        test_cond( reply->integer == 1)
            freeReplyObject(reply);
    } else {
        printf("%d FLUSHDB error\n", __LINE__); 
    }

    test("Test HSETNX ");
    reply = xRedisCommand(c,"HSETNX website google %s", "www.google.cn");
    if (reply != NULL) {
        test_cond( reply->integer == 0)
            freeReplyObject(reply);
    } else {
        printf("%d error\n", __LINE__); 
    }

    test("Test HVALS ");
    reply = xRedisCommand(c,"HVALS website");
    if (reply != NULL) {
        print_array(reply);
        test_cond(reply->elements == 1);
        freeReplyObject(reply);
    } else {
        printf(" error\n"); 
    }

    test("Test HGET ");
    reply = xRedisCommand(c,"HGET website google" );
    if (reply != NULL) {
        test_cond( strcasecmp(reply->str,"www.g.cn") == 0)
            freeReplyObject(reply);
    } 

    int j = 0;
    test("Test HGETALL\n");
    reply = xRedisCommand(c,"HGETALL website" );
    if (reply != NULL) {
        if (reply->type == REDIS_REPLY_ARRAY) {
            for (j = 0; j < reply->elements; j++) {
                printf("%u) %s\n", j, reply->element[j]->str);
            }
        }       
        test_cond( reply->elements == 2);
        freeReplyObject(reply);
    }

    test("Test HINCRBY \n");
    reply = xRedisCommand(c,"HINCRBY website google 1" );
    if (reply != NULL) {
        printf("  hincrby reply  str:%s integer:%lld\n", reply->str, reply->integer);
        test_cond(reply->integer  == 0);
        freeReplyObject(reply);
    }

    test("Test HINCRBYFLOAT \n");
    reply = xRedisCommand(c,"HSET mykey field 10.50");
    freeReplyObject(reply);
    reply = xRedisCommand(c,"HINCRBYFLOAT mykey field  0.1" );
    if (reply != NULL) {
        printf("  hincrby reply  str:%s integer:%d\n", reply->str, reply->integer);
        test_cond(strcasecmp(reply->str, "10.6") == 0);
        freeReplyObject(reply);
    }

    test("Test HDEL\n");
    reply = xRedisCommand(c,"HDEL mykey field");
    if (reply != NULL) {
        test_cond(reply->integer = 1);
        freeReplyObject(reply);
    }

    xRedisFree(c);
    return NULL;
}

void* test_hash_read(void *arg) {
    con_config *cfg = arg;
    xRedisContext *c;
    redisReply *reply;

    test("redisConnect");
    c = xRedisConnect(cfg->hostname, cfg->port);


    test("Test HMSET ");
    reply = xRedisCommand(c,"HMSET website1 google www.google.com yahoo www.yahoo.com" );
    if (reply != NULL) {
        test_cond( strcasecmp(reply->str,"OK") == 0)
            freeReplyObject(reply);
    }

    int j = 0;
    test("Test HMGET \n");
    reply = xRedisCommand(c,"HMGET website1 google yahoo" );
    if (reply != NULL) {
        if (reply->type == REDIS_REPLY_ARRAY) {
            for (j = 0; j < reply->elements; j++) {
                printf("%u) %s\n", j, reply->element[j]->str);
            }
        }       
        test_cond( reply->elements == 2);
        freeReplyObject(reply);
    }

    test("Test HGETALL ");
    reply = xRedisCommand(c,"HGETALL website1" );
    if (reply != NULL) {
        print_array(reply);
        test_cond( reply->elements == 4);
        freeReplyObject(reply);
    }

    test("Test HKEYS ");
    reply = xRedisCommand(c,"HKEYS website1" );
    if (reply != NULL) {
        print_array(reply);
        test_cond( reply->elements == 2);
        freeReplyObject(reply);
    }

    test("Test HVALS ");
    reply = xRedisCommand(c,"HVALS website1" );
    if (reply != NULL) {
        print_array(reply);
        test_cond( reply->elements == 2);
        freeReplyObject(reply);
    }

    test("Test HLEN ");
    reply = xRedisCommand(c,"HLEN website1" );
    if (reply != NULL) {
        print_array(reply);
        test_cond( reply->integer == 2);
        freeReplyObject(reply);
    }

    test("Test HSCAN");
    reply = xRedisCommand(c,"hscan website1 0 " );
    if (reply != NULL) {
        print_array(reply);
        test_cond( reply->elements == 2);
        freeReplyObject(reply);
    }


    xRedisFree(c);
    return NULL;
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
        printf(" error"); 
    }

    pthread_t pid[100];
    pthread_create(&pid[0], NULL, test_hash_write, &cfg);
    pthread_create(&pid[1], NULL, test_hash_read, &cfg);

    for (j =0; j < 2; ++j) {
        pthread_join(pid[j], &status);
    }
    
    xRedisFree(c);

    xStopClient();
    return 0;
}

