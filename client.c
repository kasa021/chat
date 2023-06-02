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
    char name[64];            // スレッドの名前
    int socket_fd;            // ソケットのファイル識別子
    pthread_mutex_t *mlock;   // 排他制御用の mutex
    pthread_t *sender;       // 送信用スレッドのファイル識別子
    pthread_t *recv;        // 受信用スレッドのファイル識別子
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
        printf("%s\n", buffer);                           // 受信した文字列を表示
        pthread_mutex_unlock(mlock);                      // mutex をアンロック
        if (strcmp(buffer, "quit") == 0) {    
            printf("recv_thread 終了\n");
            pthread_exit(NULL);                           // スレッドを終了
        }
    }
}

void *send_message(void *arg) {                                      // 送信用スレッドの関数
    mythread_args_t *args = (mythread_args_t *) arg;                 // 引数を構造体にキャスト
    char buffer[BUFSIZE];                                            // 送信する文字列を格納するバッファ
    int socket_fd = args->socket_fd;                                 // ソケットのファイル識別子
    pthread_mutex_t *mlock = args->mlock;                            // 排他制御用の mutex
    pthread_t *recv_thread = args->recv;                    // 受信用スレッドのファイル識別子

    while (1) {                                                      // 標準入力から文字列を読み込み，サーバに送信する
        fgets(buffer, BUFSIZE, stdin);                               // 標準入力から文字列を読み込む
        chop(buffer);                                                // 文字列の末尾にある改行コードを削除
        pthread_mutex_lock(mlock);                                   //  mutex をロック
        if (send(socket_fd, buffer, strlen(buffer) + 1, 0) == -1) {  // 文字列を送信
            perror("client: send");                                  // 送信に失敗した場合
            exit(EXIT_FAILURE);                                      // 異常終了
        }
        pthread_mutex_unlock(mlock);                                 //  mutex をアンロック
        if (strcmp(buffer, "quit") == 0) {
            printf("send_thread 終了\n");
            pthread_exit(NULL);                                      // 送信した文字列が "quit" の場合
        }
    }
}

void *monitoring(void *arg) {                         // 監視用スレッドの関数
    mythread_args_t *args = (mythread_args_t *) arg;  // 引数を構造体にキャスト
    pthread_t *recv_thread = args->recv;  // 受信用スレッドのファイル識別子
    pthread_t *send_thread = args->sender;       // 送信用スレッドのファイル識別子

    while (1) {  // 受信スレッド、または送信スレッドが終了したらもう一方も終了させる
        // pthread_join はスレッドが終了するまで待機する関数
        if (pthread_join(*recv_thread, NULL) == 0) {
            printf("recv_thread 終了\n");
            pthread_cancel(*send_thread);  // 送信スレッドを終了
            pthread_exit(NULL);            // 監視スレッドを終了
        }
        if (pthread_join(*send_thread, NULL) == 0) {
            printf("send_thread 終了\n");
            pthread_cancel(*recv_thread);  // 受信スレッドを終了
            pthread_exit(NULL);            // 監視スレッドを終了
        }
    }
}

int main(int argc, char *argv[]) {
    int socket_fd;              // socket() の返すファイル識別子
    struct sockaddr_in server;  // サーバプロセスのソケットアドレス情報
    struct hostent *hp;         // ホスト情報
    uint16_t port;              // ポート番号
    char buffer[BUFSIZE];       // メッセージを格納するバッファ
    pthread_t reception_thread, send_thread, monitoring_thread;

    if (argc != 3) {            // 引数の数が正しいか確認
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

    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    mythread_args_t args1 = { "reception", socket_fd, &mutex, &send_thread, &reception_thread };
    mythread_args_t args2 = { "send", socket_fd, &mutex, &reception_thread, &send_thread };
    mythread_args_t args3 = { "monitoring", socket_fd, &mutex, &reception_thread, &send_thread };

    // スレッドの作成と実行
    pthread_create(&reception_thread, NULL, reception, (void *) &args1);
    pthread_create(&send_thread, NULL, send_message, (void *) &args2);
    pthread_create(&monitoring_thread, NULL, monitoring, (void *) &args3);

    // スレッドの終了を待機
    pthread_join(reception_thread, NULL);
    pthread_join(send_thread, NULL);
    pthread_join(monitoring_thread, NULL);

    close(socket_fd);  // ソケットを閉じる



    return 0;
}
