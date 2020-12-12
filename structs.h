#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#define MSG_LENGTH 50
#define UPDATE_INTERVAL 1000 //number of milliseconds between each update
#define TIMEOUT_LEN 5

enum msg_type {
    SETNAME = 0x11,
    DISCONNECT = 0x12,
    KEEPALIVE = 0x13,
    JOIN_ROOM = 0x14,
    LEAVE_ROOM = 0x15,
    UPLOAD_MSG = 0x16,
    SEND_MSG = 0x17,
    REQUEST_ROOMS = 0x18,
    REQUEST_MEMBERS = 0x19,
    LIST = 0x1A
};

enum error {
    BAD_NAME = 0xE1,
    NAME_INUSE = 0xE2,
    BAD_MESSAGE = 0xE3,
    USER_LIMIT = 0xE4,
    ROOM_LIMIT = 0xE5,
    WRONG_TYPE = 0xE6,
    WRONG_LENGTH = 0xE7,
    TIMEOUT = 0xE8
};

struct message_header {
    uint8_t type;
    uint32_t length;
};

struct set_name {
    struct message_header header;
    char name[16];
};

int serialize_set_name(struct set_name in, char* out);
