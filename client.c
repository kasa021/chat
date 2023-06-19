#define _GNU_SOURCE
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define BUFSIZE 6144

char ASCII_ART[10][20] = { "squid", "cat","spongebob","airou" };
typedef struct {
    char *name;              // スレッドの名前
    char *username;          // ユーザ名
    int socket_fd;           // ソケットのファイル識別子
    pthread_mutex_t *mlock;  // 排他制御用の mutex
    pthread_t *sender;       // 送信用スレッドのファイル識別子
    pthread_t *recv;         // 受信用スレッドのファイル識別子
} mythread_args_t;

void chop(char *str) {
    char *p = strchr(str, '\n');
    if (p != NULL)
        *p = '\0';
}

void *reception(void *arg) {
    mythread_args_t *args = (mythread_args_t *) arg;
    char buffer[BUFSIZE];
    int socket_fd = args->socket_fd;
    pthread_mutex_t *mlock = args->mlock;
    pthread_t *send_thread = args->sender;

    while (1) {
        ssize_t recv_bytes = recv(socket_fd, buffer, BUFSIZE, 0);
        if (recv_bytes == -1) {  // 受信
            perror("client: recv");
            exit(EXIT_FAILURE);
        }

        pthread_mutex_lock(mlock);
        printf("%.*s\n", (int) recv_bytes, buffer);  // 受信したメッセージを表示
        pthread_mutex_unlock(mlock);
        if (strcmp(buffer, "quit") == 0) {
            pthread_exit(NULL);
        }
    }
}
void send_ascii_art(int socket_fd, pthread_mutex_t *mlock, char *buffer) {
    FILE *file;
    size_t file_size = strlen(buffer) + 14;
    char *file_path = (char *) malloc(file_size);
    snprintf(file_path, file_size, "./assets/%s.txt", buffer);

    file = fopen(file_path, "r");
    if (file == NULL) {
        perror("client: fopen");
        exit(EXIT_FAILURE);
    }

    char art_buffer[BUFSIZE];
    size_t read_bytes;
    while ((read_bytes = fread(art_buffer, 1, BUFSIZE, file)) > 0) {
        pthread_mutex_lock(mlock);
        printf("\x1b[32m");                         // ANSIエスケープシーケンスで表示の色を変更
        fwrite(art_buffer, 1, read_bytes, stdout);  // アスキーアートを表示
        printf("\x1b[0m");  
        printf("\n");
        fflush(stdout);                                          // 出力をフラッシュ
        pthread_mutex_unlock(mlock);
        if (send(socket_fd, art_buffer, read_bytes, 0) == -1) {  // アスキーアートを送信
            perror("client: send");
            exit(EXIT_FAILURE);
        }
    }

    fclose(file);
    free(file_path);
}

void *send_message(void *arg) {
    mythread_args_t *args = (mythread_args_t *) arg;
    char buffer[BUFSIZE];
    int socket_fd = args->socket_fd;
    pthread_mutex_t *mlock = args->mlock;
    pthread_t *recv_thread = args->recv;

    printf("Enter your username: ");
    fgets(buffer, BUFSIZE, stdin);
    chop(buffer);

    args->username = strdup(buffer);

    while (1) {
        fgets(buffer, BUFSIZE, stdin);
        chop(buffer);
        if (strcmp(buffer, "quit") == 0) {
            send(socket_fd, buffer, strlen(buffer) + 1, 0);
            pthread_exit(NULL);
        }

        chop(args->username);

        int is_ascii_art = 0;
        for (int i = 0; i < 10; i++) {
            if (strcmp(buffer, ASCII_ART[i]) == 0) {
                is_ascii_art = 1;
                send_ascii_art(socket_fd, mlock, buffer);
            }
        }
        if (is_ascii_art) continue;

        size_t message_size = strlen(args->username) + strlen(buffer) + 3;
        char *message = (char *) malloc(message_size);
        snprintf(message, message_size, "%s: %s", args->username, buffer);

        pthread_mutex_lock(mlock);
        printf("\033[1A\033[K");
        printf("\x1b[32m%s\n\x1b[0m", message);
        if (send(socket_fd, message, strlen(message) + 1, 0) == -1) {
            perror("client: send");
            exit(EXIT_FAILURE);
        }
        pthread_mutex_unlock(mlock);

        if (strcmp(buffer, "quit") == 0) {
            free(message);
            pthread_exit(NULL);
        }

        free(message);
    }
}

int main(int argc, char *argv[]) {
    int socket_fd;
    struct sockaddr_in server;
    struct hostent *hp;
    uint16_t port;
    char buffer[BUFSIZE];

    if (argc != 3) {
        fprintf(stderr, "Usage: %s <hostname> <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    port = atoi(argv[2]);

    socket_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (socket_fd == -1) {
        perror("client: socket");
        exit(EXIT_FAILURE);
    }

    memset((void *) &server, 0, sizeof(server));
    server.sin_family = PF_INET;
    server.sin_port = htons(port);

    if ((hp = gethostbyname(argv[1])) == NULL) {
        perror("client: gethostbyname");
        exit(EXIT_FAILURE);
    }

    memcpy(&server.sin_addr, hp->h_addr_list[0], hp->h_length);

    if (connect(socket_fd, (struct sockaddr *) &server, sizeof(server)) == -1) {
        perror("client: connect");
        exit(EXIT_FAILURE);
    }

    pthread_t reception_thread, send_thread;
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    mythread_args_t args1 = { "reception", NULL, socket_fd, &mutex, &send_thread, &reception_thread };
    mythread_args_t args2 = { "send", NULL, socket_fd, &mutex, &send_thread, &reception_thread };

    pthread_create(&reception_thread, NULL, reception, (void *) &args1);
    pthread_create(&send_thread, NULL, send_message, (void *) &args2);

    while (1) {
        if (pthread_tryjoin_np(reception_thread, NULL) == 0) {
            printf("reception_thread is finished\n");
            pthread_cancel(send_thread);
            printf("send_thread is canceled\n");
            break;
        }
        if (pthread_tryjoin_np(send_thread, NULL) == 0) {
            printf("send_thread is finished\n");
            pthread_cancel(reception_thread);
            printf("reception_thread is canceled\n");
            break;
        }
    }

    printf("終了\n");

    close(socket_fd);

    return 0;
}