#include <stdio.h>  
#include <string.h>  
#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>
#include <strings.h>

#include"zookeeper.h"
#include"zookeeper_log.h"

enum WORK_MODE {
    MODE_MONITOR,
    MODE_WORKER
} g_mode;

static char g_host[512]= "127.0.0.1:2181";  
static int connected = 0;
static int expired = 0;
static zhandle_t *zh;

//watch function when child list changed
void zktest_watcher_g(zhandle_t* zh, int type, int state, const char* path, void* watcherCtx);
//show all process ip:pid
void show_list(const struct String_vector *workers_list);
//if success,the g_mode will become MODE_MONITOR
void run_for_master();
//get localhost ip:pid
void getlocalhost(char *ip_pid, int len);
void get_workers();
void create_worker();
void print_usage();
void master_exists();
void create_parent(const char *path, const char *value);
void create_workers_parent(const char *path, const char *value);

void get_option(int argc, const char* argv[]);

/**********unitl*********************/  
void print_usage()
{
    printf("Usage : [monitor] [-h] [-m] [-s ip:port] \n");
    printf("        -h Show help\n");
    printf("        -m set monitor mode\n");
    printf("        -s server ip:port\n");
    printf("For example:\n");
    printf("monitor -m -s 127.0.0.1:2181 \n");
}
 
void get_option(int argc, const char* argv[])
{
    extern char *optarg;
    int optch;
    int dem = 1;
    const char optstring[] = "hms:";

    //default    
    g_mode = MODE_WORKER;
    while((optch = getopt(argc, (char * const *)argv, optstring)) != -1) {
        switch (optch) {
            case 'h':
                print_usage();
                exit(-1);
            case '?':
                print_usage();
                printf("unknown parameter: %c\n", optopt);
                exit(-1);
            case ':':
                print_usage();
                printf("need parameter: %c\n", optopt);
                exit(-1);
            case 'm':
                g_mode = MODE_MONITOR;
                break;
            case 's':
                strncpy(g_host, optarg, sizeof(g_host));
                break;
            default:
                break;
        }
    }
} 

void getlocalhost(char *ip_pid, int len)
{
    char hostname[64] = {0};
    struct hostent *hent ;

    gethostname(hostname, sizeof(hostname));
    hent = gethostbyname(hostname);

    char *localhost = inet_ntoa(*((struct in_addr*)(hent->h_addr_list[0])));

    snprintf(ip_pid, len,"%s:%lld", localhost, getpid());
}


void show_list(const struct String_vector *tmp_workers)
{
    printf("--------------\n");
    printf("workerlist:\n");
    for (int i = 0; i < tmp_workers->count; i++) {
        printf("%s\n", tmp_workers->data[i]);
    }
}

void main_watcher(zhandle_t *zh, int type, int state, const char *path, void *context)
{
    if (type == ZOO_SESSION_EVENT) {
        if (state == ZOO_CONNECTED_STATE) {
            connected = 1;
        } else if (state == ZOO_NOTCONNECTED_STATE) {
            connected = 0;
        } else if (state == ZOO_EXPIRED_SESSION_STATE) {
            expired = 1;
            connected = 0;
            zookeeper_close(zh);
        }
    }
}

int init(char *hostport, int timeout)
{
    zoo_set_debug_level(ZOO_LOG_LEVEL_WARN);
    zh = zookeeper_init(hostport,
                        main_watcher, 
                        timeout, 0, (char *)"Monitor Test", 0);
    if (zh == NULL) {
        fprintf(stderr, "Error when connecting to zookeeper servers...\n");
        return errno;
    }

    return 0;
}

int is_expired() {
    return expired;
}

int is_connected() {
    return connected;
}

void workers_completion(int rc, const struct String_vector *strings, const void *data)
{
    switch (rc) {
        case ZCONNECTIONLOSS:
        case ZOPERATIONTIMEOUT:
            get_workers();
            break;
        case ZOK:
            show_list(strings);
            break;
        default:
            fprintf(stderr, "Something went wrong wen checking workers: %d\n", rc);
            break;
    }
}

void workers_watcher(zhandle_t *zh, int type, int state, const char *path, void *context)
{
    fprintf(stdout, "[output]: workers wather wakeup: %d, %d\n", type, state);
    if (type == ZOO_CHILD_EVENT) {
        get_workers();
    }
}

void get_workers()
{
    zoo_awget_children(zh, "/workers",
                       workers_watcher, NULL,
                       workers_completion, NULL);
}

void create_workers_parent_completion(int rc, const char *value, const void *data)
{
    switch (rc) {
        case ZCONNECTIONLOSS:
            create_workers_parent(value, data);
            break;
        case ZOK:
            fprintf(stdout, "[debug]: create workers success.\n");
            zoo_awget_children(zh, "/workers",
                               workers_watcher, NULL,
                               workers_completion, NULL);
            break;
        case ZNODEEXISTS:
            break;
        default:
            fprintf(stderr, "[error]: Something went wrong when create %s.\n", value);
            break;
    }
}

void create_workers_parent(const char *path, const char *value)
{
    zoo_acreate(zh, path, value, 0, 
              &ZOO_OPEN_ACL_UNSAFE, 0, 
              create_workers_parent_completion, NULL);
}

void take_leadership()
{
    create_workers_parent("/workers", "");
    // get_workers();
}

void master_exists_completion(int rc, const struct Stat *stat, const void *data)
{
    switch (rc) {
        case ZCONNECTIONLOSS:
        case ZOPERATIONTIMEOUT:
            master_exists();
            break;
        case ZOK:
            if (stat == NULL) {
                fprintf(stderr, "Previous master is gone, runngin for master");
                run_for_master();
            } else {
                g_mode = MODE_MONITOR;
            }
            break;
        case ZNODEEXISTS:
            fprintf(stderr, "master node exists.\n");
            break;
        default:
            fprintf(stderr, "Something went wrong when executing exists: %d\n", rc);
            break;
    }
}

void master_exist_watcher(zhandle_t *zh, int type, int state, const char *path, void *watherCtx)
{
    if (type == ZOO_DELETED_EVENT) {
        run_for_master();
    } else {
        fprintf(stdout, "[debug]: Watch event: %d", type);
    }
}

void master_exists()
{
    zoo_awexists(zh, "/master", 
                master_exist_watcher, NULL,
                master_exists_completion, NULL);
}

void master_create_completion(int rc, const char *value, const void *data)
{
    switch (rc) {
        case ZCONNECTIONLOSS:
            run_for_master();
            break;
        case ZOK:
            take_leadership();
            break;
        case ZNODEEXISTS:
            fprintf(stdout, "[debug]: master has alreay existed.\n");
            master_exists();
            g_mode = MODE_WORKER;
            if (g_mode != MODE_MONITOR) {
                create_worker();
            }
            break;
        default:
            fprintf(stderr, "Somthing went wrong running for master: %d", rc);
            break;
    }
}

void run_for_master()
{
    if (!is_connected()) {
        fprintf(stderr, "Client not connected to Zookeeper.\n");
        return ;
    }

    char localhost[512]={0};
    getlocalhost(localhost, sizeof(localhost));
    zoo_acreate(zh, "/master", (const char *)localhost, strlen(localhost),
                &ZOO_OPEN_ACL_UNSAFE, ZOO_EPHEMERAL,
                master_create_completion, NULL);
}

void create_parent_completion(int rc, const char *value, const void *data)
{
    switch (rc) {
        case ZCONNECTIONLOSS:
            create_parent(value, (const char *)data);
            break;
        case ZOK:
            fprintf(stdout, "[output]: Create parent node %s.\n", value);
            
            break;
        case ZNODEEXISTS:
            fprintf(stdout, "[output]: Node '%s' has already exists.\n", value);
            break;
        default:
            fprintf(stderr, "[error]: Something went wrong when running for master.\n");
            break;
    }
}

void create_parent(const char *path, const char *value)
{
    zoo_acreate(zh, path, value, 0, 
              &ZOO_OPEN_ACL_UNSAFE, 0, 
              create_parent_completion, NULL);
}

void bootstrap()
{
    if (!is_connected()) {
        fprintf(stderr, "Client not connected to zookeeper");
        return ;
    }

    create_parent("/assign", "");
    create_parent("/tasks", "");
    create_parent("/status", "");
}

void worker_create_completion(int rc, const char *value, const void *data)
{
    switch (rc) {
        case ZCONNECTIONLOSS:
            create_worker();
            break;
        case ZOK:
            fprintf(stdout, "[output]: worker create success.\n");
            g_mode = MODE_WORKER;
            break;
        case ZNODEEXISTS:
            fprintf(stdout, "[output]: worker node has already existed, create again\n");
            create_worker();
            break;
        default:
            fprintf(stderr, "[error]: Something went wrong when create woker.\n");
            break;
    }
}

void create_worker()
{
    char localhost[512] = {0};
    getlocalhost(localhost, sizeof(localhost));
    char worker_path[512];
    sprintf(worker_path, "/workers/worker-");

    zoo_acreate(zh, worker_path, localhost, strlen(localhost),
                &ZOO_OPEN_ACL_UNSAFE, ZOO_SEQUENCE | ZOO_EPHEMERAL,
                worker_create_completion, NULL);
}

int main(int argc, const char *argv[])  
{   
    int  initialized = 0;
    int  timeout = 30000;   
    int  run = 0;
    int  ret = 0;
    get_option(argc, argv);

    if (!initialized) {
        ret = init(g_host, timeout);
        if (ret != 0) {
            fprintf(stderr, "Init Failed: %d\n", ret);
            return -1;
        }
        initialized = 1;
        sleep(4);
    }
    
    if (is_connected() && !run) {
        bootstrap();

        run_for_master();
        run = 1;
    }
  
    getchar();
    zookeeper_close(zh); 
    return 0;
}  
