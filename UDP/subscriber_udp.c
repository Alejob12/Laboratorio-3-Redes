#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define BROKER_PORT 5000
#define MY_PORT 6001
#define BUFFER_SIZE 1024

int main() {
    int sockfd;
    struct sockaddr_in my_addr, broker_addr, sender_addr;
    socklen_t sender_len = sizeof(sender_addr);
    char topic[50];
    char buffer[BUFFER_SIZE];

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Error al crear el socket");
        exit(EXIT_FAILURE);
    }

    memset(&my_addr, 0, sizeof(my_addr));
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(MY_PORT);
    my_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (struct sockaddr *)&my_addr, sizeof(my_addr)) < 0) {
        perror("Error en bind del subscriber");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    memset(&broker_addr, 0, sizeof(broker_addr));
    broker_addr.sin_family = AF_INET;
    broker_addr.sin_port = htons(BROKER_PORT);
    broker_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    printf("Ingrese el tema al que se desea suscribir: ");
    fgets(topic, sizeof(topic), stdin);
    topic[strcspn(topic, "\n")] = '\0';

    snprintf(buffer, sizeof(buffer), "SUBSCRIBE|%s", topic);

    if (sendto(sockfd, buffer, strlen(buffer), 0,
               (struct sockaddr *)&broker_addr, sizeof(broker_addr)) < 0) {
        perror("Error enviando suscripción");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Suscrito al tema '%s'. Escuchando mensajes en el puerto %d...\n", topic, MY_PORT);

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);

        int n = recvfrom(sockfd, buffer, BUFFER_SIZE - 1, 0,
                         (struct sockaddr *)&sender_addr, &sender_len);
        if (n < 0) {
            perror("Error en recvfrom");
            continue;
        }

        buffer[n] = '\0';
        printf("Recibido: %s\n", buffer);
    }

    close(sockfd);
    return 0;
}
