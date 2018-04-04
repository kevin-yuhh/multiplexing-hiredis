#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include <vector>

#include <hiredis.h>
#include <async.h>
#include <adapters/ae.h>


using namespace std;

struct privData {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    redisReply* reply;
};

/* Put event loop in the global scope, so it can be explicitly stopped */
static aeEventLoop *loop;

void getCallback(redisAsyncContext *c, void *r, void *privdata) {
    redisReply *reply = (redisReply*)r;
    if (reply == NULL) return;

    if(privdata != NULL)
    {
        privData* priv = static_cast<privData*>(privdata);//->setValue(atoi(reply->str));
        pthread_mutex_lock(&priv->mutex);
        
        priv->reply = reply;
        pthread_cond_signal(&priv->cond);
        pthread_mutex_unlock(&priv->mutex);
    }
}

void connectCallback(const redisAsyncContext *c, int status) {
    if (status != REDIS_OK) {
        printf("Error: %s\n", c->errstr);
        aeStop(loop);
        return;
    }

    printf("Connected...\n");
}

void disconnectCallback(const redisAsyncContext *c, int status) {
    if (status != REDIS_OK) {
        printf("Error: %s\n", c->errstr);
        aeStop(loop);
        return;
    }

    printf("Disconnected...\n");
    aeStop(loop);
}
struct calldata {
    redisAsyncContext* c;
    struct privData* priv;
};

pthread_mutex_t g_mutex;

struct timespec wait_time_out;

std::vector<struct calldata>  call_queue;
void* incrby(void* arg)
{
    redisAsyncContext* c = (redisAsyncContext*)arg;

    struct privData priv;
    pthread_mutex_init(&priv.mutex,NULL);
    pthread_cond_init(&priv.cond,NULL);
    priv.reply = NULL;

    pthread_mutex_lock(&g_mutex);
    //redisAsyncCommand(c, getCallback, &priv, " get  key ");
    struct calldata cd;
    cd.c = c;
    cd.priv = &priv;
    call_queue.push_back(cd);
    pthread_mutex_unlock(&g_mutex);

    pthread_mutex_lock(&priv.mutex);
    while(priv.reply == NULL )
    {
        pthread_cond_timedwait(&priv.cond, &priv.mutex, &wait_time_out);
    }

    //std::cout << " get li:" << reply->str << std::endl;
    printf(" incrby func get %s repley %p\n", priv.reply->str, priv.reply);
    pthread_mutex_unlock(&priv.mutex);

    freeReplyObject(priv.reply);
    printf(" finished incrby\n");

    return NULL;
}

void* startloop(void* arg)
{
    aeMain(loop);
}

void* dealqueue(void* arg)
{
    while(1)
    {
        pthread_mutex_lock(&g_mutex);
        while(call_queue.size() > 0) {
            struct calldata cd = call_queue.back();
            call_queue.pop_back();

            redisAsyncCommand(cd.c, getCallback, cd.priv, " get  key ");
        }
        pthread_mutex_unlock(&g_mutex);
        sleep(1);
    }
}

int main (int argc, char **argv) {
    signal(SIGPIPE, SIG_IGN);

    wait_time_out.tv_sec = 1;
    pthread_mutex_init(&g_mutex, NULL);

    redisAsyncContext *c1 = redisAsyncConnect("127.0.0.1", 6379);
    redisAsyncContext *c2 = redisAsyncConnect("127.0.0.1", 6379);
    redisAsyncContext *c3 = redisAsyncConnect("127.0.0.1", 6379);
    if (c1->err) {
        /* Let *c leak for now... */
        printf("Error: %s\n", c1->errstr);
        return 1;
    }

    loop = aeCreateEventLoop(64);
    redisAeAttach(loop, c1);
    redisAeAttach(loop, c2);
    redisAeAttach(loop, c3);

    redisAsyncSetConnectCallback(c1,connectCallback);
    redisAsyncSetConnectCallback(c2,connectCallback);
    redisAsyncSetConnectCallback(c3,connectCallback);

    redisAsyncSetDisconnectCallback(c1,disconnectCallback);
    redisAsyncSetDisconnectCallback(c2,disconnectCallback);
    redisAsyncSetDisconnectCallback(c3,disconnectCallback);
    //redisAsyncCommand(c, NULL, NULL, "SET key %b", argv[argc-1], strlen(argv[argc-1]));
    //redisAsyncCommand(c, getCallback, (char*)"end-1", "GET key");
    //incrby(c1);
 
    pthread_t p_id;
    int ret = pthread_create(&p_id, NULL, startloop, NULL);
    ret = pthread_create(&p_id, NULL, dealqueue, NULL);
    //aeMain(loop);
    //incrby(c1);
    //incrby(c1);
    printf("after aeMain\n");

    /*
    incrby(c1);
    incrby(c1);
    incrby(c1);
    incrby(c1);
    incrby(c1);
    incrby(c1);
    */

    
    pthread_t pid[10];
    for(int i = 0; i < 10; ++i)
    {

        pthread_create(&pid[i], NULL, incrby, c1);
    }
    /*
    for(int i = 0; i < 5; ++i)
    {
        pthread_create(&pid[i], NULL, incrby, c1);
    }
    */

    //std::cout << " incrby return :" << ret << std::endl;

    while(1) {
        sleep(100);
    }
    printf("Quit\n");
    return 0;
}

