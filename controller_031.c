#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>

#define AGENT_IP "127.0.0.1"
#define AGENT_PORT 9410

#define UDP_PORT 9001

#define BUFFER_SIZE 4096
#define FILE_BUFFER_SIZE 4096

#define SID "1300"
#define AUTH_TOKEN "OPS-0031"

#define UPLOAD_FILE "test_upload.txt"
#define DOWNLOAD_FILE "downloaded_test.txt"


/*
 * ============================================================
 * SEND ALL
 * ============================================================
 */

int send_all(int socket_fd,
             const void *buffer,
             size_t length)
{
    size_t total_sent;

    const char *data;

    ssize_t sent;


    total_sent = 0;

    data = (const char *)buffer;


    while (total_sent < length)
    {
        sent = send(socket_fd,
                    data + total_sent,
                    length - total_sent,
                    0);


        if (sent <= 0)
        {
            return -1;
        }


        total_sent +=
            (size_t)sent;
    }


    return 0;
}


/*
 * ============================================================
 * RECEIVE LINE
 * ============================================================
 */

int recv_line(int socket_fd,
              char *buffer,
              size_t buffer_size)
{
    size_t position;

    char character;

    ssize_t received;


    position = 0;


    while (position < buffer_size - 1)
    {
        received =
            recv(socket_fd,
                 &character,
                 1,
                 0);


        if (received <= 0)
        {
            return -1;
        }


        buffer[position] =
            character;


        position++;


        if (character == '\n')
        {
            break;
        }
    }


    buffer[position] =
        '\0';


    return 0;
}


/*
 * ============================================================
 * SEND COMMAND
 * ============================================================
 */

int send_command(int socket_fd,
                 const char *command)
{
    char message[BUFFER_SIZE];


    snprintf(message,
             sizeof(message),
             "%s\n",
             command);


    printf("\nController -> Agent: %s\n",
           command);


    return send_all(socket_fd,
                    message,
                    strlen(message));
}


/*
 * ============================================================
 * RECEIVE RESPONSE
 * ============================================================
 */

int receive_response(int socket_fd,
                     char *response,
                     size_t response_size)
{
    if (recv_line(socket_fd,
                  response,
                  response_size) != 0)
    {
        return -1;
    }


    response[strcspn(response,
                     "\r\n")] = '\0';


    printf("Agent -> Controller: %s\n",
           response);


    return 0;
}


/*
 * ============================================================
 * SEND PUT FILE
 * ============================================================
 */

int send_put_file(int socket_fd,
                  const char *filename)
{
    FILE *file;


    file =
        fopen(filename,
              "rb");


    if (file == NULL)
    {
        perror("fopen");

        return -1;
    }


    if (fseek(file,
              0,
              SEEK_END) != 0)
    {
        fclose(file);

        return -1;
    }


    long size;


    size =
        ftell(file);


    if (size < 0)
    {
        fclose(file);

        return -1;
    }


    rewind(file);


    unsigned long long file_size;


    file_size =
        (unsigned long long)size;


    char header[BUFFER_SIZE];


    snprintf(header,
             sizeof(header),
             "PUT %s %llu\n",
             filename,
             file_size);


    printf("\nController -> Agent: PUT %s %llu\n",
           filename,
           file_size);


    if (send_all(socket_fd,
                 header,
                 strlen(header)) != 0)
    {
        fclose(file);

        return -1;
    }


    char buffer[FILE_BUFFER_SIZE];


    unsigned long long total_sent;


    total_sent = 0;


    while (total_sent < file_size)
    {
        size_t remaining;

        size_t bytes_to_read;


        remaining =
            (size_t)(file_size - total_sent);


        bytes_to_read =
            remaining;


        if (bytes_to_read >
            sizeof(buffer))
        {
            bytes_to_read =
                sizeof(buffer);
        }


        size_t bytes_read;


        bytes_read =
            fread(buffer,
                  1,
                  bytes_to_read,
                  file);


        if (bytes_read == 0)
        {
            fclose(file);

            return -1;
        }


        if (send_all(socket_fd,
                     buffer,
                     bytes_read) != 0)
        {
            fclose(file);

            return -1;
        }


        total_sent +=
            (unsigned long long)bytes_read;
    }


    fclose(file);


    char response[BUFFER_SIZE];


    if (receive_response(socket_fd,
                         response,
                         sizeof(response)) != 0)
    {
        return -1;
    }


    return 0;
}


/*
 * ============================================================
 * GET FILE
 * ============================================================
 */

int send_get_file(int socket_fd,
                  const char *filename,
                  const char *output_filename)
{
    char command[BUFFER_SIZE];


    snprintf(command,
             sizeof(command),
             "GET %s",
             filename);


    if (send_command(socket_fd,
                     command) != 0)
    {
        return -1;
    }


    char header[BUFFER_SIZE];


    if (recv_line(socket_fd,
                  header,
                  sizeof(header)) != 0)
    {
        return -1;
    }


    header[strcspn(header,
                   "\r\n")] = '\0';


    printf("Agent -> Controller: %s\n",
           header);


    if (strncmp(header,
                "OK FILE_SEND ",
                13) != 0)
    {
        return -1;
    }


    char received_filename[1024];

    unsigned long long file_size;

    char received_sid[64];


    memset(received_filename,
           0,
           sizeof(received_filename));


    memset(received_sid,
           0,
           sizeof(received_sid));


    file_size = 0;


    int parsed;


    parsed =
        sscanf(header,
               "OK FILE_SEND %1023s %llu SID:%63s",
               received_filename,
               &file_size,
               received_sid);


    if (parsed != 3)
    {
        printf("Invalid GET response.\n");

        return -1;
    }


    FILE *file;


    file =
        fopen(output_filename,
              "wb");


    if (file == NULL)
    {
        perror("fopen");

        return -1;
    }


    char buffer[FILE_BUFFER_SIZE];


    unsigned long long total_received;


    total_received = 0;


    while (total_received < file_size)
    {
        size_t remaining;

        size_t bytes_to_receive;


        remaining =
            (size_t)(file_size - total_received);


        bytes_to_receive =
            remaining;


        if (bytes_to_receive >
            sizeof(buffer))
        {
            bytes_to_receive =
                sizeof(buffer);
        }


        size_t position;


        position = 0;


        while (position < bytes_to_receive)
        {
            ssize_t received;


            received =
                recv(socket_fd,
                     buffer + position,
                     bytes_to_receive - position,
                     0);


            if (received <= 0)
            {
                fclose(file);

                return -1;
            }


            position +=
                (size_t)received;
        }


        size_t written;


        written =
            fwrite(buffer,
                   1,
                   bytes_to_receive,
                   file);


        if (written != bytes_to_receive)
        {
            fclose(file);

            return -1;
        }


        total_received +=
            (unsigned long long)bytes_to_receive;
    }


    fclose(file);


    printf(
        "GET completed: %s -> %s (%llu bytes)\n",
        filename,
        output_filename,
        file_size);


    return 0;
}


/*
 * ============================================================
 * RECEIVE UDP MONITORING PACKETS
 * ============================================================
 */

int receive_monitoring_packets(int udp_socket,
                               int packet_count)
{
    int count;


    for (count = 1;
         count <= packet_count;
         count++)
    {
        char packet[BUFFER_SIZE];


        struct sockaddr_in sender_address;


        socklen_t sender_length;


        sender_length =
            sizeof(sender_address);


        memset(&sender_address,
               0,
               sizeof(sender_address));


        ssize_t received;


        received =
            recvfrom(
                udp_socket,
                packet,
                sizeof(packet) - 1,
                0,
                (struct sockaddr *)&sender_address,
                &sender_length);


        if (received < 0)
        {
            perror("recvfrom");

            return -1;
        }


        packet[received] =
            '\0';


        printf(
            "\nUDP monitoring packet %d:\n",
            count);


        printf(
            "Agent -> Controller UDP: %s\n",
            packet);


        /*
         * Verify SID.
         */

        if (strstr(packet,
                   "SID:1300") != NULL)
        {
            printf(
                "SID verified: SID:1300\n");
        }
        else
        {
            printf(
                "SID verification FAILED.\n");
        }


        /*
         * Extract CPU load, memory MB and uptime.
         */

        char cpu_load[64];

        unsigned long long memory_mb;

        unsigned long long uptime_seconds;

        char received_sid[64];


        memset(cpu_load,
               0,
               sizeof(cpu_load));


        memset(received_sid,
               0,
               sizeof(received_sid));


        memory_mb = 0;

        uptime_seconds = 0;


        int parsed;


        parsed =
            sscanf(packet,
                   "SYSINFO %63s %llu %llu SID:%63s",
                   cpu_load,
                   &memory_mb,
                   &uptime_seconds,
                   received_sid);


        if (parsed == 4)
        {
            printf(
                "CPU Load: %s\n",
                cpu_load);


            printf(
                "Memory Used: %llu MB\n",
                memory_mb);


            printf(
                "Uptime: %llu seconds\n",
                uptime_seconds);
        }
    }


    return 0;
}


/*
 * ============================================================
 * MAIN
 * ============================================================
 */

int main(void)
{
    /*
     * --------------------------------------------------------
     * Create TCP socket.
     * --------------------------------------------------------
     */

    int tcp_socket;


    tcp_socket =
        socket(AF_INET,
               SOCK_STREAM,
               0);


    if (tcp_socket < 0)
    {
        perror("socket");

        return 1;
    }


    /*
     * --------------------------------------------------------
     * Agent address.
     * --------------------------------------------------------
     */

    struct sockaddr_in agent_address;


    memset(&agent_address,
           0,
           sizeof(agent_address));


    agent_address.sin_family =
        AF_INET;


    agent_address.sin_port =
        htons(AGENT_PORT);


    if (inet_pton(AF_INET,
                  AGENT_IP,
                  &agent_address.sin_addr) <= 0)
    {
        perror("inet_pton");

        close(tcp_socket);

        return 1;
    }


    /*
     * --------------------------------------------------------
     * Connect.
     * --------------------------------------------------------
     */

    printf(
        "Connecting to RemoteOps Agent...\n");


    if (connect(tcp_socket,
                (struct sockaddr *)&agent_address,
                sizeof(agent_address)) < 0)
    {
        perror("connect");

        close(tcp_socket);

        return 1;
    }


    printf(
        "Connected to Agent %s:%d\n",
        AGENT_IP,
        AGENT_PORT);


    char response[BUFFER_SIZE];


    /*
     * --------------------------------------------------------
     * HELLO
     * --------------------------------------------------------
     */

    send_command(tcp_socket,
                 "HELLO");


    receive_response(tcp_socket,
                     response,
                     sizeof(response));


    /*
     * --------------------------------------------------------
     * AUTH
     * --------------------------------------------------------
     */

    char auth_command[128];


    snprintf(auth_command,
             sizeof(auth_command),
             "AUTH %s",
             AUTH_TOKEN);


    send_command(tcp_socket,
                 auth_command);


    receive_response(tcp_socket,
                     response,
                     sizeof(response));


    /*
     * --------------------------------------------------------
     * SYSINFO
     * --------------------------------------------------------
     */

    send_command(tcp_socket,
                 "SYSINFO");


    receive_response(tcp_socket,
                     response,
                     sizeof(response));


    /*
     * --------------------------------------------------------
     * LISTPROC
     * --------------------------------------------------------
     */

    send_command(tcp_socket,
                 "LISTPROC");


    receive_response(tcp_socket,
                     response,
                     sizeof(response));


    /*
     * --------------------------------------------------------
     * EXEC DATE
     * --------------------------------------------------------
     */

    send_command(tcp_socket,
                 "EXEC DATE");


    receive_response(tcp_socket,
                     response,
                     sizeof(response));


    /*
     * --------------------------------------------------------
     * EXEC UPTIME
     * --------------------------------------------------------
     */

    send_command(tcp_socket,
                 "EXEC UPTIME");


    receive_response(tcp_socket,
                     response,
                     sizeof(response));


    /*
     * --------------------------------------------------------
     * EXEC DISKFREE
     * --------------------------------------------------------
     */

    send_command(tcp_socket,
                 "EXEC DISKFREE");


    receive_response(tcp_socket,
                     response,
                     sizeof(response));


    /*
     * --------------------------------------------------------
     * EXEC HOSTNAME
     * --------------------------------------------------------
     */

    send_command(tcp_socket,
                 "EXEC HOSTNAME");


    receive_response(tcp_socket,
                     response,
                     sizeof(response));


    /*
     * --------------------------------------------------------
     * EXEC WHOAMI
     * --------------------------------------------------------
     */

    send_command(tcp_socket,
                 "EXEC WHOAMI");


    receive_response(tcp_socket,
                     response,
                     sizeof(response));


    /*
     * --------------------------------------------------------
     * Invalid EXEC test
     * --------------------------------------------------------
     */

    send_command(tcp_socket,
                 "EXEC LS");


    receive_response(tcp_socket,
                     response,
                     sizeof(response));


    /*
     * --------------------------------------------------------
     * PUT TEST
     * --------------------------------------------------------
     */

    printf(
        "\n========== PUT TEST ==========\n");


    send_put_file(tcp_socket,
                  UPLOAD_FILE);


    /*
     * --------------------------------------------------------
     * GET TEST
     * --------------------------------------------------------
     */

    printf(
        "\n========== GET TEST ==========\n");


    send_get_file(tcp_socket,
                  UPLOAD_FILE,
                  DOWNLOAD_FILE);


    /*
     * --------------------------------------------------------
     * Create UDP socket.
     * --------------------------------------------------------
     */

    printf(
        "\n========== UDP MONITOR TEST ==========\n");


    int udp_socket;


    udp_socket =
        socket(AF_INET,
               SOCK_DGRAM,
               0);


    if (udp_socket < 0)
    {
        perror("UDP socket");

        close(tcp_socket);

        return 1;
    }


    /*
     * Bind UDP port 9001.
     */

    struct sockaddr_in udp_address;


    memset(&udp_address,
           0,
           sizeof(udp_address));


    udp_address.sin_family =
        AF_INET;


    udp_address.sin_addr.s_addr =
        htonl(INADDR_ANY);


    udp_address.sin_port =
        htons(UDP_PORT);


    if (bind(udp_socket,
             (struct sockaddr *)&udp_address,
             sizeof(udp_address)) < 0)
    {
        perror("UDP bind");

        close(udp_socket);

        close(tcp_socket);

        return 1;
    }


    /*
     * --------------------------------------------------------
     * MONITOR START
     * --------------------------------------------------------
     */

    char monitor_command[128];


    snprintf(monitor_command,
             sizeof(monitor_command),
             "MONITOR START %d",
             UDP_PORT);


    send_command(tcp_socket,
                 monitor_command);


    receive_response(tcp_socket,
                     response,
                     sizeof(response));


    /*
     * Receive three UDP monitoring packets.
     */

    receive_monitoring_packets(
        udp_socket,
        3);


    /*
     * --------------------------------------------------------
     * MONITOR STOP
     * --------------------------------------------------------
     */

    send_command(tcp_socket,
                 "MONITOR STOP");


    receive_response(tcp_socket,
                     response,
                     sizeof(response));


    /*
     * --------------------------------------------------------
     * QUIT
     * --------------------------------------------------------
     */

    printf(
        "\n========== QUIT ==========\n");


    send_command(tcp_socket,
                 "QUIT");


    receive_response(tcp_socket,
                     response,
                     sizeof(response));


    close(udp_socket);

    close(tcp_socket);


    printf(
        "\nController finished successfully.\n");


    return 0;
}
