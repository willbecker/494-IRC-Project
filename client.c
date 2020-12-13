#include <arpa/inet.h>
#include "structs.h"
#define PORT 8888

int disconnect(){
    char *len = strtok(NULL, ",");
    char *error = strtok(NULL, ",");
    uint32_t length;
    if((length = atoi(len)) != 1){
        return WRONG_LENGTH;
    } else if(error[0] != '\0'){
        printf("Disconnected from server reason: ");
        switch(atoi(error)){
            case BAD_NAME :
                printf("BAD_NAME");
                break;
            case NAME_INUSE :
                printf("NAME_INUSE");
                break;
            case BAD_MESSAGE :
                printf("BAD_MESSAGE");
                break;
            case USER_LIMIT :
                printf("USER_LIMIT");
                break;
            case ROOM_LIMIT :
                printf("ROOM_LIMIT");
                break;
            case WRONG_TYPE :
                printf("WRONG_TYPE");
                break;
            case WRONG_LENGTH :
                printf("WRONG_LENGTH");
                break;
            case TIMEOUT :
                printf("TIMEOUT");
                break;
            case USER_QUIT :
                printf("USER_QUIT");
                break;
            default :
                printf("UNKNOWN");
        }
        printf("\n");
        return 0;
    }
}

int send_msg(){
    char *len = strtok(NULL, ",");
    char *sender = strtok(NULL, ",");
    char *room_name = strtok(NULL, ",");
    char *msg = strtok(NULL, ",");
    if(len == NULL){
        return WRONG_LENGTH;
    } else if(atoi(len) != MSG_LENGTH + 32) {
        return WRONG_LENGTH;
    } else if(sender[0] == '\0' || room_name[0] == '\0' || msg[0] == '\0'){
        return WRONG_TYPE;
    } else {
        printf("[%s] (%s): %s\n", room_name, sender, msg);
        return 0;
    }
}

int recieve_packets(int socket_fd)
{
    char packet[PACKET_LENGTH] = {0};
    char keep_alive_msg[PACKET_LENGTH] = {0};
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
        
        // Send keepalive packet to the server
        write(socket_fd, keep_alive_msg, sizeof(keep_alive_msg));

        // Handle packets from the server
        int error = 0;
        bzero(packet, sizeof(packet));
        if(read(socket_fd, packet, sizeof(packet)) != -1 && packet[0] != '\0'){
            switch((uint8_t)atoi(strtok(packet, ","))){
                case KEEPALIVE :
                    clock_gettime(CLOCK_MONOTONIC, &last_alive); 
                    break;
                case DISCONNECT :
                    if((error = disconnect()) == 0)
                        return 0;
                    break;
                case SEND_MSG :
                    error = send_msg();
                    break;
                default :
                    printf("Recieved packet with invalid type from server\n");
            }
            if(error){
                printf("Error with recieved packet: %x\n", error);
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
void send_packets(int socket_fd)
{
    // setup name
    int n = 0;
    char packet[PACKET_LENGTH] = {0};
    char name[16] = {0};
    char input[MSG_LENGTH];
    char command[MSG_LENGTH];
    printf("Enter your name: ");
    while((name[n++] = getchar()) != '\n');
    name[strlen(name) - 1] = '\0';
    struct packet_header header = {SETNAME, 16};
    struct set_name name_msg;
    name_msg.header = header;
    strcpy(name_msg.name, name);
    serialize_set_name(name_msg, packet);
    write(socket_fd, packet, sizeof(packet));
    
    // wait for user input
    while(1){
        n = 0;
        explicit_bzero(input, sizeof(input));
        printf(": ");
        while((input[n++] = getchar()) != '\n');
        input[strlen(input) - 1] = '\0';

        strcpy(command, strtok(input, " "));
        if(strcmp(command, "/join") == 0){
            sprintf(packet, "%i,%i,%s", JOIN_ROOM, 16, strtok(NULL, " "));
            write(socket_fd, packet, sizeof(packet));
        } else if(strcmp(command, "/quit") == 0){
            sprintf(packet, "%i,%i", DISCONNECT, 1);
            write(socket_fd, packet, sizeof(packet));
            return;
        } else{
            sprintf(packet, "%i,%i,%s,%s", UPLOAD_MSG, MSG_LENGTH+16, 
                    command, strtok(NULL, ""));
            write(socket_fd, packet, sizeof(packet));
        }
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
        return recieve_packets(socket_fd);
    } else {
        // we are in the child process
        send_packets(socket_fd);
        return 0;
    }
}
