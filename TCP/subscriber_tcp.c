/*
 * subscriber_tcp.c
 * ----------------
 * Suscriptor del sistema de noticias deportivas (versión TCP).
 *
 * Simula un aficionado que se suscribe a un partido y recibe en
 * su pantalla todas las actualizaciones enviadas por los periodistas.
 *
 * Uso:
 *   ./subscriber_tcp <ip_broker> <tema>
 *
 * Ejemplo:
 *   ./subscriber_tcp 127.0.0.1 RealMadrid-vs-Barcelona
 *
 * El suscriptor:
 *   1. Se conecta al broker (TCP, puerto 9000).
 *   2. Se registra enviando "SUB:<tema>\n".
 *   3. Espera y muestra los mensajes que el broker le reenvía.
 *   4. Termina cuando el broker cierra la conexión (partido finalizado)
 *      o cuando el usuario pulsa Ctrl+C.
 *
 * Compilar: gcc -o subscriber_tcp subscriber_tcp.c
 * Ejecutar: ./subscriber_tcp 127.0.0.1 RealMadrid-vs-Barcelona
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>       /* close()                          */
#include <sys/socket.h>   /* socket(), connect(), recv()      */
#include <netinet/in.h>   /* struct sockaddr_in, htons()      */
#include <arpa/inet.h>    /* inet_pton()                      */

/* ── Constantes ─────────────────────────────────────────────── */
#define BROKER_PORT  9000    /* Puerto del broker               */
#define BUF_SIZE     1024    /* Tamaño del buffer de recepción  */

/* ── Prototipo auxiliar ─────────────────────────────────────── */
static void print_banner(const char *tema);

/* ═══════════════════════════════════════════════════════════════
 *  MAIN
 * ═══════════════════════════════════════════════════════════════ */
int main(int argc, char *argv[])
{
    /* ── Validar argumentos ──────────────────────────────────── */
    if (argc != 3) {
        fprintf(stderr, "Uso: %s <ip_broker> <tema>\n", argv[0]);
        fprintf(stderr, "  ej: %s 127.0.0.1 RealMadrid-vs-Barcelona\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *broker_ip = argv[1];
    const char *tema      = argv[2];

    /* ── Crear socket TCP ────────────────────────────────────── */
    /*
     * AF_INET      → familia IPv4
     * SOCK_STREAM  → TCP (confiable, orientado a conexión, ordenado)
     * 0            → protocolo por defecto (TCP)
     */
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("[SUB] Error al crear socket");
        exit(EXIT_FAILURE);
    }

    /* ── Configurar dirección del broker ─────────────────────── */
    struct sockaddr_in broker_addr;
    memset(&broker_addr, 0, sizeof(broker_addr));
    broker_addr.sin_family = AF_INET;
    broker_addr.sin_port   = htons(BROKER_PORT);

    /*
     * inet_pton convierte la cadena de texto "127.0.0.1"
     * a representación binaria de red (32 bits, big-endian).
     */
    if (inet_pton(AF_INET, broker_ip, &broker_addr.sin_addr) <= 0) {
        perror("[SUB] Dirección IP inválida");
        close(sock);
        exit(EXIT_FAILURE);
    }

    /* ── Conectar al broker ──────────────────────────────────── */
    /*
     * connect() inicia el three-way handshake TCP:
     *   → SYN
     *   ← SYN-ACK
     *   → ACK
     * La conexión queda establecida y el canal es full-duplex.
     */
    if (connect(sock, (struct sockaddr *)&broker_addr, sizeof(broker_addr)) < 0) {
        perror("[SUB] Error al conectar con el broker");
        close(sock);
        exit(EXIT_FAILURE);
    }
    printf("[SUB] Conectado al broker %s:%d\n", broker_ip, BROKER_PORT);

    /* ── Registro: informar al broker que somos suscriptor ───── */
    /*
     * El broker espera como primer mensaje: "SUB:<tema>\n"
     * A partir de ese momento, el broker enviará a este socket
     * todos los mensajes cuyo tema coincida.
     */
    char reg_msg[256];
    snprintf(reg_msg, sizeof(reg_msg), "SUB:%s\n", tema);

    if (send(sock, reg_msg, strlen(reg_msg), 0) < 0) {
        perror("[SUB] Error al enviar registro");
        close(sock);
        exit(EXIT_FAILURE);
    }
    printf("[SUB] Suscripción registrada para el partido: \"%s\"\n", tema);

    /* Mostrar encabezado visual */
    print_banner(tema);

    /* ── Bucle de recepción de mensajes ──────────────────────── */
    /*
     * El suscriptor simplemente espera mensajes del broker.
     * recv() bloquea hasta que:
     *   a) Llegan datos nuevos  → los imprimimos.
     *   b) El broker cierra la conexión → recv devuelve 0.
     *   c) Error de red         → recv devuelve -1.
     *
     * TCP garantiza que los mensajes llegan:
     *   ✓ Completos (sin pérdida de bytes).
     *   ✓ En el mismo orden en que fueron enviados.
     *   ✓ Sin duplicados.
     */
    char buffer[BUF_SIZE];
    int  msg_count = 0;

    while (1) {
        int bytes = recv(sock, buffer, BUF_SIZE - 1, 0);

        if (bytes < 0) {
            perror("[SUB] Error al recibir");
            break;
        }

        if (bytes == 0) {
            /* El broker cerró la conexión (partido terminado) */
            printf("\n[SUB] Conexión cerrada por el broker. Fin de la transmisión.\n");
            break;
        }

        /* Aseguramos terminación de cadena */
        buffer[bytes] = '\0';

        /*
         * Un solo recv() podría contener múltiples mensajes si llegaron
         * juntos (segmentación TCP). Los separamos por '\n'.
         */
        char *line = buffer;
        char *newline;

        while ((newline = strchr(line, '\n')) != NULL) {
            *newline = '\0';            /* Cortar en el '\n'          */

            if (strlen(line) > 0) {    /* Ignorar líneas vacías      */
                msg_count++;
                printf("  [%03d] %s\n", msg_count, line);
                fflush(stdout);        /* Mostrar en pantalla de inmediato */
            }

            line = newline + 1;        /* Avanzar al siguiente mensaje */
        }
    }

    /* ── Cerrar conexión ─────────────────────────────────────── */
    /*
     * close() envía un segmento FIN para el cierre ordenado TCP.
     */
    printf("[SUB] Total de mensajes recibidos: %d\n", msg_count);
    printf("[SUB] Conexión finalizada.\n");
    close(sock);
    return 0;
}

/* ── Función auxiliar: imprime encabezado del partido ────────── */
static void print_banner(const char *tema)
{
    int len = strlen(tema);
    int width = len + 4;   /* Ancho de la caja */

    printf("\n");
    /* Línea superior */
    printf("  +");
    for (int i = 0; i < width; i++) printf("-");
    printf("+\n");

    /* Título */
    printf("  | 📺 %-*s |\n", width - 1, tema);

    /* Subtítulo */
    printf("  | %-*s |\n", width - 1, "Transmisión en VIVO");

    /* Línea inferior */
    printf("  +");
    for (int i = 0; i < width; i++) printf("-");
    printf("+\n\n");
}
