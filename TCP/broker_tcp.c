/*
 * broker_tcp.c
 * ------------
 * Nodo central del sistema de noticias deportivas (versión TCP).
 *
 * Responsabilidades:
 *   1. Aceptar conexiones de publicadores y suscriptores.
 *   2. Registrar a cada cliente según su rol y el partido que le interesa.
 *   3. Reenviar cada mensaje recibido de un publicador a todos los
 *      suscriptores suscritos al mismo partido (tema).
 *
 * Protocolo de registro (primer mensaje que envía cada cliente):
 *   Publicador  → "PUB:<tema>\n"   ej. "PUB:RealMadrid-vs-Barcelona\n"
 *   Suscriptor  → "SUB:<tema>\n"   ej. "SUB:RealMadrid-vs-Barcelona\n"
 *
 * Mensajes del publicador (después del registro):
 *   "<contenido>\n"   ej. "Gol de Real Madrid al minuto 45\n"
 *
 * Mensajes que reenvía el broker al suscriptor:
 *   "[<tema>] <contenido>\n"
 *
 * Puerto de escucha: 9000
 *
 * Compilar: gcc -o broker_tcp broker_tcp.c
 * Ejecutar: ./broker_tcp
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>   /* socket(), bind(), listen(), accept() */
#include <sys/select.h>   /* select(), fd_set, FD_*              */
#include <netinet/in.h>   /* struct sockaddr_in, htons()          */
#include <arpa/inet.h>    /* inet_ntoa()                          */

/* ── Constantes ─────────────────────────────────────────────── */
#define PORT        9000   /* Puerto en que escucha el broker     */
#define MAX_CLIENTS 50     /* Máximo de clientes simultáneos      */
#define BUF_SIZE    1024   /* Tamaño del buffer de lectura        */
#define TOPIC_SIZE  128    /* Tamaño máximo del nombre del tema   */

/* ── Tipos ───────────────────────────────────────────────────── */
typedef enum {
    UNKNOWN    = 0,   /* Rol aún no definido (recién conectado)   */
    PUBLISHER  = 1,   /* Publicador: envía noticias del partido   */
    SUBSCRIBER = 2    /* Suscriptor: recibe noticias del partido  */
} ClientType;

/* Información de cada cliente conectado */
typedef struct {
    int        fd;              /* Descriptor del socket         */
    ClientType type;            /* Rol: UNKNOWN / PUBLISHER / SUBSCRIBER */
    char       topic[TOPIC_SIZE]; /* Partido al que pertenece    */
} Client;

/* ── Prototipos ──────────────────────────────────────────────── */
static void remove_newline(char *s);
static int  find_free_slot(Client clients[], int max);
static void forward_to_subscribers(Client clients[], int max,
                                   const char *topic, const char *msg);

/* ═══════════════════════════════════════════════════════════════
 *  MAIN
 * ═══════════════════════════════════════════════════════════════ */
int main(void)
{
    int server_fd;                       /* Socket de escucha del servidor */
    int new_fd;                          /* Socket de cada nueva conexión  */
    struct sockaddr_in server_addr;      /* Dirección del servidor         */
    struct sockaddr_in client_addr;      /* Dirección del cliente entrante */
    socklen_t addr_len = sizeof(client_addr);

    Client clients[MAX_CLIENTS];        /* Tabla de clientes              */
    fd_set  read_fds;                   /* Set de descriptores para select*/
    int     max_fd;                     /* Mayor fd activo                */
    char    buffer[BUF_SIZE];           /* Buffer de lectura              */
    int     opt = 1;                    /* Opción SO_REUSEADDR            */

    /* ── Inicializar tabla de clientes ─────────────────────────── */
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].fd   = -1;
        clients[i].type = UNKNOWN;
        memset(clients[i].topic, 0, TOPIC_SIZE);
    }

    /* ── Crear socket TCP ───────────────────────────────────────── */
    /*
     * AF_INET   → familia IPv4
     * SOCK_STREAM → TCP (orientado a conexión, confiable)
     * 0         → protocolo por defecto (TCP)
     */
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("[BROKER] Error al crear socket");
        exit(EXIT_FAILURE);
    }

    /* Permitir reutilizar el puerto inmediatamente tras reiniciar */
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    /* ── Configurar dirección del servidor ──────────────────────── */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family      = AF_INET;        /* IPv4              */
    server_addr.sin_addr.s_addr = INADDR_ANY;     /* Cualquier interfaz*/
    server_addr.sin_port        = htons(PORT);    /* Puerto en orden de red */

    /* ── Bind: asociar socket a la dirección ────────────────────── */
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("[BROKER] Error en bind");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    /* ── Listen: poner socket en modo de escucha ────────────────── */
    /* El segundo argumento (10) es el tamaño de la cola de conexiones pendientes */
    if (listen(server_fd, 10) < 0) {
        perror("[BROKER] Error en listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("[BROKER] Iniciado. Escuchando en puerto %d...\n\n", PORT);

    /* ══════════════════════════════════════════════════════════════
     *  Bucle principal: select() multiplexado (sin hilos)
     *  select() bloquea hasta que algún fd esté listo para leer.
     * ══════════════════════════════════════════════════════════════ */
    while (1) {

        /* Construir el set de descriptores a monitorear */
        FD_ZERO(&read_fds);
        FD_SET(server_fd, &read_fds);   /* Siempre monitorear nuevas conexiones */
        max_fd = server_fd;

        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].fd > 0) {
                FD_SET(clients[i].fd, &read_fds);
                if (clients[i].fd > max_fd)
                    max_fd = clients[i].fd;
            }
        }

        /* Esperar actividad en algún descriptor (sin timeout) */
        if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) < 0) {
            perror("[BROKER] Error en select");
            break;
        }

        /* ── ¿Hay una nueva conexión entrante? ────────────────────── */
        if (FD_ISSET(server_fd, &read_fds)) {
            new_fd = accept(server_fd,
                            (struct sockaddr *)&client_addr,
                            &addr_len);
            if (new_fd < 0) {
                perror("[BROKER] Error en accept");
            } else {
                int slot = find_free_slot(clients, MAX_CLIENTS);
                if (slot == -1) {
                    printf("[BROKER] Sin espacio para nuevos clientes.\n");
                    close(new_fd);
                } else {
                    clients[slot].fd   = new_fd;
                    clients[slot].type = UNKNOWN;
                    memset(clients[slot].topic, 0, TOPIC_SIZE);
                    printf("[BROKER] Nueva conexión (fd=%d) desde %s\n",
                           new_fd, inet_ntoa(client_addr.sin_addr));
                }
            }
        }

        /* ── Revisar mensajes de clientes ya conectados ─────────── */
        for (int i = 0; i < MAX_CLIENTS; i++) {

            if (clients[i].fd <= 0)               continue;
            if (!FD_ISSET(clients[i].fd, &read_fds)) continue;

            /* Leer datos del cliente */
            int bytes = recv(clients[i].fd, buffer, BUF_SIZE - 1, 0);

            /* bytes == 0 → el cliente cerró la conexión */
            if (bytes <= 0) {
                printf("[BROKER] Cliente fd=%d desconectado (tema: %s)\n",
                       clients[i].fd,
                       clients[i].type == UNKNOWN ? "sin registrar" : clients[i].topic);
                close(clients[i].fd);
                clients[i].fd   = -1;
                clients[i].type = UNKNOWN;
                continue;
            }

            buffer[bytes] = '\0';
            remove_newline(buffer);   /* Eliminar '\n' o '\r\n' al final */

            /* ── Primer mensaje: registro del cliente ──────────────
             * Formato esperado: "PUB:<tema>" o "SUB:<tema>"
             * ──────────────────────────────────────────────────── */
            if (clients[i].type == UNKNOWN) {

                if (strncmp(buffer, "PUB:", 4) == 0) {
                    clients[i].type = PUBLISHER;
                    strncpy(clients[i].topic, buffer + 4, TOPIC_SIZE - 1);
                    printf("[BROKER] PUBLICADOR registrado → tema: \"%s\" (fd=%d)\n",
                           clients[i].topic, clients[i].fd);

                } else if (strncmp(buffer, "SUB:", 4) == 0) {
                    clients[i].type = SUBSCRIBER;
                    strncpy(clients[i].topic, buffer + 4, TOPIC_SIZE - 1);
                    printf("[BROKER] SUSCRIPTOR registrado → tema: \"%s\" (fd=%d)\n",
                           clients[i].topic, clients[i].fd);

                } else {
                    printf("[BROKER] Mensaje de registro inválido (fd=%d): \"%s\"\n",
                           clients[i].fd, buffer);
                }

            /* ── Mensajes subsiguientes de un publicador ───────────
             * Reenviar a todos los suscriptores del mismo tema.
             * ──────────────────────────────────────────────────── */
            } else if (clients[i].type == PUBLISHER) {

                printf("[BROKER] Publicador [%s]: %s\n", clients[i].topic, buffer);
                forward_to_subscribers(clients, MAX_CLIENTS,
                                       clients[i].topic, buffer);
            }
            /* (Los suscriptores no envían mensajes en este diseño) */
        }
    }

    close(server_fd);
    return 0;
}

/* ═══════════════════════════════════════════════════════════════
 *  Funciones auxiliares
 * ═══════════════════════════════════════════════════════════════ */

/* Elimina el carácter de nueva línea al final de la cadena.
 * Corta en el PRIMER \n para evitar que TCP entregue dos mensajes
 * juntos en un solo recv() y contamine el topic con el primer evento. */
static void remove_newline(char *s)
{
    char *p = strchr(s, '\n');   /* Busca el primer \n               */
    if (p) *p = '\0';            /* Corta ahí                        */
    p = strchr(s, '\r');         /* También elimina \r si existe     */
    if (p) *p = '\0';
}

/* Devuelve el índice del primer slot libre, o -1 si no hay */
static int find_free_slot(Client clients[], int max)
{
    for (int i = 0; i < max; i++)
        if (clients[i].fd == -1)
            return i;
    return -1;
}

/* Construye el mensaje con formato "[tema] contenido" y lo envía
 * a todos los suscriptores cuyo tema coincida con el del publicador */
static void forward_to_subscribers(Client clients[], int max,
                                   const char *topic, const char *msg)
{
    char fwd[BUF_SIZE + TOPIC_SIZE + 8];
    snprintf(fwd, sizeof(fwd), "[%s] %s\n", topic, msg);

    int count = 0;
    for (int j = 0; j < max; j++) {
        if (clients[j].fd > 0
            && clients[j].type == SUBSCRIBER
            && strcmp(clients[j].topic, topic) == 0)
        {
            send(clients[j].fd, fwd, strlen(fwd), 0);
            count++;
        }
    }

    if (count == 0)
        printf("[BROKER] (sin suscriptores para el tema \"%s\")\n", topic);
}
