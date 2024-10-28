#include <arpa/inet.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BUFFER_SIZE 256

typedef struct {
  struct sockaddr_in addr;
  int client_number;
} Student;

int find_client(Student *clients, int client_count, struct sockaddr_in *addr) {
  for (int i = 0; i < client_count; i++) {
    if (clients[i].addr.sin_addr.s_addr == addr->sin_addr.s_addr &&
        clients[i].addr.sin_port == addr->sin_port) {
      return i;
    }
  }
  return -1;
}

int add_client(Student *clients, int *client_count, struct sockaddr_in *addr,
               int max_clients) {
  if (*client_count >= max_clients) {
    return -1;
  }
  clients[*client_count].addr = *addr;
  clients[*client_count].client_number = *client_count + 1;
  return (*client_count)++;
}

int sockfd;

void sigint_handler(int signum) {
  printf("Завершаем работу сервера.\n");
  close(sockfd);
  exit(0);
}

char *get_random_move() {
  int move = rand() % 3;
  switch (move) {
  case 0:
    return "Камень";
  case 1:
    return "Ножницы";
  case 2:
    return "Бумага";
  default:
    return "-";
  }
}

int get_winner(char *move1, char *move2) {
  if (strcmp(move1, move2) == 0) {
    return 0; // Ничья
  } else if ((strcmp(move1, "Камень") == 0 && strcmp(move2, "Ножницы") == 0) ||
             (strcmp(move1, "Ножницы") == 0 && strcmp(move2, "Бумага") == 0) ||
             (strcmp(move1, "Бумага") == 0 && strcmp(move2, "Камень") == 0)) {
    return 1; // Первый выиграл
  } else {
    return 2; // Второй
  }
}

int main(int argc, char *argv[]) {
  if (argc != 4) {
    printf("Недостаточное количество аргументов\n");
    printf("Передайте IP (-1 если 127.0.0.1), порт (-1 если 8080) и "
           "максимальное количество клиентов\n");
    exit(EXIT_FAILURE);
  }

  signal(SIGINT, sigint_handler);

  int max_clients = atoi(argv[3]);
  if (max_clients < 2) {
    fprintf(stderr, "Слишком мало студентов\n");
    exit(EXIT_FAILURE);
  }

  Student students[max_clients];
  int students_count = 0;

  char buffer[BUFFER_SIZE];
  struct sockaddr_in servaddr, cliaddr, observeraddr;
  socklen_t len;

  char *IPaddr = strcmp(argv[1], "-1") == 0 ? "127.0.0.1" : argv[1];
  int port = atoi(argv[2]) == -1 ? 8080 : atoi(argv[2]);

  if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("socket creation failed");
    exit(EXIT_FAILURE);
  }

  memset(&servaddr, 0, sizeof(servaddr));
  memset(&cliaddr, 0, sizeof(cliaddr));

  servaddr.sin_family = AF_INET;
  servaddr.sin_port = htons(port);
  servaddr.sin_addr.s_addr = inet_addr(IPaddr);

  if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
    perror("bind failed");
    close(sockfd);
    exit(EXIT_FAILURE);
  }

  printf("Сервер запущен, ждём клиента-наблюдателя...\n");

  memset(buffer, 0, sizeof(buffer));
  int n = recvfrom(sockfd, (char *)buffer, BUFFER_SIZE, 0, (struct sockaddr *)&cliaddr, &len);
  if (n < 0) {
    perror("recvfrom observer failed");
    exit(EXIT_FAILURE);
  }
  observeraddr = cliaddr;
  printf("Клиент-наблюдатель подключился, его сообщение: %s\n", buffer);
  printf("Ждём подключения %d студентов...\n", max_clients);
  while (1) {
    len = sizeof(cliaddr);
    int n = recvfrom(sockfd, (char *)buffer, BUFFER_SIZE, 0,
                     (struct sockaddr *)&cliaddr, &len);
    if (n < 0) {
      perror("recvfrom student client failed");
      continue;
    }
    buffer[n] = '\0';

    int client_index = find_client(students, students_count, &cliaddr);
    if (client_index == -1) {
      client_index =
          add_client(students, &students_count, &cliaddr, max_clients);
    }

    if (client_index != -1) {
      printf("Студент %d подключился, его сообщение: %s\n",
             students[client_index].client_number, buffer);

      char response[BUFFER_SIZE];
      snprintf(response, BUFFER_SIZE,
               "Вы подключились с id=%d. Ожидайте начало турнира...\n",
               students[client_index].client_number);
      sendto(sockfd, response, strlen(response), 0,
             (const struct sockaddr *)&cliaddr, len);
    } else {
      break;
    }

    if (students_count == max_clients) {
      printf("Все подключились\n");
      break;
    }
  }

  char observerMessage[BUFFER_SIZE];
  // Турнир
  int round = 1;
  while (students[1].client_number != -1) {
    int place = 0;
    memset(observerMessage, 0, sizeof(observerMessage));
    printf("\n--------------ROUND %d------------\n", round);
    sprintf(observerMessage, "\n--------------ROUND %d------------\n", round);
    sendto(sockfd, observerMessage, strlen(observerMessage), 0, (const struct sockaddr *)&observeraddr, sizeof(observeraddr));
    for (int i = 0; i < max_clients; i++) {
      int firstID = students[i].client_number;
      struct sockaddr_in firstAddr = students[i].addr;
      if (firstID != -1) {
        int secondID = students[i + 1].client_number;
        struct sockaddr_in secondAddr = students[i + 1].addr;
        if (i + 1 == 0 || secondID == -1) {
          students[place].client_number = firstID;
          students[place].addr = firstAddr;
          continue;
        }
        
        sprintf(observerMessage, "Студент %d играет против студента %d\n", firstID, secondID);
        sendto(sockfd, observerMessage, strlen(observerMessage), 0, (const struct sockaddr *)&observeraddr, sizeof(observeraddr));

        char message1[BUFFER_SIZE];
        char message2[BUFFER_SIZE];
        sprintf(message1, "Вы играете против студента %d", secondID);
        sprintf(message2, "Вы играете против студента %d", firstID);

        sendto(sockfd, message1, strlen(message1), 0,
               (const struct sockaddr *)&firstAddr, sizeof(firstAddr));
        sendto(sockfd, message2, strlen(message2), 0,
               (const struct sockaddr *)&secondAddr, sizeof(secondAddr));
        students[i + 1].client_number = -1;
        while (1) {
          sleep(1);
          memset(message1, 0, sizeof message1);
          memset(message2, 0, sizeof message2);

          char *choice1 = get_random_move();
          char *choice2 = get_random_move();
          int winnerNum = get_winner(choice1, choice2);

          if (winnerNum == 1) {
            sprintf(message1, "Вы выиграли! Ваш выбор: %s", choice1);
            sprintf(message2, "YOU LOSE");
            sendto(sockfd, message1, strlen(message1), 0,
                   (const struct sockaddr *)&firstAddr, sizeof(firstAddr));
            sendto(sockfd, message2, strlen(message2), 0,
                   (const struct sockaddr *)&secondAddr, sizeof(secondAddr));
            students[place].client_number = firstID;
            students[place].addr = firstAddr;
            place++;
            sprintf(observerMessage, "Студент с id %d выиграл", firstID);
            sendto(sockfd, observerMessage, strlen(observerMessage), 0, (const struct sockaddr *)&observeraddr, sizeof(observeraddr));
            break;
          } else if (winnerNum == 2) {
            sprintf(message1, "YOU LOSE");
            sprintf(message2, "Вы выиграли! Ваш выбор: %s", choice2);
            sendto(sockfd, message1, strlen(message1), 0,
                   (const struct sockaddr *)&firstAddr, sizeof(firstAddr));
            sendto(sockfd, message2, strlen(message2), 0,
                   (const struct sockaddr *)&secondAddr, sizeof(secondAddr));
            students[place].client_number = secondID;
            students[place].addr = secondAddr;
            place++;
            sprintf(observerMessage, "Студент с id %d выиграл", secondID);
            sendto(sockfd, observerMessage, strlen(observerMessage), 0, (const struct sockaddr *)&observeraddr, sizeof(observeraddr));
            break;
          } else {
            sprintf(message1, "Ничья! Ваши выборы: %s и %s", choice1, choice2);
            sendto(sockfd, message1, strlen(message1), 0,
                   (const struct sockaddr *)&firstAddr, sizeof(firstAddr));
            sendto(sockfd, message2, strlen(message2), 0,
                   (const struct sockaddr *)&secondAddr, sizeof(secondAddr));
            sprintf(observerMessage, "Ничья, переигровка");
            sendto(sockfd, observerMessage, strlen(observerMessage), 0, (const struct sockaddr *)&observeraddr, sizeof(observeraddr));
          }
        }
      }
    }
    round++;
    sleep(1);
  }
  printf("ТУРНИР ОКОНЧЕН\n");
  printf("ПОБЕДИТЕЛЬ СТУДЕНТ %d!!!!\n", students[0].client_number);

  sprintf(observerMessage, "ТУРНИР ОКОНЧЕН\nПОБЕДИТЕЛЬ СТУДЕНТ %d!!!!\n", students[0].client_number);
  sendto(sockfd, observerMessage, strlen(observerMessage), 0, (const struct sockaddr *)&observeraddr, sizeof(observeraddr));
  sprintf(observerMessage, "END");
  sendto(sockfd, observerMessage, strlen(observerMessage), 0, (const struct sockaddr *)&observeraddr, sizeof(observeraddr));

  memset(buffer, 0, sizeof(buffer));
  sprintf(buffer, "YOU WIN");
  sendto(sockfd, buffer, strlen(buffer), 0,
         (const struct sockaddr *)&students[0].addr, sizeof(students[0].addr));
  close(sockfd);
  return 0;
}