#pragma once

#include <stddef.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include "banking.h"
#include "pa2345.h"
#include "ipc.h"

struct Pipe {
    int in;
    int out;
};

struct MutexQueue {
    struct {
        local_id id;
        timestamp_t currentTime;
    } buffer[MAX_PROCESS_ID + 1];

    local_id len;
};

struct AmountTypes {
    int started;
    int replies;
    int done;
};

struct Ipc {
    local_id n;
    local_id id;
    pid_t pid;
    pid_t parentPid;
    timestamp_t currentLamportTime;
    struct MutexQueue queue;
    struct AmountTypes types;
    struct Pipe pipes[MAX_PROCESS_ID + 1];
};

struct Ipc *runMainProcess(local_id n, bool useMutex);
bool runChildProcess(FILE *logFile, struct Ipc ipc, bool useMutex);
int receiveBlocking(void * self, local_id id, Message * msg);
void receiveAnyBlocking(void * self, local_id n);
void closePipes(struct Ipc *ipc);
void createMessage(Message *message, MessageType type, const char *format, ...);

void printLog(FILE *logFile, const char *format, ...);
void transfer(void * parent_data, local_id src, local_id dst,
              balance_t amount);

timestamp_t get_lamport_time(struct Ipc *ipc);
int old_send(void * self, local_id dst, const Message * msg);
void queuePush(struct MutexQueue *queue, local_id id, timestamp_t time);
void queuePop(struct MutexQueue *queue, local_id id);
local_id queuePeek(struct MutexQueue *queue);
