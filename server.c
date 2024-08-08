#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>

#define BUFFER_SIZE 1024
#define MAX_CLIENTS 5

int child_pipes[MAX_CLIENTS][2]; // Pipes for each child process
pid_t child_pids[MAX_CLIENTS] = {0}; // PIDs for child processes

// Function declarations
void error(const char *msg);
void broadcast(const char *message, size_t len);
void handle_client(int client_index, int client_fd);
void close_all_pipes();
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
                if (child_pids[i] <= 0) {
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

            // Create pipe
            if (pipe(child_pipes[i]) < 0) {
                error("Could not create pipe");
            }

            // Create child process
            child_pids[i] = fork();
            if (child_pids[i] == 0) {
                // Child process closes server socket and unused pipe ends
                close(server_fd);
                close(child_pipes[i][1]); // Child process does not need write end
                handle_client(i, client_fd);
                close(client_fd);
                exit(0);
            } else if (child_pids[i] < 0) {
                error("Fork failed");
            }

            // Parent process closes client socket's read end
            close(client_fd);
            close(child_pipes[i][0]); // Parent process does not need read end

            FD_SET(child_pipes[i][1], &masterfds);
        }

        // Handle messages from child processes
        for (i = 0; i < MAX_CLIENTS; ++i) {
            if (child_pids[i] > 0 && FD_ISSET(child_pipes[i][1], &readfds)) {
                char buffer[BUFFER_SIZE];
                int n = read(child_pipes[i][1], buffer, sizeof(buffer));
                if (n > 0) {
                    buffer[n] = '\0';
                    printf("Broadcasting message from client %d: %s\n", i, buffer);
                    log_message(i, buffer);
                    // Broadcast message to all clients
                    broadcast(buffer, n);
                } else {
                    if (n < 0) {
                        perror("Read from pipe failed");
                    } else {
                        log_disconnection(i);
                    }
                    close(child_pipes[i][1]);
                    child_pipes[i][1] = -1;
                    child_pids[i] = 0;
                    FD_CLR(child_pipes[i][1], &masterfds);
                }
            }
        }
    }

    close_all_pipes();
    close(server_fd);
}

// Function to handle client connection
void handle_client(int client_index, int client_fd) {
    char buffer[BUFFER_SIZE];
    while (1) {
        int n = recv(client_fd, buffer, sizeof(buffer), 0);
        if (n > 0) {
            buffer[n] = '\0';
            printf("Received from client %d: %s\n", client_index, buffer);
            // Send message to parent process
            write(child_pipes[client_index][1], buffer, n);
        } else if (n == 0) {
            printf("Client %d closed the connection\n", client_index);
            break;
        } else {
            perror("recv failed");
            break;
        }
    }
}

// Function to broadcast message to all clients
void broadcast(const char *message, size_t len) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (child_pipes[i][1] > 0) {
            write(child_pipes[i][1], message, len); // Write to pipe
        }
    }
}

// Function to close all pipes
void close_all_pipes() {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        close(child_pipes[i][0]);
        close(child_pipes[i][1]);
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
}

// Function to log messages
void log_message(int client_index, const char *message) {
    printf("Message from client %d: %s\n", client_index, message);
}
