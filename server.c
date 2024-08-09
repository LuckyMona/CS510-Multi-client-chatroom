#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#define BUFFER_SIZE 1024
#define MAX_CLIENTS 5

int client_sockets[MAX_CLIENTS] = {0}; // Store client sockets

// Function declarations
void error(const char *msg);
void broadcast(int sender_index, const char *message);
void close_all_sockets();
void log_connection(struct sockaddr_in *client_addr);
void log_disconnection(int client_index);
void log_message(int client_index, const char *message);
void start_server(int port);

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    int port = atoi(argv[1]);
    start_server(port);

    return 0;
}

// General server start function
void start_server(int port) {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    int i, yes = 1;
    fd_set readfds, masterfds;

    // Create socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) error("Could not create socket");

    // Allow address reuse
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0)
        error("setsockopt failed");

    // Initialize server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    // Bind socket to address
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
        error("Could not bind to the address");

    // Start listening
    listen(server_fd, MAX_CLIENTS);
    printf("Server is listening on port %d...\n", port);

    FD_ZERO(&masterfds);
    FD_SET(server_fd, &masterfds);
    FD_SET(STDIN_FILENO, &masterfds); // Add STDIN to the master set

    while (1) {
        readfds = masterfds;

        int activity = select(FD_SETSIZE, &readfds, NULL, NULL, NULL);
        if (activity < 0) {
            if (errno == EINTR) continue; // Interrupted by a signal, continue
            error("select failed");
        }

        // Check for new connections
        if (FD_ISSET(server_fd, &readfds)) {
            for (i = 0; i < MAX_CLIENTS; ++i) {
                if (client_sockets[i] == 0) {
                    break;
                }
            }
            if (i == MAX_CLIENTS) {
                perror("Too many clients");
                continue;
            }

            client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
            if (client_fd < 0) {
                error("Error on accept");
            }

            log_connection(&client_addr);

            // Save the client socket
            client_sockets[i] = client_fd;

            // Add new client socket to master set
            FD_SET(client_fd, &masterfds);
        }

        // Handle messages from clients
        for (i = 0; i < MAX_CLIENTS; ++i) {
            int sockfd = client_sockets[i];
            if (sockfd > 0 && FD_ISSET(sockfd, &readfds)) {
                char buffer[BUFFER_SIZE];
                int n = read(sockfd, buffer, sizeof(buffer));
                if (n > 0) {
                    buffer[n] = '\0';
                    log_message(i, buffer);
                    // Broadcast message to all clients, including the sender
                    broadcast(i, buffer);
                } else {
                    if (n < 0) {
                        perror("Read from client failed");
                    } else {
                        log_disconnection(i);
                    }
                    close(client_sockets[i]);
                    client_sockets[i] = 0;
                    FD_CLR(sockfd, &masterfds);
                }
            }
        }

        // Check if the server wants to send a message to all clients
        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            char buffer[BUFFER_SIZE];
            if (fgets(buffer, sizeof(buffer), stdin) != NULL) {
                buffer[strcspn(buffer, "\n")] = '\0'; // Remove newline character
                broadcast(-1, buffer); // Broadcast to all clients from the server itself
            }
        }
    }

    close_all_sockets();
    close(server_fd);
}

// Function to broadcast a message to all clients
void broadcast(int sender_index, const char *message) {
    char formatted_message[BUFFER_SIZE];
    if (sender_index >= 0) {
        // Format message as "from client X: <message>"
        snprintf(formatted_message, BUFFER_SIZE, "from client %d: %s", sender_index, message);
    } else {
        // If the server itself is sending the message
        snprintf(formatted_message, BUFFER_SIZE, "from server: %s", message);
    }

    printf("Broadcasting message: %s\n", formatted_message); // Debug print

    for (int i = 0; i < MAX_CLIENTS; i++) {
        int sockfd = client_sockets[i];
        if (sockfd > 0) { // Broadcast to all active clients
            printf("Sending message to client %d\n", i); // Debug print

            // The actual send operation
            if (send(sockfd, formatted_message, strlen(formatted_message), 0) < 0) {
                perror("Send to client failed");
            }
        }
    }
}

// Function to close all sockets
void close_all_sockets() {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_sockets[i] > 0) {
            close(client_sockets[i]);
        }
    }
}

// Function for error handling
void error(const char *msg) {
    perror(msg);
    exit(1);
}

// Function to log new connection
void log_connection(struct sockaddr_in *client_addr) {
    printf("New client connected: %s\n", inet_ntoa(client_addr->sin_addr));
}

// Function to log disconnection
void log_disconnection(int client_index) {
    printf("Client %d disconnected\n", client_index);
    client_sockets[client_index] = 0;
}

// Function to log messages
void log_message(int client_index, const char *message) {
    printf("\nReceived Message from client %d: %s\n", client_index, message);
}
