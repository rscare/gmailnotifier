#include "rpass.h"
#include "gmailxml.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <curl/curl.h>
#include <gpgme.h>
#include <time.h>
#include <libnotify/notify.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <libxml/parser.h>

#define URL "https://mail.google.com/mail/feed/atom"
#define ACNAME "Primary Gmail"
#define PASSFILE "/home/redscare/.passwords.gpg"
#define WAITTIME 7 // How long to wait between tries
#define MAXTRIES 7
#define NOTIFICATION_SLEEP 180 // How long to wait between notifications
#define APPNAME "rgmailnotifier"

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

int GetUSERPWD(char **str, const char * const acname) {
    gpgme_data_t ptext = NULL;
    rpass_parent *rparent = NULL;
    rpass_entry *uentry = NULL, *pentry = NULL;
    MemoryStruct *mem;

    gpgme_data_new(&ptext);
    initialize_engine();

    decrypt_file(PASSFILE, ptext);
    GetAccountInfo(ptext, acname, &rparent);

    pentry = uentry = rparent->first_entry;
    while (strcmp(uentry->key, "user") != 0)
        uentry = uentry->next_entry;
    while (strcmp(pentry->key, "pass") != 0)
        pentry = pentry->next_entry;

    *str = (char *)malloc(strlen(uentry->value) + strlen(pentry->value) + 2);
    sprintf(*str, "%s:%s", uentry->value, pentry->value);

    destroy_rpass_parent(rparent);
    destroy_engine();
    gpgme_data_release(ptext);

    return 0;
}

void SetupCurl(CURL **handle) {
    char *userpwd = NULL;

    curl_global_init(CURL_GLOBAL_ALL);

    *handle = curl_easy_init();

    curl_easy_setopt(*handle, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(*handle, CURLOPT_URL, URL);

    GetUSERPWD(&userpwd, ACNAME);
    curl_easy_setopt(*handle, CURLOPT_USERPWD, userpwd);
    free(userpwd);

    curl_easy_setopt(*handle, CURLOPT_WRITEFUNCTION, &WriteMemoryCallback);
}

void CleanupCurl(CURL **handle) {
    curl_easy_cleanup(*handle);
    curl_global_cleanup();
}

void daemonize () {
    if (fork() < 0)
        exit(1);
    else
        exit(0);

    if (setsid() < 0)
        exit(1);

    if (chdir("/") < 0)
        exit(1);

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
}

int main(int argc, char *argv[])
{
    CURL *easyhandle = NULL;
    unsigned short int failurecount = 0;
    time_t last_update = time(NULL);
    MemoryStruct mem;

    daemonize();

    mem.memory = malloc(1);
    mem.size = 0;

    SetupSignals();
    SetupSHM();
    SetupCurl(&easyhandle);

    curl_easy_setopt(easyhandle, CURLOPT_WRITEDATA, &mem);

    notify_init(APPNAME);
    while (running) {
        while (curl_easy_perform(easyhandle) != 0) {
            free(mem.memory); mem.memory = malloc(1); mem.size = 0;
            if (++failurecount == MAXTRIES)
                break;
            sleep(WAITTIME);
        }

        if (failurecount < MAXTRIES) {
            *new_msgs = notify_New_Emails(mem.memory, URL, last_update);
            last_update = time(NULL);
            free(mem.memory); mem.memory = malloc(1); mem.size = 0;
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

    return 0;
}
