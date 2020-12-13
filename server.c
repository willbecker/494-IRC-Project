#include <netinet/in.h>
#include <sys/types.h>
#include <errno.h>
#include "structs.h"
#define PORT 8888
#define BACKLOG_SIZE 20
#define CLIENT_MAX 10

struct user {
    int id;
    char name[16];
    int fd;
    struct timespec last_alive;
};

struct room {
    char name[16];
    struct user *users[CLIENT_MAX];
};

int check_title(char *title) {
    if(strcmp(title, "(null)") == 0)
        return -1;
    int len = strlen(title);
    int is_alphanum = 1;
    if(0 < len < 16){
        for(int i = 0; i < 16; i++){
            if(!(48 <= (int)title[i] <= 122)) {
                is_alphanum = 0;
                break;
            }
        }
        if(is_alphanum)
            return 0;
    }
    return -1;
}

int keep_alive(struct user *client) {
    char *len = strtok(NULL, ",");
    if(len == NULL){
        return WRONG_LENGTH;
    } else if(atoi(len) == 0) {
        clock_gettime(CLOCK_MONOTONIC, &client->last_alive);
        return 0;
    }
    return WRONG_LENGTH;
}

int set_name(struct user *client) {
    char *len = strtok(NULL, ",");
    char *name = strtok(NULL, ",");
    uint32_t length;
    if((length = atoi(len)) != 16){
        return WRONG_LENGTH;
    } else if(check_title(name) != 0){
        return BAD_NAME;
    }

    strcpy(client->name, name);
    return 0;
}    

int set_nonblock(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    flags |= O_NONBLOCK;
    fcntl(fd, F_SETFL, flags);
    return 0;
}
int find_free_slot(struct user* user_list)
{
    int slot = 0;
    while(slot < CLIENT_MAX){
        if(user_list[slot].id == 0){
            return slot;
        }
        slot++;
    }
    return -1;
}
int find_free_pointer_slot(struct user **user_list)
{
    int slot = 0;
    while(slot < CLIENT_MAX){
        if(user_list[slot] == NULL){
            return slot;
        }
        slot++;
    }
    return -1;
}
struct room * find_free_room(struct room *room_list)
{
    int slot = 0;
    while(slot < CLIENT_MAX){
        if(room_list[slot].name[0] == '\0'){
            return &room_list[slot];
        }
        slot++;
    }
    return NULL;
}
struct room * find_room(struct room *room_list, char *name)
{
    int slot = 0;
    while(slot < CLIENT_MAX){
        if(strcmp(room_list[slot].name, name) == 0){
            return &room_list[slot];
        }
        slot++;
    }
    return NULL;
}


int accept_connections(int socket_fd, struct user* user_list)
{
    while(1){
        int slot;
        struct sockaddr_in client;
        int client_len = sizeof(client);
        int connection_fd = (accept(socket_fd, (struct sockaddr*)&client, &client_len));
        if(connection_fd == -1){
            if(errno == EAGAIN || EWOULDBLOCK)
                return -1;
            printf("Failded to accept new connetion\n");
            return -1;
        }

        if((slot = find_free_slot(user_list)) == -1){
            printf("Could not find open slot\n");
            return -1;
        }

        set_nonblock(connection_fd);
        user_list[slot].id = slot+1;
        user_list[slot].fd = connection_fd;
        clock_gettime(CLOCK_MONOTONIC, &user_list[slot].last_alive);
        strcpy(user_list[slot].name, "");
        printf("User connected on slot %i\n", slot);
    }
    return 0;
    
}

int send_admin_msg(struct user *client, char *msg)
{
    char packet[PACKET_LENGTH];
    sprintf(packet, "%i,%i,%s,%s,%s", SEND_MSG, MSG_LENGTH+32, 
            "server", "system", msg);
    write(client->fd, packet, sizeof(packet));
}

int send_msg(struct user *sender, struct room *room, char *msg)
{
    char packet[PACKET_LENGTH];
    sprintf(packet, "%i,%i,%s,%s,%s", SEND_MSG, MSG_LENGTH+32, 
            sender->name, room->name, msg);
    for(int i = 0; i < CLIENT_MAX; i++){
        if(room->users[i] != NULL && room->users[i]->id > 0){
            write(room->users[i]->fd, packet, sizeof(packet));
        }
    }
    
}

int join_room(struct user *client, struct room *room_list){
    char message[MSG_LENGTH];
    char *len = strtok(NULL, ",");
    char *room_name = strtok(NULL, ",");
    int length;
    struct room *room;
    if((length = atoi(len)) != 16){
        return WRONG_LENGTH;
    } else if(check_title(room_name) == -1){
        return BAD_NAME;
    } else if((room = find_room(room_list, room_name)) != NULL){
        room->users[find_free_pointer_slot(room->users)] = client; 
        sprintf(message, "Joined room %s\n", room_name);
        send_admin_msg(client, message);
        return 0;
    } else if((room = find_free_room(room_list)) != NULL){
        strcpy(room->name, room_name);
        room->users[find_free_pointer_slot(room->users)] = client; 
        sprintf(message, "%s not found. Created new room %s\n", room_name, room_name);
        send_admin_msg(client, message);
        sprintf(message, "Joined room %s\n", room_name);
        send_admin_msg(client, message);
        return 0;
    } else {
        sprintf(message, "%s not found. Room limit reached\n", room_name);
        send_admin_msg(client, message);
        return 0;
    }
}

int upload_msg(struct user *client, struct room *room_list){
    char message[MSG_LENGTH];
    char *len = strtok(NULL, ",");
    char *room_name = strtok(NULL, ",");
    char *msg = strtok(NULL , ",");
    int length;
    struct room *room;
    if((length = atoi(len)) != MSG_LENGTH+16){
        return WRONG_LENGTH;
    } else if(check_title(room_name) == -1){
        return BAD_NAME;
    } else if((room = find_room(room_list, room_name)) != NULL){
        send_msg(client, room, msg);
        return 0;
    } else {
        sprintf(message, "Room %s not found. Cannot send message\n", room_name);
        send_admin_msg(client, message);
        return 0;
    }
}

int disconnect(struct user *client, uint8_t error)
{
    char packet[PACKET_LENGTH];
    sprintf(packet, "%i,%i,%i", DISCONNECT, 1, error);
    write(client->fd, packet, sizeof(packet));
    printf("User disconnected on slot %i\n", client->id - 1);
    close(client->fd);
    client->fd = 0;
    client->id = 0;
    strcpy(client->name, "");
}

int main()
{
    struct sockaddr_in server_addr = {0};
    struct user user_list[CLIENT_MAX] = {0};
    struct room room_list[CLIENT_MAX] = {0};
    struct timespec start, end, time_taken;
    struct timespec time_since_last_alive;
    char packet[PACKET_LENGTH];

    // Create socket and assingn it to a file descripter
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(socket_fd >= 0){
        printf("Successfully created socket\n");
    } else {
        printf("Failed to create socket\n");
        return 1;
    }

    // Set the non-blocking flag for socket_fd
    set_nonblock(socket_fd);

    // Prevent broken pipe from crashing the program
    sigaction(SIGPIPE, &(struct sigaction){SIG_IGN}, NULL);

    // Setup IP address and port number.
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(PORT);

    // Bind the socket to the IP address and port
    if (bind(socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == 0) {
        printf("successfully bound socket\n");
    } else {
        printf("failed to bind socket\n");
        return 1;
    }

    // Put the server into listen mode
    if (listen(socket_fd, BACKLOG_SIZE) == 0) {
        printf("Server started listening\n");
    } else {
        printf("Server Failed to start listening\n");
        return 1;
    }

    while(1){
        clock_gettime(CLOCK_MONOTONIC, &start); 
        bzero(packet, sizeof(packet));

        // Handle any new incoming connections if they exsist
        struct pollfd fds[1] = {socket_fd, POLLIN, 0};
        if (poll(fds, 1, 0) > 0){
            accept_connections(socket_fd, user_list);
        }

        // Send keep alive packet to each conneted client
        for(int i = 0; i < CLIENT_MAX; i++){
            if(user_list[i].id != 0){
                sprintf(packet, "%i,%i", KEEPALIVE, 0);
                write(user_list[i].fd, packet, sizeof(packet));
            }
        }

        // Disconnect any client that has not sent a recent keep 
        // alive massage to the server.
        for(int i = 0; i < CLIENT_MAX; i++){
            if(user_list[i].id != 0){
                time_since_last_alive.tv_sec = start.tv_sec 
                                             - user_list[i].last_alive.tv_sec;
                if(time_since_last_alive.tv_sec >= TIMEOUT_LEN){
                    disconnect(&(user_list[i]), TIMEOUT);
                }
            }
        }

        // Handle packets from all clients
        int error = 0;
        bzero(packet, sizeof(packet));
        for(int i = 0; i < CLIENT_MAX; i++){
            if(user_list[i].id == i+1){
                if(read(user_list[i].fd, packet, sizeof(packet)) != -1
                   && packet[0] != '\0'){
                    switch((uint8_t)atoi(strtok(packet, ","))){
                        case KEEPALIVE :
                            error = keep_alive(&user_list[i]);
                            break;
                        case DISCONNECT :
                            error = USER_QUIT;
                            break;
                        case SETNAME :
                            error = set_name(&user_list[i]);
                            break;
                        case JOIN_ROOM :
                            error = join_room(&user_list[i], room_list);
                            break;
                        case UPLOAD_MSG :
                            error = upload_msg(&user_list[i], room_list);
                            break;
                        default :
                            printf("Recieved packet with invalid type from client %i\n", i+1);
                    }
                    if(error){
                        disconnect(&user_list[i], error);
                    }
                }
            }
        }
        
        clock_gettime(CLOCK_MONOTONIC, &end);
        time_taken.tv_sec = end.tv_sec - start.tv_sec;
        time_taken.tv_nsec = end.tv_nsec - start.tv_nsec;
        struct timespec sleep_time = {0, UPDATE_INTERVAL * 1000000 - time_taken.tv_nsec};
        if(time_taken.tv_sec < UPDATE_INTERVAL)
            nanosleep(&sleep_time, NULL);
    }

}
