#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <fcntl.h>
#include "pa2345.h"
#include "ipc_struct.h"
#include "common.h"

void closePipes(struct Ipc *ipc) {
    local_id processAmount = ipc[0].n;
    for (local_id i = 0; i < processAmount; i++) {
        local_id currentId = ipc[i].id;
        for (local_id j = 0; j < processAmount; j++) {
            if (i != currentId) {
                if (ipc[i].pipes[j].in > 0) {
                    close(ipc[i].pipes[j].in);
                }
                if (ipc[i].pipes[j].out > 0) {
                    close(ipc[i].pipes[j].out);
                }
            }
        }
    }
}

void closeProcessPipes(struct Ipc *ipc) {
    local_id processAmount = ipc->n;
    for (local_id i = 0; i < processAmount; i++) {
        if (i != ipc->id) {
            if (ipc->pipes[i].in > 0) {
                close(ipc->pipes[i].in);
            }
            if (ipc->pipes[i].out > 0) {
                close(ipc->pipes[i].out);
            }
        }
    }
}

int request_cs(const void * self) {
    struct Ipc *ipc = (struct Ipc *) self;

    Message messageRequest;
    messageRequest.s_header.s_magic = MESSAGE_MAGIC;
    messageRequest.s_header.s_type = CS_REQUEST;
    messageRequest.s_header.s_payload_len = 0;
    send_multicast(ipc, &messageRequest);

    queuePush(&(ipc->queue), ipc->id, get_lamport_time(ipc));
    ipc->types.replies = 0;

    local_id processAmountForReplies = ipc->n - 2;
    while (ipc->types.replies < processAmountForReplies || queuePeek(&(ipc->queue)) != ipc->id) { // Changed second false
        Message messageReplies;
        local_id src = receive_any(ipc, &messageReplies);

        switch (messageReplies.s_header.s_type) {
//            case STARTED: {
//                ipc->types.started += 1;
//                break;
//            }
//            case DONE: {
//                ipc->types.done += 1;
//                break;
//            }
            case CS_REQUEST: {
                queuePush(&(ipc->queue), src, messageReplies.s_header.s_local_time);
                Message messageReply;
                messageReply.s_header.s_magic = MESSAGE_MAGIC;
                messageReply.s_header.s_type = CS_REPLY;
                messageReply.s_header.s_payload_len = 0;
                send(ipc, src, &messageReply);
                break;
            }
            case CS_REPLY: {
                ipc->types.replies += 1;
                break;
            }
            case CS_RELEASE: {
                queuePop(&(ipc->queue), src);
                break;
            }
        }
    }
   return 0;
}

int release_cs(const void * self) {
    struct Ipc *ipc = (struct Ipc *) self;

    queuePop(&(ipc->queue), ipc->id);
    Message messageRelease;
    messageRelease.s_header.s_magic = MESSAGE_MAGIC;
    messageRelease.s_header.s_type = CS_RELEASE;
    messageRelease.s_header.s_payload_len = 0;
    send_multicast(ipc, &messageRelease);

    return 0;
}

struct Ipc *runMainProcess(local_id n, bool useMutex) {
    struct Ipc *ipcs = malloc(n * sizeof(struct Ipc));

    FILE *pipesLogFile = fopen(pipes_log, "a");
    FILE *eventLogFile = fopen(events_log, "a");

    pid_t parentPid = getpid();

    for (local_id i = 0; i < n; i++) {
        ipcs[i].n = n;
        ipcs[i].id = i;
        ipcs[i].parentPid = parentPid;
        ipcs[i].currentLamportTime = 0;
        ipcs[i].queue.len = 0;
        ipcs[i].types.started = 0;
        ipcs[i].types.replies = 0;
        ipcs[i].types.done = 0;
        for (int j = 0; j < n; j++) {
            if (i != j) {
                int descriptors[2];
                if (pipe(descriptors) == -1) {
                    perror("Problem with pipe");
                    exit(EXIT_FAILURE);
                } else {
                    //Check later
                    int flag1 = fcntl(descriptors[0], F_GETFL);
                    if (flag1 == -1) {
                        perror("Problem with nonblock");
                        exit(EXIT_FAILURE);
                    }
                    if (fcntl(descriptors[0], F_SETFL, flag1 | O_NONBLOCK) == -1) {
                        perror("Problem with nonblock");
                        exit(EXIT_FAILURE);
                    }
                    int flag2 = fcntl(descriptors[1], F_GETFL);
                    if (flag2 == -1) {
                        perror("Problem with nonblock");
                        exit(EXIT_FAILURE);
                    }
                    if (fcntl(descriptors[1], F_SETFL, flag2 | O_NONBLOCK) == -1) {
                        perror("Problem with nonblock");
                        exit(EXIT_FAILURE);
                    }
                    //
                    ipcs[i].pipes[j].in = descriptors[0];
                    ipcs[j].pipes[i].out = descriptors[1];

                }
            }
        }
    }

    for (local_id i = 0; i < n; i++) {
        if (i != PARENT_ID) {
            ipcs[i].pid = fork();
            if (ipcs[i].pid == -1) {
                perror("Problem with fork");
                exit(EXIT_FAILURE);
            } else if (ipcs[i].pid == 0) {
                ipcs[i].pid = getpid();
                for (local_id k = 0; k < n; k++) {
                    if (i != k) {
                        for (local_id l = 0; l < n; l++) {
                            if (l != k) {
                                close(ipcs[k].pipes[l].in);
                                close(ipcs[k].pipes[l].out);
                            }
                        }
                    }
                }
                exit(!runChildProcess(eventLogFile, ipcs[i], useMutex));
            }
        } else {
            ipcs[PARENT_ID].pid = parentPid;
        }
    }


    fclose(pipesLogFile);
    fclose(eventLogFile);

    return ipcs;
} // +--

bool runChildProcess(FILE *logFile, struct Ipc ipc, bool useMutex) {

    printLog(logFile,
             log_started_fmt,
             get_lamport_time(&ipc), ipc.id, ipc.pid, ipc.parentPid, 0);

    Message multicastMessageStarted;
    createMessage(&multicastMessageStarted,
                  STARTED,
                  log_started_fmt,
                  get_lamport_time(&ipc), ipc.id, ipc.pid, ipc.parentPid, 0);                  
    send_multicast(&ipc, &multicastMessageStarted);
    
    local_id processAmountForStart = ipc.n - 2;
    while (ipc.types.started < processAmountForStart) {
        Message messageStarted;
        local_id src = receive_any(&ipc, &messageStarted);

        switch (messageStarted.s_header.s_type) {
            case STARTED: {
                ipc.types.started += 1;
                break;
            }
            case CS_REQUEST: {
                queuePush(&(ipc.queue), src, messageStarted.s_header.s_local_time);

                Message messageReply;
                messageReply.s_header.s_magic = MESSAGE_MAGIC;
                messageReply.s_header.s_type = CS_REPLY;
                messageReply.s_header.s_payload_len = 0;
                send(&ipc, src, &messageReply);
                break;
            }
            case CS_REPLY: {
                ipc.types.replies += 1;
                break;
            }
            case CS_RELEASE: {
                queuePop(&(ipc.queue), src);
                break;
            }
        }
    }

    printLog(logFile,
             log_received_all_started_fmt,
             get_lamport_time(&ipc), ipc.id);

    local_id printAmount = ipc.id * 5;
    for (local_id i = 0; i < printAmount; i++) {
        if (useMutex) {
            request_cs(&ipc);
        }
        char str[256];
        snprintf(str, 256, log_loop_operation_fmt, ipc.id, i + 1, printAmount);
        print(str);
        if (useMutex) {
            release_cs(&ipc);
        }
    }

    printLog(logFile,
             log_done_fmt,
             get_lamport_time(&ipc), ipc.id, 0);
    Message multicastMessageDone;
    createMessage(&multicastMessageDone,
                  DONE,
                  log_done_fmt,
                  get_lamport_time(&ipc), ipc.id, 0);
    send_multicast(&ipc, &multicastMessageDone);

    local_id processAmountForDone = ipc.n - 2;
    while (ipc.types.done < processAmountForDone) {
        Message messageDone;
        local_id src = receive_any(&ipc, &messageDone);

        switch (messageDone.s_header.s_type) {
//            case STARTED: {
//                ipc.types.started += 1;
//                break;
//            }
            case DONE: {
                ipc.types.done += 1;
                break;
            }
            case CS_REQUEST: {
                queuePush(&(ipc.queue), src, messageDone.s_header.s_local_time);
                Message messageReply;
                messageReply.s_header.s_magic = MESSAGE_MAGIC;
                messageReply.s_header.s_type = CS_REPLY;
                messageReply.s_header.s_payload_len = 0;
                send(&ipc, src, &messageReply);
                break;
            }
            case CS_REPLY: {
                ipc.types.replies += 1;
                break;
            }
            case CS_RELEASE: {
                queuePop(&(ipc.queue), src);
                break;
            }
        }
    }

    printLog(logFile,
             log_received_all_done_fmt,
             get_lamport_time(&ipc), ipc.id);

    return 0;
}

void createMessage(Message *message, MessageType type, const char *format, ...) {
    va_list ap;
    va_start(ap, format);

    message->s_header.s_magic = MESSAGE_MAGIC;
    message->s_header.s_type = type;
    message->s_header.s_payload_len = vsnprintf(message->s_payload, MAX_PAYLOAD_LEN, format, ap);

    va_end(ap);
}

timestamp_t get_lamport_time(struct Ipc *ipc) {
    return ipc->currentLamportTime;
}

void queuePush(struct MutexQueue *queue, local_id id, timestamp_t time) {
    local_id queueLength = queue->len;
    queue->buffer[queueLength].id = id;
    queue->buffer[queueLength].currentTime = time;
    queue->len += 1;
}

void queuePop(struct MutexQueue *queue, local_id id) {
    local_id minId = MAX_PROCESS_ID, minI = 0;
    timestamp_t minTime = INT16_MAX;

    local_id queueLength = queue->len;
    local_id i = 0;
    while (i < queueLength) {
        if (minTime < queue->buffer[i].currentTime) {
            continue;
        }
        if (minTime == queue->buffer[i].currentTime && minId < queue->buffer[i].id) {
            continue;
        }

        minTime = queue->buffer[i].currentTime;
        minId = queue->buffer[i].id;
        minI = i;
        i++;
    }

    local_id errorId = id;
    if (errorId != queue->buffer[minI].id) {
        queue->len -= 1;
        queue->buffer[minI].id = 0;
        queue->buffer[minI].currentTime = 0;
        //queue->buffer[minI] = queue->buffer[queue->len];
    }
}

local_id queuePeek(struct MutexQueue *queue) {
    local_id minId = MAX_PROCESS_ID, minI = 0;
    timestamp_t minTime = INT16_MAX;

    local_id queueLength = queue->len;
    local_id i = 0;
    while (i < queueLength) {
        if (minTime < queue->buffer[i].currentTime) {
            continue;
        }
        if (minTime == queue->buffer[i].currentTime && minId < queue->buffer[i].id) {
            continue;
        }
        if (queue->buffer[i].currentTime == 0) {
            continue;
        }

        minTime = queue->buffer[i].currentTime;
        minId = queue->buffer[i].id;
        minI = i;
        i++;
    }

    local_id res = queue->buffer[minI].id;
    return res;
}
