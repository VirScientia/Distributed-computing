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
        int nIndex = 0;
        for (int i = 0; i < argc; i++) {
            if (strcmp("-p", argv[i]) == 0) {
                nIndex = i + 1;
            }
            if (strcmp("--mutexl", argv[i]) == 0) {
                useMutex = true;
            }
        }
        n = (local_id) (atoi(argv[nIndex]) + 1);
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

    struct Ipc *ipcParent = (&ipcs)[PARENT_ID];

    local_id started = 0;
    local_id done = 0;
    do {
        Message messageParent;
        receive_any(ipcParent, &messageParent);
        if (messageParent.s_header.s_magic == MESSAGE_MAGIC) {
            if (messageParent.s_header.s_type == STARTED) {
                started += 1;
            } else if (messageParent.s_header.s_type == DONE) {
                done += 1;
            }
        }
    } while (started < n - 1 || done < n - 1);

    local_id i = 0;
    while (i < n) {
        wait(&ipcs[i].pid);
        i++;
    }

    closePipes(ipcs);
    free(ipcs);

    return 0;
}
