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

int check_title(char *title) {
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
    char keep_alive_msg[MSG_LENGTH] = {0};
    if(len == NULL){
        return WRONG_LENGTH;
    } else if(atoi(len) == 0) {
        sprintf(keep_alive_msg, "%i,%i", KEEPALIVE, 0);
        clock_gettime(CLOCK_MONOTONIC, &client->last_alive);
        write(client->fd, keep_alive_msg, sizeof(keep_alive_msg));
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

int disconnect(struct user *user_list, uint8_t error)
{
    printf("User disconnected on slot %i\n", user_list->id - 1);
    user_list->id = 0;
    user_list->fd = 0;
    strcpy(user_list->name, "");
}

int main()
{
    struct sockaddr_in server_addr = {0};
    struct user user_list[CLIENT_MAX] = {0};
    struct timespec start, end, time_taken;
    struct timespec time_since_last_alive;
    char message[MSG_LENGTH];

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

        // Handle any new incoming connections if they exsist
        struct pollfd fds[1] = {socket_fd, POLLIN, 0};
        if (poll(fds, 1, 0) > 0){
            accept_connections(socket_fd, user_list);
        }


        // Disconnect any client that has not sent a recent keep 
        // alive massage to the server.
        for(int i = 0; i < CLIENT_MAX; i++){
            if(user_list[i].id != 0){
                time_since_last_alive.tv_sec = start.tv_sec 
                                             - user_list[i].last_alive.tv_sec;
                if(time_since_last_alive.tv_sec >= TIMEOUT_LEN){
                //    disconnect(&(user_list[i]), TIMEOUT);
                }
            }
        }

        // Handle messages from all clients
        int error = 0;
        for(int i = 0; i < CLIENT_MAX; i++){
            if(user_list[i].id == i+1){
                if(read(user_list[i].fd, message, sizeof(message)) != -1
                   && message != NULL){
                    switch((uint8_t)atoi(strtok(message, ","))){
                        case KEEPALIVE :
                            error = keep_alive(&user_list[i]);
                            break;
                        case SETNAME :
                            error = set_name(&user_list[i]);
                            break;
                        default :
                            printf("Recieved message with invalid type from client %i\n", i+1);
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
