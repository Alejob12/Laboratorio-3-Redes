/*
 * publisher_tcp.c
 * ---------------
 * Publicador del sistema de noticias deportivas (versión TCP).
 *
 * Simula un periodista deportivo que envía en vivo los eventos
 * de un partido al broker, el cual los redistribuye a los aficionados.
 *
 * Uso:
 *   ./publisher_tcp <ip_broker> <tema>
 *
 * Ejemplo:
 *   ./publisher_tcp 127.0.0.1 RealMadrid-vs-Barcelona
 *
 * El publicador:
 *   1. Se conecta al broker (TCP, puerto 9000).
 *   2. Se registra enviando "PUB:<tema>\n".
 *   3. Envía 12 mensajes deportivos pregrabados con una pausa entre cada uno.
 *   4. Cierra la conexión.
 *
 * Compilar: gcc -o publisher_tcp publisher_tcp.c
 * Ejecutar: ./publisher_tcp 127.0.0.1 RealMadrid-vs-Barcelona
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>          /* close(), sleep()               */
#include <sys/socket.h>      /* socket(), connect(), send()    */
#include <netinet/in.h>      /* struct sockaddr_in, htons()    */
#include <arpa/inet.h>       /* inet_pton()                    */

/* ── Constantes ─────────────────────────────────────────────── */
#define BROKER_PORT  9000    /* Puerto del broker               */
#define BUF_SIZE     512     /* Tamaño de buffer para mensajes  */
#define SEND_DELAY   2       /* Segundos entre mensajes         */

/* ── Mensajes deportivos de ejemplo ─────────────────────────── */
/*
 * Estos 12 eventos simulan la cobertura en vivo de un partido.
 * En un sistema real se leerían desde stdin o un archivo.
 */
static const char *eventos[] = {
    "⚽ Min  3 - Inicio del partido, primer saque de centro.",
    "⚽ Min 12 - Disparo al arco del equipo local, para el portero.",
    "⚽ Min 20 - GOL del equipo LOCAL. Cabezazo de cabeza al ángulo.",
    "⚽ Min 28 - Tarjeta AMARILLA al número 5 del equipo visitante.",
    "⚽ Min 35 - Cambio equipo visitante: entra #18, sale #9.",
    "⚽ Min 40 - GOL del equipo VISITANTE. Contragolpe letal.",
    "⚽ Min 45+2 - FIN del primer tiempo. Marcador: 1 - 1.",
    "⚽ Min 50 - Inicio del segundo tiempo.",
    "⚽ Min 63 - Tarjeta ROJA al número 3 del equipo local.",
    "⚽ Min 75 - GOL del equipo VISITANTE. Tiro libre al palo izquierdo.",
    "⚽ Min 88 - Cambio equipo local: entra #21, sale #7.",
    "⚽ Min 90+3 - FIN DEL PARTIDO. Resultado final: 1 - 2."
};

#define NUM_EVENTOS  (int)(sizeof(eventos) / sizeof(eventos[0]))

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
     * SOCK_STREAM  → TCP (flujo de bytes, confiable, ordenado)
     * 0            → protocolo por defecto para SOCK_STREAM (TCP)
     */
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("[PUB] Error al crear socket");
        exit(EXIT_FAILURE);
    }

    /* ── Configurar dirección del broker ─────────────────────── */
    struct sockaddr_in broker_addr;
    memset(&broker_addr, 0, sizeof(broker_addr));
    broker_addr.sin_family = AF_INET;
    broker_addr.sin_port   = htons(BROKER_PORT);

    /*
     * inet_pton: convierte la IP en texto ("127.0.0.1")
     * a formato binario de red (big-endian de 32 bits).
     */
    if (inet_pton(AF_INET, broker_ip, &broker_addr.sin_addr) <= 0) {
        perror("[PUB] Dirección IP inválida");
        close(sock);
        exit(EXIT_FAILURE);
    }

    /* ── Conectar al broker ──────────────────────────────────── */
    /*
     * connect() realiza el three-way handshake TCP:
     *   SYN  → broker
     *   SYN-ACK ← broker
     *   ACK  → broker
     * Después de esto la conexión queda establecida.
     */
    if (connect(sock, (struct sockaddr *)&broker_addr, sizeof(broker_addr)) < 0) {
        perror("[PUB] Error al conectar con el broker");
        close(sock);
        exit(EXIT_FAILURE);
    }
    printf("[PUB] Conectado al broker %s:%d\n", broker_ip, BROKER_PORT);

    /* ── Registro: informar al broker que somos publicador ───── */
    /*
     * El broker espera como primer mensaje: "PUB:<tema>\n"
     * Este mensaje define el tema (partido) que cubrirá este publicador.
     */
    char reg_msg[BUF_SIZE];
    snprintf(reg_msg, sizeof(reg_msg), "PUB:%s\n", tema);

    if (send(sock, reg_msg, strlen(reg_msg), 0) < 0) {
        perror("[PUB] Error al enviar registro");
        close(sock);
        exit(EXIT_FAILURE);
    }
    printf("[PUB] Registrado en el broker como publicador del tema: \"%s\"\n\n", tema);

    /* ── Enviar eventos del partido ──────────────────────────── */
    printf("[PUB] Iniciando transmisión de %d eventos...\n\n", NUM_EVENTOS);

    char msg[BUF_SIZE];
    for (int i = 0; i < NUM_EVENTOS; i++) {

        /* Construir el mensaje con salto de línea al final */
        snprintf(msg, sizeof(msg), "%s\n", eventos[i]);

        /*
         * send() escribe datos en el socket TCP.
         * TCP garantiza que los bytes llegarán al destino
         * de forma confiable y en orden.
         */
        if (send(sock, msg, strlen(msg), 0) < 0) {
            perror("[PUB] Error al enviar mensaje");
            break;
        }

        printf("[PUB] Enviado (%d/%d): %s\n", i + 1, NUM_EVENTOS, eventos[i]);

        /* Pausa entre eventos para simular tiempo real */
        sleep(SEND_DELAY);
    }

    /* ── Cerrar conexión ─────────────────────────────────────── */
    /*
     * Al llamar close(), TCP envía un segmento FIN al broker,
     * lo que indica que este extremo terminó de enviar datos.
     * Se completa el four-way handshake de cierre.
     */
    printf("\n[PUB] Transmisión finalizada. Cerrando conexión.\n");
    close(sock);
    return 0;
}
