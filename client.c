#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include "structs.h"
#define MSG_LENGTH 50
#define PORT 8888


int recieve_messages(int socket_fd)
{
    char message[MSG_LENGTH] = {0};
    while(1){
        int ret = read(socket_fd, message, sizeof(message));
        if(ret == 0){
            printf("Dissconected from server");
        }
        else if(ret > 0){
            printf("%s\n", message);
        }
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

    pid_t pid = fork();
    if(pid < 0){
        printf("Failed to fork");
        return 1;
    } else if (pid > 0) {
        // we are in the parent process
        recieve_messages(socket_fd);

    } else {
        // we are in the child process
        send_messages(socket_fd);
    }

    while(1){
        
    }
}
