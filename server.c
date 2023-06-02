#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define BUFSIZE 256
#define MAX_CLIENTS 10

typedef struct {
    int socket_fd;
    pthread_mutex_t *mlock;
} client_args_t;

int num_clients = 0;
int client_sockets[MAX_CLIENTS];

void *handle_client(void *arg) {
    client_args_t *client_args = (client_args_t *) arg;
    int socket_fd = client_args->socket_fd;
    pthread_mutex_t *mlock = client_args->mlock;

    char buffer[BUFSIZE];
    ssize_t bytes_read;

    while ((bytes_read = recv(socket_fd, buffer, BUFSIZE, 0)) > 0) {
        pthread_mutex_lock(mlock);

        // メッセージを他のクライアントに転送
        for (int i = 0; i < num_clients; i++) {
            int client_socket = client_sockets[i];
            if (client_socket != socket_fd) {
                if (send(client_socket, buffer, bytes_read, 0) == -1) {
                    perror("server: send");
                    // エラー処理
                }
            }
        }

        pthread_mutex_unlock(mlock);
        memset(buffer, 0, BUFSIZE);
    }

    // クライアントが切断した場合の処理
    pthread_mutex_lock(mlock);
    for (int i = 0; i < num_clients; i++) {
        if (client_sockets[i] == socket_fd) {
            // 配列からクライアントを削除
            memmove(client_sockets + i, client_sockets + i + 1, (num_clients - i - 1) * sizeof(int));
            num_clients--;
            break;
        }
    }
    pthread_mutex_unlock(mlock);

    close(socket_fd);
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    int socket_fd;
    struct sockaddr_in server;
    uint16_t port;

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
    server.sin_family = PF_INET;
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

    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_t client_threads[MAX_CLIENTS];

    while (1) {
        struct sockaddr_in client;
        socklen_t client_len = sizeof(client);

        // クライアントからの接続を待機
        int client_socket = accept(socket_fd, (struct sockaddr *) &client, &client_len);
        if (client_socket == -1) {
            perror("server: accept");
            // エラー処理
        }

        // 接続されたクライアントのソケットを配列に格納
        pthread_mutex_lock(&mutex);
        client_sockets[num_clients++] = client_socket;
        pthread_mutex_unlock(&mutex);

        printf("Client connected. Socket FD: %d\n", client_socket);

        client_args_t client_args = { client_socket, &mutex };

        // クライアントごとにスレッドを作成
        pthread_create(&client_threads[num_clients - 1], NULL, handle_client, (void *) &client_args);
    }

    // サーバのソケットをクローズ
    close(socket_fd);
    return 0;
}
