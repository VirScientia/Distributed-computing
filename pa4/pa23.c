#include <stdbool.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <string.h>
#include <unistd.h>
#include "ipc_struct.h"
#include "banking.h"

int main(int argc, char * argv[]) {
    local_id n;
    bool useMutex = false;

    if (argc < 3) {
        exit(EXIT_FAILURE);
    } else {
        n = (local_id) (atoi(argv[2]) + 1);
        for (int i = 0; i < argc; i++) {
            if (strcmp("-p", argv[i]) == 0) {
                continue;
            }
            if (strcmp("--mutexl", argv[i]) == 0) {
                useMutex = true;
            }
        }
        
    }

    struct Ipc *ipcs = runMainProcess(n, useMutex);

    for (local_id k = 0; k < n; k++) {
        if (PARENT_ID != k) {
            for (local_id l = 0; l < n; l++) {
                if (l != k) {
                    close(ipcs[k].pipes[l].in);
                    close(ipcs[k].pipes[l].out);
                }
            }
        }
    }

    struct Ipc *ipcParent = (&ipcs)[PARENT_ID]; //BAD VERY BAD AND DONT FORGET CLOSE PIPES!!!

    Message messageStarted;
    for (local_id i = 1; i < n; i++) {
        if (ipcs[i].id != i) {
            receive(ipcParent, i, &messageStarted);
        }
    }

    Message messageDone;
    for (local_id i = 1; i < n; i++) {
        if (ipcs[i].id != i) {
            receive(ipcParent, i, &messageDone);
        }
    }

//    local_id started = 0;
//    local_id done = 0;
//    while (started < n - 1 || done < n - 1) {
//        Message messageParent;
//        receive_any(ipcParent, &messageParent);
//        switch (messageParent.s_header.s_type) {
//            case STARTED: {
//                started += 1;
//                break;
//            }
//            case DONE: {
//                done += 1;
//                break;
//            }
//        }
//    }

    local_id i = 0;
    while (i < n) {
        wait(&ipcs[i].pid);
        i++;
    }

    closePipes(ipcs);
    free(ipcs);

    return 0;
}
