# Sistema de Noticias Deportivas — TCP y UDP

Sistema pub-sub en C que simula cobertura en vivo de partidos de futbol.
Un Publisher envia eventos, el Broker los recibe y los reenvía a los Subscribers suscritos al mismo partido.

---

## Librerias

Ambas versiones usan solo cabeceras POSIX del sistema, sin librerias externas.

| Cabecera | TCP | UDP |
|---|---|---|
| sys/socket.h | socket, bind, listen, accept, connect, send, recv | socket, bind |
| sys/select.h | select (multiplexar clientes) | no se usa |
| netinet/in.h | struct sockaddr_in, htons | struct sockaddr_in, htons |
| arpa/inet.h | inet_pton, inet_ntoa | inet_addr, sendto, recvfrom |
| unistd.h | close, sleep | close |

---

## TCP vs UDP

| Criterio | TCP | UDP |
|---|---|---|
| Conexion previa | Si (handshake) | No |
| Mensajes garantizados | Si | No |
| Orden garantizado | Si | No |
| Velocidad | Mas lento | Mas rapido |
| Cabecera | 20 bytes | 8 bytes |

TCP establece un canal dedicado por cliente. El broker usa select() para
atender varios a la vez. UDP no tiene canal; el subscriber debe registrar
su IP y puerto para que el broker sepa donde enviarle los mensajes.

---

## Compilar

```bash
gcc -Wall -o broker_tcp broker_tcp.c
gcc -Wall -o publisher_tcp publisher_tcp.c
gcc -Wall -o subscriber_tcp subscriber_tcp.c

gcc -Wall -o broker_udp broker_udp.c
gcc -Wall -o publisher_udp publisher_udp.c
gcc -Wall -o subscriber_udp subscriber_udp.c
```

---

## Ejecutar TCP (puerto 9000)

```bash
./broker_tcp
./subscriber_tcp 127.0.0.1 RealMadrid-vs-Barcelona
./publisher_tcp  127.0.0.1 RealMadrid-vs-Barcelona
```

## Ejecutar UDP (puerto 5000)

```bash
./broker_udp
./subscriber_udp   # pide el tema por teclado
./publisher_udp    # pide tema y mensajes por teclado, 'salir' para terminar
```

Nota UDP: si corres dos subscribers en la misma maquina, cambia MY_PORT
en subscriber_udp.c (ej: 6001, 6002) porque no pueden compartir el mismo puerto.

---

## Wireshark

Interfaz: Loopback: lo

```
Filtro TCP:  tcp.port == 9000
Filtro UDP:  udp.port == 5000
```

Guardar como tcp_pubsub.pcap y udp_pubsub.pcap.
