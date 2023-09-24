#pragma once

#include <stddef.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include "banking.h"
#include "pa2345.h"
#include "ipc.h"

enum MutexState {
    WAIT,
    FREE,
    BUSY
};

struct Pipe {
    int in;
    int out;
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
    timestamp_t currentTime;
    timestamp_t currentLamportTime;
    enum MutexState state;
    bool defReply[MAX_PROCESS_ID + 1];
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
void handleMessage(struct Ipc *ipc);
void createMessageWithSType(Message *message, MessageType type);
void sendMulticastMessageWithSType(struct Ipc *ipc, MessageType type);
int old_send(void * self, local_id dst, const Message * msg);
