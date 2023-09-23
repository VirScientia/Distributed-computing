#include <unistd.h>
#include <errno.h>
#include <string.h>
#include "ipc.h"
#include "ipc_struct.h"

int send(void * self, local_id dst, const Message * msg) {
    struct Ipc *ipc = (struct Ipc *) self;
    Message newMessage;

    ipc->currentLamportTime += 1;
    newMessage.s_header = msg->s_header;
    newMessage.s_header.s_local_time = ipc->currentLamportTime;
    memcpy(newMessage.s_payload, msg->s_payload, msg->s_header.s_payload_len);

    local_id currentId = ipc->id;
    if (dst != currentId) {
        size_t messageSize;
        size_t messageSize1 = sizeof(Message) + newMessage.s_header.s_payload_len - MAX_PAYLOAD_LEN;
        size_t messageSize2 = sizeof(MessageHeader) + newMessage.s_header.s_payload_len;
        if (messageSize1 > messageSize2) {
            messageSize = messageSize1;
        } else {
            messageSize = messageSize2;
        }
        ssize_t sizeWrite;
        sizeWrite = write(ipc->pipes[dst].out, &newMessage, messageSize);
        if (messageSize != sizeWrite) {
            return -1;
        }
    }

    //printf("Send - id = %d, time = %d, type = %d\n", ipc->id, ipc->currentLamportTime, newMessage->s_header.s_type);

    return 0;
}

int old_send(void * self, local_id dst, const Message * msg) {
    struct Ipc *ipc = (struct Ipc *) self;

    local_id currentId = ipc->id;
    if (dst != currentId) {
        size_t messageSize;
        size_t messageSize1 = sizeof(Message) + msg->s_header.s_payload_len - MAX_PAYLOAD_LEN;
        size_t messageSize2 = sizeof(MessageHeader) + msg->s_header.s_payload_len;
        if (messageSize1 > messageSize2) {
            messageSize = messageSize1;
        } else {
            messageSize = messageSize2;
        }
        ssize_t sizeWrite;
        sizeWrite = write(ipc->pipes[dst].out, msg, messageSize);
        if (messageSize != sizeWrite) {
            return -1;
        }
    }

    //printf("Send - id = %d, time = %d, type = %d\n", ipc->id, ipc->currentLamportTime, newMessage->s_header.s_type);

    return 0;
}

int send_multicast(void * self, const Message * msg) {
    struct Ipc *ipc = (struct Ipc *) self;
    Message newMessage;

    ipc->currentLamportTime += 1;
    newMessage.s_header = msg->s_header;
    newMessage.s_header.s_local_time = ipc->currentLamportTime;
    memcpy(newMessage.s_payload, msg->s_payload, msg->s_header.s_payload_len);

    local_id processAmount = ipc->n;
    local_id currentId = ipc->id;
    for (local_id i = 0; i < processAmount; i++) {
        if (i != currentId) {
            if (old_send(ipc, i, &newMessage) == 0) {
                continue;
            } else {
                return -1;
            }
        }
    }

    return 0;
}

int receive(void * self, local_id from, Message * msg) {
    struct Ipc *ipc = (struct Ipc *) self;

    local_id currentId = ipc->id;
    if (from != currentId) {
        size_t headerSize;
        size_t headerSize1 = sizeof(msg->s_header);
        size_t headerSize2 = sizeof(MessageHeader);
        if (headerSize1 > headerSize2) {
            headerSize = headerSize1;
        } else {
            headerSize = headerSize2;
        }
        size_t sizeRead = read(ipc->pipes[from].in, &msg->s_header, headerSize);
        if (headerSize != sizeRead) {
            if (sizeRead == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    return EAGAIN;
                }
            }
            return -1;
        }
    }

    if (from != currentId) {
        size_t otherSize = msg->s_header.s_payload_len;
        size_t sizeRead;
        sizeRead = read(ipc->pipes[from].in, msg->s_payload, otherSize);
        if (otherSize != sizeRead) {
            return -2;
        }
    }

    if (msg->s_header.s_local_time > ipc->currentLamportTime) {
        ipc->currentLamportTime = msg->s_header.s_local_time;
    }

    ipc->currentLamportTime += 1;

    //printf("Receive - id = %d, time = %d\n", ipc->id, ipc->currentLamportTime);

    return 0;
}

int receive_any(void * self, Message * msg) {
    struct Ipc *ipc = (struct Ipc *) self;

    local_id processAmount = ipc->n;
    while (true) {
        for (local_id i = 0; i < processAmount; i++) {
            local_id currentId = ipc->id;
            if (i != currentId) {
                int status = receive(ipc, i, msg);
                switch (status)
                {
                    case EAGAIN:
                        continue;
                    case 0:
                        return 0;
                    default:
                        return status;
                }
            }
        }
    }
}
