#include "socket/server.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

using namespace skynet;

static void* _poll(void* ud)
{
    socket::socket_server* ss = (socket::socket_server*)ud;
    socket::socket_message result;
    for (;;)
    {
        bool is_more = false;
        int type = ss->poll_socket_event(&result, is_more);
        // DO NOT use any ctrl command (socket_server_close , etc. ) in this thread.
        switch (type)
        {
        case socket::socket_event::EVENT_EXIT:
            return NULL;
        case socket::socket_event::EVENT_DATA:
            printf("message(%llu) [id=%d] size=%d\n", result.svc_handle, result.socket_id, result.ud);
            free(result.data);
            break;
        case socket::socket_event::EVENT_CLOSE:
            printf("close(%llu) [id=%d]\n", result.svc_handle, result.socket_id);
            break;
        case socket::socket_event::EVENT_OPEN:
            printf("open(%llu) [id=%d] %s\n", result.svc_handle, result.socket_id, result.data);
            break;
        case socket::socket_event::EVENT_ERROR:
            printf("error(%llu) [id=%d]\n", result.svc_handle, result.socket_id);
            break;
        case socket::socket_event::EVENT_ACCEPT:
            printf("accept(%llu) [id=%d %s] from [%d]\n", result.svc_handle, result.ud, result.data, result.socket_id);
            break;
        }
    }
}

static void test(socket::socket_server* ss)
{
    pthread_t pid;
    pthread_create(&pid, NULL, _poll, ss);

    int c = ss->connect(100, "127.0.0.1", 80);
    printf("connecting %d\n",c);

    int l = ss->listen(200, "127.0.0.1", 8888, 32);
    printf("listening %d\n",l);

    ss->start(201, l);
    int b = ss->bind(300, 1);
    printf("binding stdin %d\n",b);

    for (int i = 0; i < 100; i++)
    {
        ss->connect(400+i, "127.0.0.1", 8888);
    }
    sleep(5);

    ss->exit();

    pthread_join(pid, NULL); 
}

int main()
{
    struct sigaction sa;
    sa.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sa, 0);

    socket::socket_server* ss = new socket::socket_server();
    ss->init();
    test(ss);
    
    delete ss;

    return 0;
}