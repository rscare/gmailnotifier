#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdio.h>

#define SHM_KEY "/home/redscare/.rpasswords"

int main(int argc, char *argv[])
{
    unsigned short int *msgs;
    key_t shmkey = ftok(SHM_KEY, 'G');
    int shmid = shmget(shmkey, sizeof(*msgs), 0644|IPC_CREAT);

    msgs = shmat(shmid, 0, SHM_RDONLY);

    printf("%d\n", *msgs);
    shmdt(msgs);

    return 0;
}

