#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define BUFSIZE 256
#define MAX_CLIENTS 10

int main(int argc, char *argv[]) {
    int socket_fd;
    struct sockaddr_in server;
    uint16_t port;
    int client_sockets[MAX_CLIENTS];  // 接続中のクライアントのソケット
    fd_set active_fds;
    int max_fd = 0;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    port = atoi(argv[1]);

    // ソケットの作成
    socket_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (socket_fd == -1) {
        perror("server: socket");
        exit(EXIT_FAILURE);
    }

    // サーバのソケットアドレス情報の設定
    memset((void *) &server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = htonl(INADDR_ANY);

    // ソケットにアドレスをバインド
    if (bind(socket_fd, (struct sockaddr *) &server, sizeof(server)) == -1) {
        perror("server: bind");
        exit(EXIT_FAILURE);
    }

    // ソケットをリッスン状態にする
    if (listen(socket_fd, MAX_CLIENTS) == -1) {
        perror("server: listen");
        exit(EXIT_FAILURE);
    }

    printf("Server started. Waiting for connections...\n");

    FD_ZERO(&active_fds);
    FD_SET(socket_fd, &active_fds);
    max_fd = socket_fd;

    for (int i = 0; i < MAX_CLIENTS; i++) {
        client_sockets[i] = 0;
    }

    while (1) {
        fd_set read_fds = active_fds;

        // クライアントの接続を監視
        if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("server: select");
            exit(EXIT_FAILURE);
        }

        // 新しいクライアントの接続を処理
        if (FD_ISSET(socket_fd, &read_fds)) {
            struct sockaddr_in client;
            socklen_t client_len = sizeof(client);

            // クライアントからの接続を待機
            int client_socket = accept(socket_fd, (struct sockaddr *) &client, &client_len);
            if (client_socket == -1) {
                perror("server: accept");
                // エラー処理
                continue;
            }

            printf("Client connected. Socket FD: %d\n", client_socket);

            // 接続されたクライアントのソケットを配列に格納
            int i;
            for (i = 0; i < MAX_CLIENTS; i++) {
                if (client_sockets[i] == 0) {
                    client_sockets[i] = client_socket;
                    break;
                }
            }

            if (i == MAX_CLIENTS) {
                fprintf(stderr, "Reached maximum number of clients.\n");
                close(client_socket);
            }
            else {
                FD_SET(client_socket, &active_fds);
                if (client_socket > max_fd) {
                    max_fd = client_socket;
                }
            }
        }

        // クライアントからのメッセージを処理
        for (int i = 0; i < MAX_CLIENTS; i++) {
            int client_socket = client_sockets[i];
            if (client_socket != 0 && FD_ISSET(client_socket, &read_fds)) {
                char buffer[BUFSIZE];
                ssize_t bytes_read;

                // クライアントからのメッセージを受信
                bytes_read = recv(client_socket, buffer, BUFSIZE, 0);
                if (bytes_read > 0) {
                    // メッセージを他のクライアントに転送
                    for (int j = 0; j < MAX_CLIENTS; j++) {
                        int dest_socket = client_sockets[j];
                        if (dest_socket != 0 && dest_socket != client_socket) {
                            if (send(dest_socket, buffer, bytes_read, 0) == -1) {
                                perror("server: send");
                                // エラー処理
                                continue;
                            }
                        }
                    }
                }
                else if (bytes_read == 0) { 
                    // クライアントが切断した場合の処理
                    printf("Client disconnected. Socket FD: %d\n", client_socket);

                    // クライアントのソケットを配列から削除
                    client_sockets[i] = 0;
                    FD_CLR(client_socket, &active_fds);
                    close(client_socket);
                }
                else {
                    perror("server: recv");
                    // エラー処理
                    continue;
                }
            }
        }

        // クライアントの接続数が0ならサーバのソケットを閉じる
        int connected_clients = 0;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (client_sockets[i] != 0) {
                connected_clients++;
            }
        }
        if (connected_clients == 0) {
            printf("All clients disconnected. Closing server socket.\n");
            break;
        }
    }

    // サーバのソケットをクローズ
    close(socket_fd);
    return 0;
}
