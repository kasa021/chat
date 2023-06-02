// simple_server_async.c
// クライアントから文字列を受信するサーバプログラム (非同期通信版)
//
// 使い方：
// ./simple_server_async ポート番号
//
// 実行例：
// ./simple_server_async 50000
//

#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#define BUFSIZE 256
#define SOCK_MAX 4
#define UNUSED (-1)

void chop(char *str) {
  char *p = strchr(str, '\n');
  if (p != NULL)
    *p = '\0';
}

int main(int argc, char *argv[]) {
  int listening_socket;  // socket() が返すファイル識別子
  int connected_socket;  // accept() が返すファイル識別子
  int fd_stdin;
  fd_set readfds;
  struct sockaddr_in server;  // サーバプロセスのソケットアドレス情報
  struct sockaddr_in client;  // クライアントプロセスのソケットアドレス情報
  socklen_t fromlen;          // クライアントプロセスのソケットアドレス情報の長さ
  uint16_t port;              // ポート番号
  char buffer[BUFSIZE];       // メッセージを格納するバッファ
  int temp = 1;
  int retval;
  int input_flag;
  struct timeval timeout = { 0, 10 };  // select のタイムアウト時間を設定

  if (argc != 2) {
    fprintf(stderr, "Usage: %s <port>\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  port = atoi(argv[1]);

  // ソケットの作成: INET ドメイン・ストリーム型
  listening_socket = socket(PF_INET, SOCK_STREAM, 0);
  if (listening_socket == -1) {
    perror("server: socket");
    exit(EXIT_FAILURE);
  }

  // ソケットオプションの設定
  // SO_REUSEADDR を指定しておかないと、サーバの異常終了後に再起動した場合
  // 数分間ポートがロックされ bind() に失敗することがある
  if (setsockopt(listening_socket, SOL_SOCKET, SO_REUSEADDR, (void *) &temp, sizeof(temp))) {
    perror("server: setsockopt");
    exit(EXIT_FAILURE);
  }

  // サーバプロセスのソケットアドレス情報の設定
  memset((void *) &server, 0, sizeof(server));  // アドレス情報構造体の初期化
  server.sin_family = PF_INET;                  // プロトコルファミリの設定
  server.sin_port = htons(port);                // ポート番号の設定
  server.sin_addr.s_addr = htonl(INADDR_ANY);

  // ソケットにアドレスをバインド
  if (bind(listening_socket, (struct sockaddr *) &server, sizeof(server)) == -1) {
    perror("server: bind");
    exit(EXIT_FAILURE);
  }

  // 接続要求の受け入れ準備
  // バインドされたソケットを待機状態に
  if (listen(listening_socket, 5) == -1) {
    perror("server: listen");
    exit(EXIT_FAILURE);
  }

  memset((void *) &client, 0, sizeof(client));
  fromlen = sizeof(client);
  // クライアントからの接続要求を受け入れ、通信経路を確保する
  // クライアントと接続したソケットの識別子が connected_socket に格納される
  connected_socket = accept(listening_socket, (struct sockaddr *) &client, &fromlen);
  if (connected_socket == -1) {
    perror("server: accept");
    exit(1);
  }

  printf("Connection is established.\n");

  // listening_socket は必要なくなったので閉じる
  close(listening_socket);

  input_flag = 1;  // > を出すタイミングの制御に使う
  fd_stdin = 0;    // 標準入力は0番

  // クライアントと接続されているソケットとデータの送受信
  while (1) {
    // 監視対象のソケットを readfds に登録する
    FD_ZERO(&readfds);                   // readfds の 初期化
    FD_SET(connected_socket, &readfds);  // クライアントと接続したソケットをreadfdsに登録
    FD_SET(fd_stdin, &readfds);          // 標準入力もreadfdsに登録

    if (input_flag) {
      printf("> ");
      fflush(stdout);
      input_flag = 0;
    }

    // readfds に登録したディスクリプタの状態をselect関数で監視する．
    // 入出力準備ができているディスクリプタが存在する場合はその個数を返す．
    // timeout で設定した時間内に何も起こらなかった場合は時間切れとなり0を返し，エラーの場合は-1を返す．
    retval = select(FD_SETSIZE, &readfds, NULL, NULL, &timeout);

    if (retval == -1) {  // エラーが発生した場合
      perror("select");
      exit(EXIT_FAILURE);
    }
    else if (retval != 0) {                        // 監視対象の
      if (FD_ISSET(connected_socket, &readfds)) {  // connected_socket が入出力可能な場合
        // 受信データを読み込む
        memset(buffer, '\0', BUFSIZE);  //バッファの初期化
        // データ受信
        recv(connected_socket, buffer, BUFSIZE, 0);
        printf("\rfrom client: %s\n> ", buffer);
        fflush(stdout);  // printfを実行した結果を直ちに標準出力(画面)に放出する
      }

      if (FD_ISSET(fd_stdin, &readfds)) {  // 標準入力からデータの読み込みが可能な場合
        // 送信データを読み込む
        memset(buffer, '\0', BUFSIZE);
        if (fgets(buffer, BUFSIZE, stdin) == NULL) {
          strcpy(buffer, "quit");
        }
        chop(buffer);

        // データ送信
        send(connected_socket, buffer, BUFSIZE, 0);

        if (strcmp(buffer, "quit") == 0) {  // 入力された文字列が"quit"の場合は終了
          break;
        }
        input_flag = 1;
      }
    }
  }

  close(connected_socket);

  return 0;
}
