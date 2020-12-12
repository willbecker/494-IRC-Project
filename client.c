#include <arpa/inet.h>
#include "structs.h"
#define PORT 8888



int recieve_messages(int socket_fd)
{
    char message[MSG_LENGTH] = {0};
    char keep_alive_msg[MSG_LENGTH] = {0};
    struct timespec start, end, time_taken;
    struct timespec last_alive, time_since_last_alive;
    clock_gettime(CLOCK_MONOTONIC, &last_alive); 
    sprintf(keep_alive_msg, "%i,%i", KEEPALIVE, 0);

    while(1){
        clock_gettime(CLOCK_MONOTONIC, &start);

        // Check if we have timed out from the server
        time_since_last_alive.tv_sec = start.tv_sec - last_alive.tv_sec;
        if(time_since_last_alive.tv_sec >= TIMEOUT_LEN){
            printf("Connection to server timed out\n");
            return 0;
        }
        
        // Send keepalive message to the server
        write(socket_fd, keep_alive_msg, sizeof(keep_alive_msg));

        // Handle messages from the server
        int error = 0;
        bzero(message, sizeof(message));
        if(read(socket_fd, message, sizeof(message)) != -1 && message[0] != '\0'){
            switch((uint8_t)atoi(strtok(message, ","))){
                case KEEPALIVE :
                    clock_gettime(CLOCK_MONOTONIC, &last_alive); 
                    break;
                default :
                    printf("Recieved message with invalid type from server\n");
            }
            if(error){
                printf("Error: %i\n", error);
            }
        }

        // Maintain maximum update rate
        clock_gettime(CLOCK_MONOTONIC, &end);
        time_taken.tv_sec = end.tv_sec - start.tv_sec;
        time_taken.tv_nsec = end.tv_nsec - start.tv_nsec;
        struct timespec sleep_time = {0, UPDATE_INTERVAL * 1000000 - time_taken.tv_nsec};
        if(time_taken.tv_sec < UPDATE_INTERVAL)
            nanosleep(&sleep_time, NULL);
    }
}
void send_messages(int socket_fd)
{
    // setup name
    int n = 0;
    char message[MSG_LENGTH] = {0};
    char name[16] = {0};
    printf("Enter your name: ");
    while((name[n++] = getchar()) != '\n');
    struct message_header header = {SETNAME, 16};
    struct set_name name_msg;
    name_msg.header = header;
    strcpy(name_msg.name, name);
    serialize_set_name(name_msg, message);
    write(socket_fd, message, sizeof(message));
    
    // wait for user input
    while(1){
        n = 0;
        printf(": ");
        while((message[n++] = getchar()) != '\n');
    }
}

int main()
{
    struct sockaddr_in server_addr = {0};

    // Create socket and assingn it to a file descripter
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(socket_fd >= 0){
        printf("Successfully created socket\n");
    } else {
        printf("Failed to create socket\n");
        return 1;
    }


    // Setup IP address and port number.
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_addr.sin_port = htons(PORT);


    // Connect to the server
    if (connect(socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == 0 ) {
        printf("Connected to server\n");
    } else {
        printf("Failed to connect to server\n");
        return 1;
    }

    // Set the non-blocking flag for socket_fd
    int flags = fcntl(socket_fd, F_GETFL, 0);
    flags |= O_NONBLOCK;
    fcntl(socket_fd, F_SETFL, flags);

    // Prevent broken pipe from crashing the program
    sigaction(SIGPIPE, &(struct sigaction){SIG_IGN}, NULL);

    pid_t pid = fork();
    if(pid < 0){
        printf("Failed to fork");
        return 1;
    } else if (pid > 0) {
        // we are in the parent process
        return recieve_messages(socket_fd);
    } else {
        // we are in the child process
        send_messages(socket_fd);
    }
}
