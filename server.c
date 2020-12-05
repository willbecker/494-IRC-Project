#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <time.h>
#include <errno.h>
#define PORT 8888
#define BACKLOG_SIZE 20
#define CLIENT_MAX 10
#define UPDATE_INTERVAL 1000 //number of milliseconds between each update
#define TIMEOUT 5

struct user {
    int id;
    char name[16];
    int fd;
    struct timespec last_alive;
};

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

int disconnect(int slot, struct user* user_list)
{
    user_list[slot].id = 0;
    user_list[slot].fd = 0;
    strcpy(user_list[slot].name, "");
    printf("User disconnected on slot %i\n", slot);
}

int main()
{
    struct sockaddr_in server_addr = {0};
    struct user user_list[CLIENT_MAX] = {0};

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
        struct timespec start, end, time_taken;
        clock_gettime(CLOCK_MONOTONIC, &start); 
        char message[16];

        // Handle any new incoming connections if they exsist
        struct pollfd fds[1] = {socket_fd, POLLIN, 0};
        if (poll(fds, 1, 0) > 0){
            accept_connections(socket_fd, user_list);
        }

        // Time out client if it sent a recent keep alive request
        for(int i = 0; i < CLIENT_MAX; i++){
            if(user_list[i].id != 0){
                struct timespec time_since_last_alive = {start.tv_sec - user_list[i].last_alive.tv_sec, 0};
                if(time_since_last_alive.tv_sec >= TIMEOUT){
                    disconnect(i, user_list);
                }
            }
        }

        for(int i = 0; i < CLIENT_MAX; i++){
            if(user_list[i].id == i+1){
                if(read(user_list[i].fd, message, sizeof(message)) != -1){
                    printf("%s", message);
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
