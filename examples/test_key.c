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

void* test_key_cmd1(void *arg) {
    xRedisContext *c;
    redisReply *reply;
    con_config *cfg = arg;

    c = xRedisConnect(cfg->hostname, cfg->port);

    test("TEST exists key");
    reply = xRedisCommand(c, "set test1 test");
    if (reply != NULL) {
        freeReplyObject(reply);
    }
    reply = xRedisCommand(c, "exists test1");
    if (reply != NULL) {
        test_cond(reply->integer == 1);
        freeReplyObject(reply);
    } else {
        printf(" error\n");
    }

    test("TEST DEL key ");
    reply = xRedisCommand(c, "del test1");
    if (reply != NULL) {
        test_cond(reply->integer == 1);     
        freeReplyObject(reply);   
    } else {
        printf(" error\n");
    }

    test("TEST dump ");
    reply = xRedisCommand(c, "SET greeting %s", "hello, dumping world!");
    if (reply != NULL) {
        freeReplyObject(reply);
    }
    reply = xRedisCommand(c, "dump greeting");
    if (reply != NULL ) { 
        test_cond(reply->len > 0);
        freeReplyObject(reply);
    }
   /* 
    test("TEST restore");
    char *dump_str = "\\x00\\x15hello, dumping world!\\a\\x00,\\x7f\\xe7\\xf1%\\xed(W";
    printf("dump_string:%s\n", dump_str);
    reply = xRedisCommand(c, "restore greeting-again 0 %s", dump_str);
    freeReplyObject(reply);
    reply = xRedisCommand(c, "get greeting-again");
    test_cond(strcasecmp(reply->str,"hello, dumping world!") == 0); 
    freeReplyObject(reply); 
*/


    test("TEST expire");
    reply = xRedisCommand(c,"expire greeting 30");
    if (reply != NULL ) {   
        freeReplyObject(reply);
    }
    reply = xRedisCommand(c, "ttl greeting");
    if (reply != NULL) {
        test_cond(reply->integer > 0);
        freeReplyObject(reply);
    }

    test(" Test expireat");

    reply = xRedisCommand(c, "SET greeting-again %s", "hello, dumping world!");
    if (reply != NULL ) {  
        freeReplyObject(reply);
    }

    reply = xRedisCommand(c,"expireat greeting-again 1513952040");
    if (reply != NULL ) {  
        freeReplyObject(reply);
    }
    reply = xRedisCommand(c, "ttl greeting-again");
    if (reply != NULL) {
        test_cond(reply->integer > 0);
        freeReplyObject(reply);
    }

    test(" Test keys");
    reply = xRedisCommand(c,"keys *");
    if (reply != NULL) {
        test_cond(reply->elements > 0);
        freeReplyObject(reply);
    }
    test(" Test persist ");
    reply = xRedisCommand(c,"persist greeting-again");
    if (reply != NULL ) {  
        freeReplyObject(reply);
    }
    reply = xRedisCommand(c, "ttl greeting-again");
    if (reply != NULL) {
        test_cond(reply->integer == -1);
        freeReplyObject(reply);
    }
    test(" Test pexpire");
    reply = xRedisCommand(c,"SET mykey Hello");
    if (reply != NULL ) {  
        freeReplyObject(reply);
    }
    reply = xRedisCommand(c,"pexpire mykey 3000");
    if (reply != NULL) {
        test_cond(reply->integer > 0);
        freeReplyObject(reply);
    }

    xRedisFree(c);
    return NULL;
}

void* test_key_cmd2(void *arg) {
    xRedisContext *c;
    redisReply *reply;
    con_config *cfg = arg;

    c = xRedisConnect(cfg->hostname, cfg->port);

    test(" Test pexpireat");
    reply = xRedisCommand(c,"SET mykey2 Hello");
    if (reply != NULL) {
        freeReplyObject(reply);
    }
    reply = xRedisCommand(c,"pexpireat mykey2 1513952040000");
    if (reply != NULL) {
        test_cond(reply->integer > 0);
        freeReplyObject(reply);
    }

    test(" Test pttl ");
    reply = xRedisCommand(c,"pttl mykey2");
    if (reply != NULL) {
        test_cond(reply->integer > 0);
        freeReplyObject(reply);
    }

    test(" Test RANDOMKEY");
    reply = xRedisCommand(c,"RANDOMKEY");
    if (reply != NULL) {
        test_cond(strlen(reply->str)> 0);
        freeReplyObject(reply);
    }
    test(" Test renamenx");
    reply = xRedisCommand(c,"mset test11 t1 test12 t2");
    if (reply != NULL) {
        freeReplyObject(reply);
    }
    reply = xRedisCommand(c, "renamenx test11 test12");
    if (reply != NULL) {
        test_cond(reply->integer == 0);
        freeReplyObject(reply);
    }
    test(" Test rename");
    reply = xRedisCommand(c, "rename test11 test12");
    if (reply != NULL) {
        test_cond(strcasecmp(reply->str,"ok") == 0);
        freeReplyObject(reply);
    }

    test(" Test sort");
    reply = xRedisCommand(c,"LPUSH today_cost 30 1.5 10 8");
    if (reply != NULL) {
        freeReplyObject(reply);
    }
    reply = xRedisCommand(c,"sort today_cost");
    if (reply != NULL) {
        print_array(reply);
        test_cond(reply->elements > 0);
        freeReplyObject(reply);
    }
    test(" Test type");
    reply = xRedisCommand(c,"type today_cost");
    if (reply != NULL) {
        test_cond(strcasecmp(reply->str,"list") == 0);
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
    xSetTimeout(10);
    c = xRedisConnect(cfg.hostname, cfg.port);
    if( c == NULL) {
        sleep(1);
        return 1;
    }

    reply = xRedisCommand(c,"FLUSHDB");
    if (reply != NULL) {
        test_cond(reply->type == REDIS_REPLY_STATUS &&
                strcasecmp(reply->str,"OK") == 0)
            freeReplyObject(reply);
    } else {
        test(" error\n"); 
    }

    pthread_t pid[100];
    pthread_create(&pid[0], NULL, test_key_cmd1, &cfg);
    pthread_create(&pid[1], NULL, test_key_cmd2, &cfg);

    for (j =0; j < 2; ++j) {
        pthread_join(pid[j], &status);
    }
 
    xRedisFree(c);
    xStopClient();
    sleep(2);
    return 0;
}

