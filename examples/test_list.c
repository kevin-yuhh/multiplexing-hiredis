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

pthread_mutex_t gmutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t gcond = PTHREAD_COND_INITIALIZER;

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


void* test_list_cmd(void* arg) {
    con_config* cfg = (con_config*)arg;
    xRedisContext *c;
    redisReply *reply;

    c = xRedisConnect(cfg->hostname, cfg->port);

    test("Test LPUSH ");
    reply = xRedisCommand(c,"LPUSH languages %s %s %s", "python", "java", "C++");
    if (reply != NULL) {
        test_cond( reply->integer > 0)
            freeReplyObject(reply);
    } else {
        test(" error\n"); 
    }

    test("Test RPUSH ");
    reply = xRedisCommand(c," RPUSH languages %s %s", "js", "go");
    if (reply != NULL) {
        test_cond( reply->integer > 0)
            freeReplyObject(reply);
    } else {
       test(" error\n"); 
    }

    
    test("Test LLEN");
    reply = xRedisCommand(c," LLEN languages");
    if (reply != NULL) {
        test_cond( reply->integer > 0)
            freeReplyObject(reply);
    } else {
       test(" error\n"); 
    }

    test("Test LPOP ");
    reply = xRedisCommand(c,"LPOP languages");
    if (reply != NULL) {
        printf(" reply->str:%s\n", reply->str);
            freeReplyObject(reply);
    } else {
        printf(" error\n"); 
    }

    test("Test RPOP ");
    reply = xRedisCommand(c,"RPOP languages");
    if (reply != NULL) {
        printf(" reply->str:%s\n", reply->str);
            freeReplyObject(reply);
    } else {
        printf(" error\n"); 
    }

    test("Test LPUSHX ");
    reply = xRedisCommand(c,"lpushx lang");
    if (reply != NULL) {
        test_cond( reply->integer == 0)
            freeReplyObject(reply);
    } else {
        printf(" error\n"); 
    }

    test("Test LRANGE ");
    reply = xRedisCommand(c, "lpush testrange 1");
    freeReplyObject(reply);
    reply = xRedisCommand(c,"LRANGE testrange 0 -1");
    if (reply != NULL) {
        test_cond( reply->elements > 0)
        printf(" reply->str :%s\n", reply->str);
            freeReplyObject(reply);
    } else {
        printf(" error\n"); 
    }

    test("Test LREM ");
    reply = xRedisCommand(c, "lpush languages4 python python c++ c");
    freeReplyObject(reply);
    reply = xRedisCommand(c,"LREM languages 1 python");
    if (reply != NULL) {
        test_cond( reply->integer > 0)
        printf(" reply->integer :%d\n", reply->integer);
            freeReplyObject(reply);
    } else {
        printf(" error\n"); 
    }

    test("Test LSET ");
    reply = xRedisCommand(c,"LPUSH languages2  python go go go go go ");
    freeReplyObject(reply);
    reply = xRedisCommand(c,"LSET languages2 0 python");
    if (reply != NULL) {
        printf(" reply str 111111:%s\n", reply->str);
        test_cond( strcasecmp(reply->str,"OK") == 0)
            freeReplyObject(reply);
    } else {
        printf(" error\n"); 
    }

    test("Test LTRIM ");
    reply = xRedisCommand(c,"LPUSH alpha o, l l e h" );
    freeReplyObject(reply);
    reply = xRedisCommand(c,"LTRIM alpha 1 -1" );
    if (reply != NULL) {
        test_cond( strcasecmp(reply->str,"OK") == 0)
            freeReplyObject(reply);
    } 

    test("Test RPOPLPUSH ");
    reply = xRedisCommand(c,"RPOPLPUSH alpha reciver" );
    if (reply != NULL) {
        freeReplyObject(reply);
    }
    reply = xRedisCommand(c, "llen reciver");
    if (reply != NULL) {
        test_cond( reply->integer > 0)
            freeReplyObject(reply);
    }
    
    test("Test BLPOP timeout=0");
    reply = xRedisCommand(c,"lpush command %s", "update system..." );
    reply = xRedisCommand(c,"lpush command %s", "update system..." );
    reply = xRedisCommand(c,"lpush command %s", "update system..." );
    reply = xRedisCommand(c,"lpush command %s", "update system..." );
    reply = xRedisCommand(c,"lpush command %s", "update system..." );
    reply = xRedisCommand(c,"lpush request %s", "visit page..." );
    reply = xRedisCommand(c,"lpush request %s", "visit page..." );
    reply = xRedisCommand(c,"lpush request %s", "visit page..." );
    reply = xRedisCommand(c,"lpush request %s", "visit page..." );
    reply = xRedisCommand(c,"lpush request %s", "visit page..." );
    if (reply != NULL) {
        freeReplyObject(reply);
    }

    reply = xRedisCommand(c, "BLPOP job command request 0");
    if (reply != NULL) {
        printf(" reply:%s\n", reply->str);
        test_cond(reply->elements > 0)
            freeReplyObject(reply);
    }

    test("Test LINDEX ")
    reply = xRedisCommand(c, "LPUSH test1 t");
    reply = xRedisCommand(c, "LINDEX test1 0");
    if (reply != NULL) {
        printf(" reply:%s\n", reply->str);
        test_cond( strcasecmp(reply->str, "t") == 0)
            freeReplyObject(reply);
    }

    test("Test LINSERT")
    reply = xRedisCommand(c, "RPUSH mylist hello");
    reply = xRedisCommand(c, "RPUSH mylist world");
    if(reply != NULL) {
        freeReplyObject(reply);
    }
    reply = xRedisCommand(c, "LINSERT mylist before %s %s", "world", "there");
    if (reply != NULL) {
        printf(" reply:%s\n", reply->str);
        test_cond( reply->integer > 2)
            freeReplyObject(reply);
    }

    //sleep(2);
    xRedisFree(c);
    printf("Finished test\n");
    return NULL;
}

void* test_list_write(void* arg) {
    con_config* cfg = (con_config*)arg;
    xRedisContext *c;
    redisReply *reply;

    printf("write connect ip:%s port:%d\n", cfg->hostname, cfg->port);
    c = xRedisConnect(cfg->hostname, cfg->port);

    test("Test LPUSH ");
    reply = xRedisCommand(c,"LPUSH languages %s %s %s", "python", "java", "C++");
    if (reply != NULL) {
        test_cond( reply->integer == 3)
            freeReplyObject(reply);
    } else {
        test(" error\n"); 
    }

    test("Test RPUSH ");
    reply = xRedisCommand(c," RPUSH languages %s %s", "js", "go");
    if (reply != NULL) {
        test_cond( reply->integer == 5)
            freeReplyObject(reply);
    } else {
       test(" error\n"); 
    }

    test("Test LPOP ");
    reply = xRedisCommand(c,"LPUSH languages1 %s %s %s %s %s", "python", "java", "C++", "js", "go");
    freeReplyObject(reply);
    reply = xRedisCommand(c,"LPOP languages1 ");
    if (reply != NULL) {
        test_cond( strcasecmp(reply->str,"go") == 0)
            freeReplyObject(reply);
    } else {
        printf(" error\n"); 
    }

    

    test("Test LPUSHX ");
    reply = xRedisCommand(c,"lpushx lang ll");
    if (reply != NULL) {
        test_cond( reply->integer == 0)
            freeReplyObject(reply);
    } else {
        printf(" error\n"); 
    }


    test("Test LPUSHX 2");
    reply = xRedisCommand(c,"LPUSH languages3 %s %s %s %s %s", "python", "java", "C++", "js", "go");
    freeReplyObject(reply);
    reply = xRedisCommand(c,"lpushx languages3 ll");
    if (reply != NULL) {
        test_cond( reply->integer > 0)
            freeReplyObject(reply);
    } else {
        printf(" error\n"); 
    }

    test("Test LRANGE ");
    reply = xRedisCommand(c, "lpush testrange 1");
    freeReplyObject(reply);
    reply = xRedisCommand(c,"LRANGE testrange 0 -1");
    if (reply != NULL) {
        test_cond( reply->elements > 0)
        printf(" reply->str :%s\n", reply->str);
            freeReplyObject(reply);
    } else {
        printf(" error\n"); 
    }

    test("Test LREM ");
    reply = xRedisCommand(c,"LPUSH languages4 %s %s %s %s %s", "python", "java", "C++", "js", "go");
    freeReplyObject(reply);
    reply = xRedisCommand(c,"LREM languages4 1 python");
    if (reply != NULL) {
        test_cond( reply->integer == 1)
        printf(" reply->integer :%d\n", reply->integer);
            freeReplyObject(reply);
    } else {
        printf(" error\n"); 
    }

    test("Test LSET ");
    reply = xRedisCommand(c,"LPUSH l5 %s %s %s %s %s", "python", "java", "C++", "js", "go");
    freeReplyObject(reply);
    reply = xRedisCommand(c,"LSET l5 0 php");
    if (reply != NULL) {
        test_cond( strcasecmp(reply->str,"OK") == 0)
            freeReplyObject(reply);
    } else {
        printf(" error\n"); 
    }

    test("Test LTRIM ");
    reply = xRedisCommand(c,"LPUSH alpha o l l e h" );
    freeReplyObject(reply);
    reply = xRedisCommand(c,"LTRIM alpha 1 -1" );
    if (reply != NULL) {
        test_cond( strcasecmp(reply->str,"OK") == 0)
            freeReplyObject(reply);
    } 

    test("Test RPOPLPUSH ");
    reply = xRedisCommand(c,"LPUSH alpha1 o l l e h" );
    freeReplyObject(reply);
    reply = xRedisCommand(c,"RPOPLPUSH alpha1 reciver" );
    if (reply != NULL) {
        freeReplyObject(reply);
    }
    reply = xRedisCommand(c, "llen reciver");
    if (reply != NULL) {
        test_cond( reply->integer == 1)
            freeReplyObject(reply);
    }
    
    test("Test BLPOP timeout=0");
    reply = xRedisCommand(c,"lpush command %s", "update system..." );
    reply = xRedisCommand(c,"lpush request %s", "visit page..." );
    if (reply != NULL) {
        freeReplyObject(reply);
    }

    reply = xRedisCommand(c, "BLPOP job command request 0");
    if (reply != NULL) {
        printf(" reply:%s\n", reply->str);
        test_cond(reply->elements > 0)
            freeReplyObject(reply);
    }

    test("Test LINDEX ")
    reply = xRedisCommand(c, "LPUSH test1 t");
    reply = xRedisCommand(c, "LINDEX test1 0");
    if (reply != NULL) {
        printf(" reply:%s\n", reply->str);
        test_cond( strcasecmp(reply->str, "t") == 0)
            freeReplyObject(reply);
    }

    test("Test LINSERT")
    reply = xRedisCommand(c, "RPUSH mylist hello");
    reply = xRedisCommand(c, "RPUSH mylist world");
    if(reply != NULL) {
        freeReplyObject(reply);
    }
    reply = xRedisCommand(c, "LINSERT mylist before %s %s", "world", "there");
    if (reply != NULL) {
        printf(" reply:%s\n", reply->str);
        test_cond( reply->integer == 3)
            freeReplyObject(reply);
    }

    //sleep(2);
    xRedisFree(c);
    printf("Finished test\n");
    return NULL;
}

void* test_list_read(void* arg) {
    con_config* cfg = (con_config*)arg;
    xRedisContext *c;
    redisReply *reply;

    printf(" connect ip:%s port:%d\n", cfg->hostname, cfg->port);
    c = xRedisConnect(cfg->hostname, cfg->port);

    sleep(1);
    test("Test LLEN");
    reply = xRedisCommand(c," LLEN languages");
    if (reply != NULL) {
        test_cond( reply->integer == 5)
            freeReplyObject(reply);
    } else {
       test(" error\n"); 
    }

    test("Test LRANGE ");
    reply = xRedisCommand(c,"LRANGE languages1 0 -1");
    if (reply != NULL) {
        printf(" languages2 all elements :\n");
        print_array(reply);
        test_cond( reply->elements == 4);
        freeReplyObject(reply);
    } else {
        printf(" error\n"); 
    }


    test("Test LPUSHX ");
    reply = xRedisCommand(c,"LRANGE languages3 0 -1");
    if (reply != NULL) {
        printf(" languages3 all elements :\n");
        print_array(reply);
        test_cond( reply->elements == 6);
        freeReplyObject(reply);
    } else {
        printf(" error\n"); 
    }

    test("Test LREM ");
    reply = xRedisCommand(c,"lrange languages4 0 -1");
    if (reply != NULL) {
        print_array(reply);
        test_cond( reply->elements > 0)
            freeReplyObject(reply);
    } else {
        printf(" error\n"); 
    }

    test("Test LSET 3");
    reply = xRedisCommand(c,"lrange l5 0 -1");
    if (reply != NULL) {
        print_array(reply);
        test_cond( reply->elements > 0)
            freeReplyObject(reply);
    } else {
        printf(" error\n"); 
    }

    test("Test LTRIM 3");
    reply = xRedisCommand(c,"lrange alpha 0 -1");
    if (reply != NULL) {
        print_array(reply);
        test_cond( reply->elements > 0)
            freeReplyObject(reply);
    } else {
        printf(" error\n"); 
    }

    test("Test rpoplpush 3");
    reply = xRedisCommand(c,"lrange reciver 0 -1");
    if (reply != NULL) {
        print_array(reply);
        test_cond( reply->elements > 0)
            freeReplyObject(reply);
    } else {
        printf(" error\n"); 
    }

    test("Test LTRIM 3");
    reply = xRedisCommand(c,"lrange alpha 0 -1");
    if (reply != NULL) {
        print_array(reply);
        test_cond( reply->elements > 0)
            freeReplyObject(reply);
    } else {
        printf(" error\n"); 
    }

    test("Test LINDEX ")
    reply = xRedisCommand(c, "LINDEX test1 0");
    if (reply != NULL) {
        printf(" reply:%s\n", reply->str);
        test_cond( strcasecmp(reply->str, "t") == 0)
            freeReplyObject(reply);
    }

    test("Test LINSERT")
    reply = xRedisCommand(c, "lrange mylist 0 -1");
    if (reply != NULL) {
        print_array(reply);
        test_cond( reply->elements == 3)
            freeReplyObject(reply);
    }

    //sleep(2);
    xRedisFree(c);
    printf("Finished test\n");
    return NULL;
}

void* test_disconn(void* arg) {
    con_config* cfg = (con_config*)arg;
    xRedisContext *c;
    redisReply *reply;

    c = xRedisConnect(cfg->hostname, cfg->port);

    sleep(1);
    reply = xRedisCommand(c,"lpush discon  2 3 ");
    if (reply != NULL) {
        freeReplyObject(reply);
    } else {
       test(" error\n"); 
    }

    //close(c->fd);

    //printf("reconnect....\n");
    //xRedisReconnect(c);
    //printf("finished reconnect....\n");

    test("Test disconn ");
    reply = xRedisCommand(c,"LRANGE discon 0 -1");
    if (reply != NULL) {
        printf(" discon all elements :\n");
        print_array(reply);
        test_cond( reply->elements == 2);
        freeReplyObject(reply);
    } else {
        printf(" error\n"); 
    }

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
        test(" error\n"); 
    }

    pthread_t p_id[100];
    int i = 0;
    con_config cfg2;
    cfg2.hostname = "127.0.0.1";
    cfg2.port = 6379;
    /* test multiple thread operation */
    for (i = 0; i < 3; ++i) {
        pthread_create(&p_id[i], NULL, test_list_cmd, &cfg);
    }
    pthread_create(&p_id[3], NULL, test_list_cmd, &cfg2);
    for (i = 0; i < 4; ++i) {
        pthread_join(p_id[i], &status);
    }

    reply = xRedisCommand(c,"FLUSHDB");
    if (reply != NULL) {
        test_cond(reply->type == REDIS_REPLY_STATUS &&
                strcasecmp(reply->str,"OK") == 0)
            freeReplyObject(reply);
    } else {
        test(" error\n"); 
    }
    /* test one thread write the other read */
    /* not support blpop brpop bpoplpush */
    pthread_create(&p_id[0], NULL, test_list_write, &cfg);
    pthread_create(&p_id[1], NULL, test_list_read, &cfg);
    for (i = 0; i < 2; ++i) {
        pthread_join(p_id[i], &status);
    }

    xRedisFree(c);
    /* test disconnect */
    //pthread_create(&p_id[0], NULL, test_disconn, &cfg);
    //pthread_join(p_id[0], &status);

    xStopClient();
    sleep(2);
    return 0;
}

