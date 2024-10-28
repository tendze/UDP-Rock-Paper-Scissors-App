#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define BUFFER_SIZE 256

int main(int argc, char *argv[]) {
  if (argc != 3) {
    printf("Неверное количество аргументов.\n");
    printf("Передайте IP (-1 если 127.0.0.1), порт (-1 если 8080)N\n");
    exit(EXIT_FAILURE);
  }

  char *IPaddr = strcmp(IPaddr, "-1") == 0 ? "127.0.0.1" : IPaddr;
  int port = atoi(argv[2]) == -1 ? 8080 : atoi(argv[2]);

  int sockfd;
  char buffer[BUFFER_SIZE];
  char *message = "Connect me please... Im observer!";
  struct sockaddr_in servaddr;

  if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("socket creation failed");
    exit(EXIT_FAILURE);
  }

  memset(&servaddr, 0, sizeof(servaddr));

  // Инфа о сервере
  servaddr.sin_family = AF_INET;
  servaddr.sin_port = htons(port);
  servaddr.sin_addr.s_addr = inet_addr(IPaddr);

  // Первый запрос - на подключение
  int len = sizeof(servaddr);
  sendto(sockfd, (const char *)message, strlen(message), 0,
         (const struct sockaddr *)&servaddr, len);
  printf("Сообщение серверу: %s\n", message);

  // Получение ответа от сервера
  int n = recvfrom(sockfd, (char *)buffer, BUFFER_SIZE, 0,
                   (struct sockaddr *)&servaddr, &len);
  if (n < 0) {
    perror("recvfrom failed");
    close(sockfd);
    exit(EXIT_FAILURE);
  }
  buffer[n] = '\0';
  printf("%s", buffer);

  while (1) {
    int n = recvfrom(sockfd, (char *)buffer, BUFFER_SIZE, 0,
                     (struct sockaddr *)&servaddr, &len);
    if (n < 0) {
      perror("recvfrom failed");
      close(sockfd);
      exit(EXIT_FAILURE);
    }
    if (strncmp(buffer, "END", 3) == 0) {
      printf("%s", buffer);
      exit(1);
    }

    buffer[n] = '\0';
    printf("%s\n", buffer);
  }
  close(sockfd);
  return 0;
}