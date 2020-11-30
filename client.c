#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <stdio.h>
#include <unistd.h>
#include <poll.h>
#define MSG_LENGTH 50
#define PORT 8888

int recieve_messages(int socket_fd)
{
    struct pollfd fds[1] = {socket_fd, POLLIN, 0};
    while(1){
        if (poll(fds, 1, -1) > 0){
            printf("new message\n");
        }
    }
}

void send_messages(int socket_fd)
{
    char message[MSG_LENGTH] = {0};
    int n;
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
