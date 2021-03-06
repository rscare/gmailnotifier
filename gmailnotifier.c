#include "gmailxml.h"
#include <rpass.h>

#include <sys/socket.h>
#include <sys/un.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <curl/curl.h>
#include <gpgme.h>
#include <libnotify/notify.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <libxml/parser.h>

#define URL "https://mail.google.com/mail/feed/atom"
#define PASSFILE "/home/redscare/.rpasswords"
#define WAITTIME 7 // How long to wait between tries
#define MAXTRIES 7
#define NOTIFICATION_SLEEP 180 // How long to wait between notifications
#define APPNAME "rgmailnotifier"
#define LOGFILE "gmailnotifier.log"

unsigned short int *new_msgs = NULL, running = 1;
int shmid = 0;

typedef struct {
    char *memory;
    size_t size;
} MemoryStruct;

static size_t WriteMemoryCallback(void *ptr, size_t size, size_t nmemb, void *data) {
    size_t realsize = size * nmemb;
    MemoryStruct *mem = (MemoryStruct *)data;

    mem->memory = realloc(mem->memory, mem->size + realsize + 1);
    if (mem->memory == NULL) {
        printf("Ran out of memory!\n");
        exit(EXIT_FAILURE);
    }
    memcpy(&(mem->memory[mem->size]), ptr, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = '\0';

    return realsize;
}

void SetupSHM() {
    key_t shmkey = ftok(PASSFILE, 'G');
    shmid = shmget(shmkey, sizeof(*new_msgs), 0644|IPC_CREAT);
    new_msgs = shmat(shmid, 0, 0);
}

void signal_handler(int sig) {
    switch (sig) {
    case SIGINT:
        running = 0;
        break;
    case SIGUSR1:
        printf("%d\n", *new_msgs);
        break;
    case SIGUSR2:
        break;
    case SIGTERM:
        running = 0;
        break;
    default:
        break;
    }
}

void SetupSignals() {
    struct sigaction sa;

    sa.sa_handler = signal_handler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);

    sigaction(SIGUSR1, &sa, NULL);
    sigaction(SIGUSR2, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

void ConstructDaemonMsg(void *msg, int nargs, ...) {
    va_list argp;
    void *msg_ptr = msg, *tmp;
    int i;
    size_t tmp_size;
    va_start(argp, nargs);

    for (i = 0; i < nargs; ++i) {
        tmp = va_arg(argp, void *);
        tmp_size = va_arg(argp, size_t);

        memcpy(msg_ptr, tmp, tmp_size);
        msg_ptr += tmp_size;
        if (i < nargs - 1)
            *(char *)(msg_ptr++) = ' ';
    }

    va_end(argp);
}

int GetUSERPWD(char **str, const char * const acname) {
    int s, len, in_len, remaining;
    int flags = 0;
    struct sockaddr_un remote;

    void *msg, *msg_ptr;

    char tmp[BUFSIZ], *user, *pass;

    rpass_parent *p; rpass_entry *e;

    if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        exit(1);
    }

    remote.sun_family = AF_UNIX;
    strcpy(remote.sun_path, SOCKET_NAME);
    len = strlen(remote.sun_path) + sizeof(remote.sun_family);
    if (connect(s, (struct sockaddr *)&remote, len) == -1) {
        exit(1);
    }

    len = strlen(RPASS_DAEMON_MSG_GETACCOUNTS)
        + 1 + strlen(PASSFILE)
        + 1 + strlen(RPASS_DAEMON_AC_START)
        + 1 + strlen(acname)
        + 1 + strlen(RPASS_DAEMON_AC_START)
        + 1 + sizeof(int)
        + 1 + strlen("user")
        + 1 + strlen("pass");

    msg = malloc(len);

    ConstructDaemonMsg(msg, 8,
                       RPASS_DAEMON_MSG_GETACCOUNTS, strlen(RPASS_DAEMON_MSG_GETACCOUNTS),
                       PASSFILE, strlen(PASSFILE),
                       RPASS_DAEMON_AC_START, strlen(RPASS_DAEMON_AC_START),
                       acname, strlen(acname),
                       RPASS_DAEMON_AC_START, strlen(RPASS_DAEMON_AC_START),
                       &flags, sizeof(int),
                       "user", strlen("user"),
                       "pass", strlen("pass"));

    send(s, msg, len, 0);
    free(msg);

    msg_ptr = msg = malloc(BUFSIZ + 1);
    len = BUFSIZ + 1;
    remaining = BUFSIZ;

    while (in_len = recv(s, tmp, BUFSIZ, MSG_WAITALL)) {
        remaining -= in_len;
        if (remaining < 0) {
            len += BUFSIZ;
            remaining += BUFSIZ;
            msg = realloc(msg, len);
        }
        memcpy(msg_ptr, tmp, in_len);
        msg_ptr += in_len;
        if (in_len < BUFSIZ) {
            break;
        }
    }

    *(char *)(++msg_ptr) = '\0';

    createRpassParentFromString(&p, msg);
    free(msg);

    e = p->first_entry;
    while(e) {
        if (strcmp(e->key, "user") == 0) {
            user = e->value;
        }
        else if (strcmp(e->key, "pass") == 0) {
            pass = e->value;
        }
        e = e->next_entry;
    }

    *str = malloc(strlen(user) + 1 + strlen(pass) + 1);
    sprintf(*str, "%s:%s", user, pass);

    freeRpassParents(p);

    return 0;
}

void SetupCurl(CURL **handle, const char *acname) {
    char *userpwd = NULL;

    curl_global_init(CURL_GLOBAL_ALL);

    *handle = curl_easy_init();

    curl_easy_setopt(*handle, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(*handle, CURLOPT_URL, URL);

    GetUSERPWD(&userpwd, acname);
    curl_easy_setopt(*handle, CURLOPT_USERPWD, userpwd);
    free(userpwd);

    curl_easy_setopt(*handle, CURLOPT_WRITEFUNCTION, &WriteMemoryCallback);
}

void CleanupCurl(CURL **handle) {
    curl_easy_cleanup(*handle);
    curl_global_cleanup();
}

void daemonize () {
    pid_t pid;

    if (getppid() == 1)
        return;

    pid = fork();
    if (pid < 0)
        exit(1);
    if (pid > 0)
        exit(0);

    if (setsid() < 0)
        exit(1);

    if (chdir("/tmp") < 0)
        exit(1);
}

void close_std_fds() {
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
}

int main(int argc, char *argv[]) {
    CURL *easyhandle = NULL;
    unsigned short int failurecount = 0, foreground = 0, verbose = 0;
    MemoryStruct mem;
    char *acname = NULL;


    // Commandline options parsing
    if (argc > 1) {
        int c;
        for (c = 1; c < argc; ++c) {
            if ((strcmp(argv[c], "--help") == 0) || (strcmp(argv[c], "-h") == 0)) {
                printf("Options: [--foreground|-F] [--verbose|-v] RPASS_ACCOUNT_NAME\n");
                exit(0);
            }
            else if ((strcmp(argv[c], "--foreground") == 0) || (strcmp(argv[c], "-F") == 0))
                foreground = 1;
            else if ((strcmp(argv[c], "--verbose") == 0) || (strcmp(argv[c], "-v") == 0))
                verbose = 1;
            else
                acname = argv[c];
        }
    }

    if ((argc <= 1) || acname == NULL) {
        printf("Usage: %s [OPTIONS] RPASS_ACCOUNT_NAME\n", argv[0]);
        exit(0);
    }

    if (!foreground)
        daemonize();

    SetupCurl(&easyhandle, acname);

    if (!foreground)
        close_std_fds();

    mem.memory = malloc(sizeof(char));
    mem.size = 0;

    SetupSignals();
    SetupSHM();

    curl_easy_setopt(easyhandle, CURLOPT_WRITEDATA, &mem);

    notify_init(APPNAME);
    while (running) {
        while (curl_easy_perform(easyhandle) != 0) {
            free(mem.memory); mem.memory = malloc(sizeof(char)); mem.size = 0;
            if (++failurecount == MAXTRIES)
                break;
            sleep(WAITTIME);
        }

        if (failurecount < MAXTRIES) {
            if (verbose) {
                printf(mem.memory);
                printf("\n");
            }
            *new_msgs = notify_New_Emails(mem.memory, URL);
            free(mem.memory); mem.memory = malloc(sizeof(char)); mem.size = 0;
        }
        else {
            failurecount = 0;
        }

        if (running)
            sleep(NOTIFICATION_SLEEP);
    }
    notify_uninit();

    CleanupCurl(&easyhandle);

    shmdt(new_msgs);
    shmctl(shmid, IPC_RMID, NULL);
}
