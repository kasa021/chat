#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define BUFSIZE 256

typedef struct {
    char *name;              // スレッドの名前
    char *username;          // ユーザ名
    int socket_fd;           // ソケットのファイル識別子
    pthread_mutex_t *mlock;  // 排他制御用の mutex
    pthread_t *sender;       // 送信用スレッドのファイル識別子
    pthread_t *recv;         // 受信用スレッドのファイル識別子
} mythread_args_t;

void chop(char *str) {  // 文字列の末尾にある改行コードを削除する関数
    char *p = strchr(str, '\n');
    if (p != NULL)
        *p = '\0';
}

void *reception(void *arg) {                              // 受信用スレッドの関数
    mythread_args_t *args = (mythread_args_t *) arg;      // 引数を構造体にキャスト
    char buffer[BUFSIZE];                                 // 受信した文字列を格納するバッファ
    int socket_fd = args->socket_fd;                      // ソケットのファイル識別子
    pthread_mutex_t *mlock = args->mlock;                 // 排他制御用の mutex
    pthread_t *send_thread = args->sender;                // 送信用スレッドのファイル識別子

    while (1) {                                           // 受信した文字列を表示する
        if (recv(socket_fd, buffer, BUFSIZE, 0) == -1) {  // 文字列を受信
            perror("client: recv");                       // 受信に失敗した場合
            exit(EXIT_FAILURE);                           // 異常終了
        }
        pthread_mutex_lock(mlock);                        // mutex をロック
        printf("%s\n",buffer);       // 受信した文字列を表示
        pthread_mutex_unlock(mlock);                      // mutex をアンロック
        if (strcmp(buffer, "quit") == 0) {
            pthread_exit(NULL);
        }
    }
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

    args->username = strdup(buffer);  // ユーザ名を設定

    while (1) {
        fgets(buffer, BUFSIZE, stdin);
        chop(buffer);
        chop(args->username);  // ユーザ名の末尾にある改行コードを削除

        size_t message_size = strlen(args->username) + strlen(buffer) + 3;
        char *message = (char *) malloc(message_size);
        snprintf(message, message_size, "%s: %s", args->username, buffer);

        pthread_mutex_lock(mlock);
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
    int socket_fd;                            // socket() の返すファイル識別子
    struct sockaddr_in server;                // サーバプロセスのソケットアドレス情報
    struct hostent *hp;                       // ホスト情報
    uint16_t port;                            // ポート番号
    char buffer[BUFSIZE];                     // メッセージを格納するバッファ

    if (argc != 3) {                          // 引数の数が正しいか確認
        fprintf(stderr, "Usage: %s <hostname> <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    port = atoi(argv[2]);  // ポート番号を設定

    // ソケットの作成: INET ドメイン・ストリーム型
    socket_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (socket_fd == -1) {
        perror("client: socket");
        exit(EXIT_FAILURE);
    }

    // サーバプロセスのソケットアドレス情報の設定
    memset((void *) &server, 0, sizeof(server));  // アドレス情報構造体の初期化
    server.sin_family = PF_INET;                  // プロトコルファミリの設定
    server.sin_port = htons(port);                // ポート番号の設定

    // argv[1] のマシンの IP アドレスを返す
    if ((hp = gethostbyname(argv[1])) == NULL) {
        perror("client: gethostbyname");
        exit(EXIT_FAILURE);
    }
    // IP アドレスの設定
    memcpy(&server.sin_addr, hp->h_addr_list[0], hp->h_length);

    // サーバに接続．サーバが起動し，bind(), listen() している必要あり
    if (connect(socket_fd, (struct sockaddr *) &server, sizeof(server)) == -1) {
        perror("client: connect");
        exit(EXIT_FAILURE);
    }

    pthread_t reception_thread, send_thread;  // 送受信用スレッドのファイル識別子
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    mythread_args_t args1 = { "reception", NULL, socket_fd, &mutex, &send_thread, &reception_thread };
    mythread_args_t args2 = { "send", NULL, socket_fd, &mutex, &send_thread, &reception_thread };

    // スレッドの作成と実行
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

    close(socket_fd);  // ソケットを閉じる

    return 0;
}
