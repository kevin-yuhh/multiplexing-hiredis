#include <pthread.h>

class autolock  
{  
public:  
    autolock(pthread_mutex_t* lock): lk(lock)  
    {  
        pthread_mutex_lock(lk);
    }  

    ~autolock()  
    {  
        pthread_mutex_unlock(lk);
    }  

private:  
    pthread_mutex_t* lk;
};  
