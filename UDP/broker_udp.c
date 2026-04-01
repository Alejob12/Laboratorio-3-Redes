#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 5000
#define BUFFER_SIZE 1024
#define MAX_SUBSCRIBERS 100
#define MAX_TOPIC 50

typedef struct {
    struct sockaddr_in addr;
    char topic[MAX_TOPIC];
    int active;
} Subscriber;

Subscriber subscribers[MAX_SUBSCRIBERS];

int same_client(struct sockaddr_in *a, struct sockaddr_in *b) {
    return (a->sin_addr.s_addr == b->sin_addr.s_addr) &&
           (a->sin_port == b->sin_port);
}

void add_subscriber(struct sockaddr_in client_addr, const char *topic) {
    for (int i = 0; i < MAX_SUBSCRIBERS; i++) {
        if (subscribers[i].active &&
            same_client(&subscribers[i].addr, &client_addr)) {
            strncpy(subscribers[i].topic, topic, MAX_TOPIC - 1);
            subscribers[i].topic[MAX_TOPIC - 1] = '\0';
            printf("Suscriptor actualizado -> IP: %s, Puerto: %d, Tema: %s\n",
                   inet_ntoa(client_addr.sin_addr),
                   ntohs(client_addr.sin_port),
                   subscribers[i].topic);
            return;
        }
    }

    for (int i = 0; i < MAX_SUBSCRIBERS; i++) {
        if (!subscribers[i].active) {
            subscribers[i].addr = client_addr;
            strncpy(subscribers[i].topic, topic, MAX_TOPIC - 1);
            subscribers[i].topic[MAX_TOPIC - 1] = '\0';
            subscribers[i].active = 1;

            printf("Nuevo suscriptor -> IP: %s, Puerto: %d, Tema: %s\n",
                   inet_ntoa(client_addr.sin_addr),
                   ntohs(client_addr.sin_port),
                   subscribers[i].topic);
            return;
        }
    }

    printf("No hay espacio para más suscriptores.\n");
}

void forward_message(int sockfd, const char *topic, const char *message) {
    char buffer[BUFFER_SIZE];

    snprintf(buffer, sizeof(buffer), "[%s] %s", topic, message);

    for (int i = 0; i < MAX_SUBSCRIBERS; i++) {
        if (subscribers[i].active && strcmp(subscribers[i].topic, topic) == 0) {
            sendto(sockfd, buffer, strlen(buffer), 0,
                   (struct sockaddr *)&subscribers[i].addr,
                   sizeof(subscribers[i].addr));

            printf("Mensaje reenviado a %s:%d -> %s\n",
                   inet_ntoa(subscribers[i].addr.sin_addr),
                   ntohs(subscribers[i].addr.sin_port),
                   buffer);
        }
    }
}

int main() {
    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];

    for (int i = 0; i < MAX_SUBSCRIBERS; i++) {
        subscribers[i].active = 0;
    }

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Error al crear el socket");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    memset(&client_addr, 0, sizeof(client_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error en bind");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Broker UDP escuchando en el puerto %d...\n", PORT);

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);

        int n = recvfrom(sockfd, buffer, BUFFER_SIZE - 1, 0,
                         (struct sockaddr *)&client_addr, &addr_len);
        if (n < 0) {
            perror("Error en recvfrom");
            continue;
        }

        buffer[n] = '\0';
        printf("Mensaje recibido de %s:%d -> %s\n",
               inet_ntoa(client_addr.sin_addr),
               ntohs(client_addr.sin_port),
               buffer);

        if (strncmp(buffer, "SUBSCRIBE|", 10) == 0) {
            char *topic = buffer + 10;
            add_subscriber(client_addr, topic);
        }
        else if (strncmp(buffer, "PUBLISH|", 8) == 0) {
            char *rest = buffer + 8;
            char *topic = strtok(rest, "|");
            char *message = strtok(NULL, "");

            if (topic != NULL && message != NULL) {
                forward_message(sockfd, topic, message);
            } else {
                printf("Formato inválido de publicación.\n");
            }
        }
        else {
            printf("Mensaje no reconocido.\n");
        }
    }

    close(sockfd);
    return 0;
}