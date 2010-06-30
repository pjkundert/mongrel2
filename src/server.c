#include <dbg.h>
#include <task/task.h>
#include <listener.h>
#include <register.h>
#include <server.h>
#include <host.h>
#include <assert.h>
#include <string.h>
#include <mem/halloc.h>

Server *Server_create(const char *port)
{
    Server *srv = h_calloc(sizeof(Server), 1);
    check(srv, "Out of memory.");

    srv->hosts = RouteMap_create();
    check(srv->hosts, "Failed to create host RouteMap.");

    srv->port = atoi(port);
    check(port > 0, "Can't bind to the given port: %s", port);

    srv->listen_fd = netannounce(TCP, 0, srv->port);
    check(srv->listen_fd >= 0, "Can't announce on TCP port %d", srv->port);

    check(fdnoblock(srv->listen_fd) == 0, "Failed to set listening port %d nonblocking.", srv->port);

    return srv;

error:
    Server_destroy(srv);
    return NULL;
}

void Server_destroy(Server *srv)
{
    if(srv) {
        RouteMap_destroy(srv->hosts);
        close(srv->listen_fd);
        h_free(srv);
    }
}


void Server_init()
{
    mqinit(2);
    Register_init();
    Listener_init();
}


void handlers_receive_start(void *value, void *data)
{
    Route *route = (Route *)value;
    if(route) { 
        Backend *found = (Backend *)route->data;

        // TODO: make this optional
        if(found->type == BACKEND_HANDLER) {
            debug("LOADING BACKEND %s", bdata(route->pattern));
            taskcreate(Handler_task, found->target.handler, HANDLER_STACK);
        }
    }
}

void Server_start(Server *srv)
{
    int cfd;
    int rport;
    char remote[IPADDR_SIZE];

    check(srv->default_host, "No default_host set.");

    taskname("SERVER");

    debug("Starting server on port %d", srv->port);

    tst_traverse(srv->default_host->routes->routes, handlers_receive_start, srv);

    while((cfd = netaccept(srv->listen_fd, remote, &rport)) >= 0) {
        debug("Connection from %s:%d to %s:%d", remote, rport, 
                bdata(srv->default_host->name), srv->port);

        taskstate("accepting");
        Listener_accept(srv, cfd, rport, remote);
        taskstate("waiting");
    }

    return;

error:
    taskexitall(1);
}



int Server_add_host(Server *srv, bstring pattern, Host *host)
{
    return RouteMap_insert_reversed(srv->hosts, pattern, host);
}


void Server_set_default_host(Server *srv, Host *host)
{
    srv->default_host = host;
}
