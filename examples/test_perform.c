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

int qps = 0;

void* show_qps(void* arg) {
    while(1) {
        printf("qps :%d\n", __sync_fetch_and_and(&qps,0));
        sleep(1);
    }
}

void print_array(redisReply *reply) {
    int j = 0;
    if (reply->type == REDIS_REPLY_ARRAY) {
        for (j = 0; j < reply->elements; j++) {
            printf("%u) %s\n", j, reply->element[j]->str);
        }
    }
    return ;
}

void* test_write(void *arg) {
    xRedisContext *c = arg;
    redisReply *reply;
    //con_config *cfg = arg;
    //c = xRedisConnect(cfg->hostname, cfg->port);
    printf("SET stable1\n");
    uint64_t num = 0;
    while(1) {
        ++num;
        reply = xRedisCommand(c, "SET %d %d", num, num);
        if (reply != NULL) {
//            printf("reply-str:%s\n", reply->str);
            freeReplyObject(reply);
        }
        __sync_fetch_and_add(&qps,1);

    }

    xRedisFree(c);
    return NULL;
}


void* test_read(void *arg) {
    xRedisContext *c = arg;
    redisReply *reply;
    //con_config *cfg = arg;
    //c = xRedisConnect(cfg->hostname, cfg->port);
    printf("GET stable1\n");
    uint64_t num = 0;
    while(1) {
        ++num;
        reply = xRedisCommand(c, "GET %d", num);
        if (reply != NULL) {
            //printf("reply-str:%s\n", reply->str);
            freeReplyObject(reply);
        }
        __sync_fetch_and_add(&qps,1);
    }

    xRedisFree(c);
    return NULL;
}

int main(int argc, char **argv) {
    unsigned int j;
    redisReply *reply;
    xRedisContext *c;
    xRedisContext *c2;
    void *status;
    con_config cfg;
    cfg.hostname = (argc > 1) ? argv[1] : "127.0.0.1";
    cfg.port = (argc > 2) ? atoi(argv[2]) : 6379;
    //cfg.hostname = "10.33.125.171";
    //cfg.port = 9013;
    //cfg.hostname = "10.33.126.16";
    //cfg.port = 8000;
    printf("hostname:%s port:%d\n", cfg.hostname, cfg.port);

    test("redisConnect");
    c = xRedisConnect(cfg.hostname, cfg.port);
    c2 = xRedisConnect(cfg.hostname, cfg.port);
    if (c == NULL) {
        printf(" connect error\n");
        return -1;
    }

    int test_type = atoi(argv[3]);
    int thread_num = atoi(argv[4]);

    /*
    if (test_type == 1) {
        reply = xRedisCommand(c,"FLUSHDB");
        if (reply != NULL) {
            test_cond(reply->type == REDIS_REPLY_STATUS &&
                    strcasecmp(reply->str,"OK") == 0)
                freeReplyObject(reply);
        } else {
            test(" error\n"); 
        }
    }
    */

    pthread_t p_qps;
    pthread_create(&p_qps, NULL, show_qps,NULL);

    pthread_t pid[100];
    int i = 0;
    for (i = 0; i < thread_num; ++i) {
        if (test_type == 1) {
            pthread_create(&pid[i], NULL, test_write, c);
        } else {
            pthread_create(&pid[i], NULL, test_read, c);
        }
    }
/*
    for (i = 0; i < thread_num; ++i) {
        if (test_type == 1) {
            pthread_create(&pid[i], NULL, test_write, c2);
        } else {
            pthread_create(&pid[i], NULL, test_read, c2);
        }
    }
    */

    for (j =0; j < thread_num; ++j) {
        pthread_join(pid[j], &status);
    }
 
    xRedisFree(c);
    xStopClient();
    return 0;
}

