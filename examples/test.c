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


int test_connect(con_config cfg) {
    xRedisContext *c;
    redisReply *reply;

    test("redisConnect");
    c = xRedisConnect(cfg.hostname, cfg.port);

    reply = xRedisCommand(c,"PING");
    if (reply != NULL) {
        printf("%d ping reply :%s\n", __LINE__, reply->str); 
        test_cond(reply->type == REDIS_REPLY_STATUS &&
                strcasecmp(reply->str,"pong") == 0)
            freeReplyObject(reply);
    } else {
        printf("%d ping error\n", __LINE__); 
    }

    sleep(20);
    /* Set a key */
    reply = xRedisCommand(c,"SET %s %s", "foo", "hello world");
    if (reply == NULL) {
        printf(" set error\n");
        return -1;
    }
    printf("SET: %s\n", reply->str);
    freeReplyObject(reply);

    /* Set a key using binary safe API */
    reply = xRedisCommand(c,"SET %b %b", "bar", (size_t) 3, "hello", (size_t) 5);
    printf("SET (binary API): %s\n", reply->str);
    freeReplyObject(reply);

    /* Try a GET and two INCR */
    reply = xRedisCommand(c,"GET foo");
    printf("GET foo: %s\n", reply->str);
    freeReplyObject(reply);



    //struct timeval timeout = { 1, 500000 }; // 1.5 seconds
    //c = redisConnectWithTimeout(hostname, port, timeout);
    //c = redisConnectBindNonBlock(hostname, port, hostname);
    //c = redisConnectBindNonBlockWithReuse(hostname, port, hostname);
 
    xRedisFree(c);
    return 0;
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

    xRedisFree(c);
    return 0;
}

int test_hash_cmd(con_config cfg) {
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
        printf(" error"); 
    }

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
        printf("%d FLUSHDB error\n", __LINE__); 
    }

    test("Test HVALS ");
    reply = xRedisCommand(c,"HVALS website");
    if (reply != NULL) {
        print_array(reply);
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
    test("Test HMSET ");
    reply = xRedisCommand(c,"HMSET website google www.google.com yahoo www.yahoo.com" );
    if (reply != NULL) {
        test_cond( strcasecmp(reply->str,"OK") == 0)
            freeReplyObject(reply);
    }

    int j = 0;
    test("Test HMGET \n");
    reply = xRedisCommand(c,"HMGET website google yahoo" );
    if (reply != NULL) {
        if (reply->type == REDIS_REPLY_ARRAY) {
            for (j = 0; j < reply->elements; j++) {
                printf("%u) %s\n", j, reply->element[j]->str);
            }
        }       
        freeReplyObject(reply);
    }

    test("Test HGETALL \n");
    reply = xRedisCommand(c,"HGETALL website" );
    if (reply != NULL) {
        if (reply->type == REDIS_REPLY_ARRAY) {
            for (j = 0; j < reply->elements; j++) {
                printf("%u) %s\n", j, reply->element[j]->str);
            }
        }       
        freeReplyObject(reply);
    }

    test("Test HINCRBY \n");
    reply = xRedisCommand(c,"HINCRBY website google 1" );
    if (reply != NULL) {
        printf("  hincrby reply  str:%s integer:%d\n", reply->str, reply->integer);
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
    return 0;
}

int test_list_cmd(con_config cfg) {
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
        test(" error\n"); 
    }

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

    test("Test LLEN");
    reply = xRedisCommand(c," LLEN languages");
    if (reply != NULL) {
        test_cond( reply->integer == 5)
            freeReplyObject(reply);
    } else {
       test(" error\n"); 
    }

    test("Test LPOP ");
    reply = xRedisCommand(c,"LPOP languages");
    if (reply != NULL) {
        test_cond( strcasecmp(reply->str,"C++") == 0)
            freeReplyObject(reply);
    } else {
        printf(" error\n"); 
    }

    test("Test RPOP ");
    reply = xRedisCommand(c,"RPOP languages");
    if (reply != NULL) {
        test_cond( strcasecmp(reply->str,"go") == 0)
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
    reply = xRedisCommand(c,"LREM languages 1 python");
    if (reply != NULL) {
        test_cond( reply->integer == 1)
        printf(" reply->integer :%d\n", reply->integer);
            freeReplyObject(reply);
    } else {
        printf(" error\n"); 
    }

    test("Test LSET ");
    reply = xRedisCommand(c,"LSET languages 0 python");
    if (reply != NULL) {
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

    xRedisFree(c);
    return 0;

}

int test_set_cmd(con_config cfg) {
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
        test(" error\n"); 
    }

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
        test_cond( reply->integer == 1)
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
    reply = xRedisCommand(c,"SMOVE peter bet_man");
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
        test_cond( reply->elements = 1)
        printf(" reply->str :%s\n", reply->str);
            freeReplyObject(reply);
    } else {
        printf(" error\n"); 
    }

    xRedisFree(c);
    return 0;

}



int main(int argc, char **argv) {
    unsigned int j;
    redisReply *reply;
    void *status;
    con_config cfg;
    cfg.hostname = (argc > 1) ? argv[1] : "127.0.0.1";
    cfg.port = (argc > 2) ? atoi(argv[2]) : 6379;

    test_connect(cfg);
    /*
    test_string_cmd(cfg);
    test_hash_cmd(cfg);
    test_list_cmd(cfg);
    test_set_cmd(cfg);
    */

    xStopClient();
    return 0;
}

