#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>

#define SERVER_IP "127.0.0.1"
#define BUFFER_SIZE 1024

void error(const char *msg);

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <server_port>\n", argv[0]);
        exit(1);
    }

    int server_port = atoi(argv[1]);
    int sockfd;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    fd_set readfds;

    // Create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Could not create socket");
        exit(1);
    }

    // Initialize server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

    // Connect to server
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connect failed");
        close(sockfd);
        exit(1);
    }

    printf("Connected to server on port %d\n", server_port);
    printf("Enter message: ");
    fflush(stdout);

    // Send and receive messages
    while (1) {
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);
        FD_SET(STDIN_FILENO, &readfds);

        int max_fd = sockfd > STDIN_FILENO ? sockfd : STDIN_FILENO;

        int activity = select(max_fd + 1, &readfds, NULL, NULL, NULL);
        if (activity < 0 && errno != EINTR) {
            perror("select error");
            break;
        }

        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            if (fgets(buffer, BUFFER_SIZE, stdin) != NULL) {
                send(sockfd, buffer, strlen(buffer), 0);
                // After sending a message, print the prompt again
                printf("Enter message: ");
                fflush(stdout);
            }
        }

        if (FD_ISSET(sockfd, &readfds)) {
            int n = recv(sockfd, buffer, BUFFER_SIZE, 0);
            if (n > 0) {
                buffer[n] = '\0';

                // Parse the message to extract the sender information and content
                int sender_client_id;
                char message_content[BUFFER_SIZE];

                if (sscanf(buffer, "from client %d: %[^\n]", &sender_client_id, message_content) == 2) {
                    printf("\nReceived broadcast message: from client %d: %s\n", sender_client_id, message_content);

                } else {
                    // Fallback if the format is different
                    printf("\nReceived broadcast message: %s\n", buffer);
                }

                // Print the prompt again after displaying the received message
                printf("Enter message: ");
                fflush(stdout);

            } else if (n == 0) {
                printf("Server closed the connection\n");
                break;
            } else {
                perror("recv failed");
                break;
            }
        }
    }

    close(sockfd);
    return 0;
}

void error(const char *msg) {
    perror(msg);
    exit(1);
}
