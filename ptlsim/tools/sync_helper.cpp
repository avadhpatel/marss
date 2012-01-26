/*
 * sync_helper.cpp : A small helper tool for Marss's -sync option
 *
 * This small tool is aimed to help Marss users in -sync option by
 * providing options to manipulate semaphore used for syncing between
 * simulation instances.  Available options are:
 *
 *    delete   :  Delete the semaphore
 *    set N    :  Set semaphore value to N
 *
 * To compile:
 *    $ g++ sync_helper.cpp -o sync_helper
 */


#include <iostream>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

using namespace std;

#define SEM_NUM 3764

void info(int sem_id)
{
    int rc;

    rc = semctl(sem_id, 0, GETVAL);
    cout << "Semaphore value: " << rc << endl;

    rc = semctl(sem_id, 0, GETZCNT);
    cout << "Processes waiting for zero: " << rc << endl;
}

void remove(int sem_id)
{
    int rc;

    rc = semctl(sem_id, 0, IPC_RMID);

    if (rc != 0) {
        cout << "Unable to delete semaphore: ";
        perror("sem_id");
        return;
    }

    cout << "Semaphore removed." << endl;
}

void set(int sem_id, int val)
{
    int rc;

    rc = semctl(sem_id, 0, SETVAL, val);

    if (rc != 0) {
        cout << "Unable to set semaphore value: ";
        perror("sem_id");
        return;
    }

    cout << "Semaphore value set to " << val << endl;
}

int main(int argc, char** argv)
{
    char *env_sem_id_p;
    int sem_num;
    int sem_id;
    int val;
    int rc;

    /* First check if semaphore exists or not */

    env_sem_id_p = getenv("MARSS_SEM_ID");

    if (env_sem_id_p)
        sem_num = atoi(env_sem_id_p);
    else
        sem_num = SEM_NUM;

    sem_id = semget(sem_num, 1, IPC_CREAT|IPC_EXCL|0666);

    if (sem_id == -1) {
        if (errno == EEXIST) {
            sem_id = semget(sem_num, 1, IPC_CREAT|0666);
        }

        if (sem_id == -1) {
            cout << "Unable to access semaphore.\n";
            perror("sem_id");
            exit(0);
        }
    }

    info(sem_id);

    if (argc < 2)
        return 0;

    if (strcmp("delete", argv[1]) == 0) {
        remove(sem_id);
    } else if (strcmp("set", argv[1]) == 0) {

        if (argc < 3) {
            cout << "Please specify the value to set.\n";
            return -1;
        }

        val = atoi(argv[2]);
        set(sem_id, val);
    }

    return 0;
}
