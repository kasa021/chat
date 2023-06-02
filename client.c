// simple_client.c
// ./simple_client 接続ホスト ポート番号
// サーバへ文字列を送信するクライアントプログラム

#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define BUFSIZE 256

void chop(char *str) {
  char *p = strchr(str, '\n');
  if (p != NULL)
    *p = '\0';
}

int main(int argc, char *argv[]) {
  int socket_fd;              // socket() の返すファイル識別子
  struct sockaddr_in server;  // サーバプロセスのソケットアドレス情報
  struct hostent *hp;         // ホスト情報
  uint16_t port;              // ポート番号
  char buffer[BUFSIZE];       // メッセージを格納するバッファ

  if (argc != 3) {
    fprintf(stderr, "Usage: %s <hostname> <port>\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  port = atoi(argv[2]);

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

  // サーバとデータを送受信する
  while (1) {
    // 送信データを読み込む
    memset(buffer, '\0', BUFSIZE);  //バッファの初期化
    printf("> ");
    if (fgets(buffer, BUFSIZE, stdin) == NULL) {
      strcpy(buffer, "quit");
    }

    chop(buffer);

    // データ送信
    send(socket_fd, buffer, BUFSIZE, 0);

    if (strcmp(buffer, "quit") == 0) {
      break;
    }

    // 受信データを読み込む
    memset(buffer, '\0', BUFSIZE);  //バッファの初期化
    // データ受信
    recv(socket_fd, buffer, BUFSIZE, 0);
    printf("from server: %s\n", buffer);
    fflush(stdout);  // printfを実行した結果を直ちに標準出力(画面)に放出する
  }

  close(socket_fd);

  return 0;
}
