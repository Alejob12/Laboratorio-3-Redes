#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 5000
#define BUFFER_SIZE 1024

int main() {
    int sockfd;
    struct sockaddr_in broker_addr;
    char topic[50];
    char message[900];
    char buffer[BUFFER_SIZE];

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Error al crear el socket");
        exit(EXIT_FAILURE);
    }

    memset(&broker_addr, 0, sizeof(broker_addr));
    broker_addr.sin_family = AF_INET;
    broker_addr.sin_port = htons(PORT);
    broker_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    printf("Ingrese el tema (ej: partido1): ");
    fgets(topic, sizeof(topic), stdin);
    topic[strcspn(topic, "\n")] = '\0';

    printf("Escriba mensajes. Escriba 'salir' para terminar.\n");

    while (1) {
        printf("Mensaje: ");
        fgets(message, sizeof(message), stdin);
        message[strcspn(message, "\n")] = '\0';

        if (strcmp(message, "salir") == 0) break;

        snprintf(buffer, sizeof(buffer), "PUBLISH|%s|%s", topic, message);

        sendto(sockfd, buffer, strlen(buffer), 0,
               (struct sockaddr *)&broker_addr, sizeof(broker_addr));

        printf("Mensaje enviado\n");
    }

    close(sockfd);
    return 0;
}