#include "hiredis.h"


void* rediscmd1(void* arg)
{
    int i;
    for(i = 0; i < 100; ++i)
    {
        redisReply* reply = redisMulCommand("incrby test1 2"); 
        if (reply == NULL) {
            printf(" reply is null\n");
        } else {
            printf(" reply:%s\n", reply->str);
        }
        freeReplyObject(reply);
    }
}

void* rediscmd2(void* arg)
{
    int i;
    for(i = 0; i < 100; ++i)
    {
        char key[10] = { 0 };
        itoa(i, key, 10);

        redisReply* reply = redisMulCommand("incrby test2 2"); 
        if (reply == NULL) {
            printf(" reply is null\n");
        } else {
            printf(" reply:%s\n", reply->str);
        }
        freeReplyObject(reply);
    }
}

int main (int argc, char **argv) {

    int ret = 0;
    ret = initClient("127.0.0.1", 6379, 2);
    printf(" initClient return:%d\n", ret);
    if(ret != 0) {
        return -1;
    }

    redisReply* reply = redisMulCommand("set test 2"); 
    if (reply == NULL) {
        printf(" reply is null\n");
    } else {
        printf(" reply:%s\n", reply->str);
    }
    freeReplyObject(reply);

    reply = redisMulCommand("get test"); 
    if (reply == NULL) {
        printf(" reply is null\n");
    } else {
        printf(" reply:%s\n", reply->str);
    }
    freeReplyObject(reply);

    pthread_t p1;
    pthread_t p2;
    pthread_create(&p1, NULL, rediscmd1, NULL);
    pthread_create(&p2, NULL, rediscmd2, NULL);

    pthread_join(&p1);
    pthread_join(&p2);

    return 0;
}
