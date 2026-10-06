#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>

#define SERVER_PORT 9410
#define SID "1300"
#define AUTH_TOKEN "OPS-0031"

#define BUFFER_SIZE 4096
#define FILE_BUFFER_SIZE 4096
#define MAX_FILENAME 256
#define MAX_FILE_SIZE (10ULL * 1024ULL * 1024ULL)

#define MONITOR_INTERVAL 5

#define LOG_FILE "remoteops_IT24100031.log"
#define STORAGE_DIRECTORY "./agentfiles/IT24100031"


typedef struct
{
    int running;
    int udp_socket;
    char controller_ip[INET_ADDRSTRLEN];
    int udp_port;
    pthread_t thread_id;

} MonitorData;


/*
 * ============================================================
 * TIMESTAMP LOGGING
 * ============================================================
 */

void write_log(const char *event)
{
    FILE *log_file;
    time_t current_time;
    struct tm time_info;
    char timestamp[64];

    log_file = fopen(LOG_FILE, "a");

    if (log_file == NULL)
    {
        return;
    }

    current_time = time(NULL);

    if (localtime_r(&current_time,
                    &time_info) == NULL)
    {
        fclose(log_file);
        return;
    }

    strftime(timestamp,
             sizeof(timestamp),
             "%Y-%m-%d %H:%M:%S",
             &time_info);

    fprintf(log_file,
            "[%s] %s\n",
            timestamp,
            event);

    fflush(log_file);

    fclose(log_file);
}


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

        total_sent += (size_t)sent;
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
        received = recv(socket_fd,
                        &character,
                        1,
                        0);

        if (received <= 0)
        {
            return -1;
        }

        buffer[position] = character;
        position++;

        if (character == '\n')
        {
            break;
        }
    }

    buffer[position] = '\0';

    return 0;
}


/*
 * ============================================================
 * RECEIVE EXACT NUMBER OF BYTES
 * ============================================================
 */

int recv_exact(int socket_fd,
               void *buffer,
               size_t length)
{
    size_t total_received;
    char *data;
    ssize_t received;

    total_received = 0;
    data = (char *)buffer;

    while (total_received < length)
    {
        received = recv(socket_fd,
                        data + total_received,
                        length - total_received,
                        0);

        if (received <= 0)
        {
            return -1;
        }

        total_received += (size_t)received;
    }

    return 0;
}


/*
 * ============================================================
 * SEND TCP RESPONSE
 * ============================================================
 */

int send_response(int socket_fd,
                  const char *message)
{
    char response[BUFFER_SIZE];

    snprintf(response,
             sizeof(response),
             "%s SID:%s\n",
             message,
             SID);

    return send_all(socket_fd,
                    response,
                    strlen(response));
}


/*
 * ============================================================
 * SYSTEM INFORMATION
 *
 * Output:
 * CPU load
 * Memory used in MB
 * Uptime in seconds
 * ============================================================
 */

int get_system_info(char *output,
                    size_t output_size)
{
    FILE *file;

    char load_average[128];

    unsigned long long total_memory_kb;
    unsigned long long available_memory_kb;
    unsigned long long used_memory_kb;
    unsigned long long used_memory_mb;

    unsigned long long uptime_seconds;

    double uptime_decimal;


    /*
     * --------------------------------------------------------
     * CPU LOAD
     * --------------------------------------------------------
     */

    file = fopen("/proc/loadavg",
                 "r");

    if (file == NULL)
    {
        return -1;
    }

    if (fscanf(file,
               "%127s",
               load_average) != 1)
    {
        fclose(file);

        return -1;
    }

    fclose(file);


    /*
     * --------------------------------------------------------
     * MEMORY
     *
     * /proc/meminfo gives values in kB.
     * Assignment requires mem_used_mb.
     * Therefore convert kB to MB.
     * --------------------------------------------------------
     */

    file = fopen("/proc/meminfo",
                 "r");

    if (file == NULL)
    {
        return -1;
    }

    total_memory_kb = 0;
    available_memory_kb = 0;


    char line[256];


    while (fgets(line,
                 sizeof(line),
                 file) != NULL)
    {
        unsigned long long value;


        if (sscanf(line,
                   "MemTotal: %llu kB",
                   &value) == 1)
        {
            total_memory_kb = value;
        }


        if (sscanf(line,
                   "MemAvailable: %llu kB",
                   &value) == 1)
        {
            available_memory_kb = value;
        }
    }


    fclose(file);


    if (total_memory_kb >= available_memory_kb)
    {
        used_memory_kb =
            total_memory_kb - available_memory_kb;
    }
    else
    {
        used_memory_kb = 0;
    }


    /*
     * Convert kB to MB.
     *
     * 1024 kB = 1 MB
     */

    used_memory_mb =
        used_memory_kb / 1024;


    /*
     * --------------------------------------------------------
     * UPTIME
     * --------------------------------------------------------
     */

    file = fopen("/proc/uptime",
                 "r");

    if (file == NULL)
    {
        return -1;
    }

    uptime_decimal = 0.0;


    if (fscanf(file,
               "%lf",
               &uptime_decimal) != 1)
    {
        fclose(file);

        return -1;
    }


    fclose(file);


    uptime_seconds =
        (unsigned long long)uptime_decimal;


    /*
     * --------------------------------------------------------
     * FINAL FORMAT
     *
     * CPU load
     * Memory used in MB
     * Uptime in seconds
     * --------------------------------------------------------
     */

    snprintf(output,
             output_size,
             "%s %llu %llu",
             load_average,
             used_memory_mb,
             uptime_seconds);


    return 0;
}


/*
 * ============================================================
 * VALIDATE FILENAME
 * ============================================================
 */

int valid_filename(const char *filename)
{
    if (filename == NULL)
    {
        return 0;
    }

    if (strlen(filename) == 0)
    {
        return 0;
    }

    if (strlen(filename) >= MAX_FILENAME)
    {
        return 0;
    }

    if (strstr(filename,
                "..") != NULL)
    {
        return 0;
    }

    if (strchr(filename,
               '/') != NULL)
    {
        return 0;
    }

    if (strchr(filename,
               '\\') != NULL)
    {
        return 0;
    }

    return 1;
}


/*
 * ============================================================
 * EXEC COMMAND
 * ============================================================
 */

int execute_allowed_command(const char *command,
                            char *output,
                            size_t output_size)
{
    const char *program;


    if (strcmp(command,
               "DATE") == 0)
    {
        program = "date";
    }
    else if (strcmp(command,
                    "UPTIME") == 0)
    {
        program = "uptime";
    }
    else if (strcmp(command,
                    "DISKFREE") == 0)
    {
        program = "df -h /";
    }
    else if (strcmp(command,
                    "HOSTNAME") == 0)
    {
        program = "hostname";
    }
    else if (strcmp(command,
                    "WHOAMI") == 0)
    {
        program = "whoami";
    }
    else
    {
        return 0;
    }


    FILE *process;


    process = popen(program,
                    "r");


    if (process == NULL)
    {
        return -1;
    }


    size_t used;


    used = 0;


    while (used < output_size - 1)
    {
        size_t remaining;
        size_t bytes_read;


        remaining =
            output_size - used - 1;


        bytes_read =
            fread(output + used,
                  1,
                  remaining,
                  process);


        if (bytes_read == 0)
        {
            break;
        }


        used += bytes_read;
    }


    output[used] = '\0';


    pclose(process);


    /*
     * Convert multiline output to one line.
     */

    size_t i;


    for (i = 0;
         i < used;
         i++)
    {
        if (output[i] == '\n' ||
            output[i] == '\r')
        {
            output[i] = ' ';
        }
    }


    return 1;
}


/*
 * ============================================================
 * PUT
 * ============================================================
 */

int receive_file(int socket_fd,
                 const char *filename,
                 unsigned long long file_size)
{
    char path[512];


    if (!valid_filename(filename))
    {
        send_response(socket_fd,
                      "ERR 003 INVALID_FILENAME");

        write_log("PUT rejected: invalid filename");

        return -1;
    }


    if (file_size > MAX_FILE_SIZE)
    {
        send_response(socket_fd,
                      "ERR 004 FILE_TOO_LARGE");

        write_log("PUT rejected: file too large");

        return -1;
    }


    snprintf(path,
             sizeof(path),
             "%s/%s",
             STORAGE_DIRECTORY,
             filename);


    FILE *file;


    file = fopen(path,
                 "wb");


    if (file == NULL)
    {
        send_response(socket_fd,
                      "ERR 005 FILE_OPEN_FAILED");

        write_log("PUT failed: could not create file");

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


        if (bytes_to_receive > sizeof(buffer))
        {
            bytes_to_receive =
                sizeof(buffer);
        }


        if (recv_exact(socket_fd,
                       buffer,
                       bytes_to_receive) != 0)
        {
            fclose(file);

            write_log(
                "PUT failed: incomplete file data");

            return -1;
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

            write_log(
                "PUT failed: file write error");

            return -1;
        }


        total_received +=
            (unsigned long long)bytes_to_receive;
    }


    fclose(file);


    char log_message[512];


    snprintf(log_message,
             sizeof(log_message),
             "PUT completed: %s (%llu bytes)",
             filename,
             file_size);


    write_log(log_message);


    char response[BUFFER_SIZE];


    snprintf(response,
             sizeof(response),
             "OK FILE_RECEIVED %s",
             filename);


    send_response(socket_fd,
                  response);


    return 0;
}


/*
 * ============================================================
 * GET
 * ============================================================
 */

int send_file(int socket_fd,
              const char *filename)
{
    char path[512];


    if (!valid_filename(filename))
    {
        send_response(socket_fd,
                      "ERR 003 INVALID_FILENAME");

        write_log("GET rejected: invalid filename");

        return -1;
    }


    snprintf(path,
             sizeof(path),
             "%s/%s",
             STORAGE_DIRECTORY,
             filename);


    FILE *file;


    file = fopen(path,
                 "rb");


    if (file == NULL)
    {
        send_response(socket_fd,
                      "ERR 006 FILE_NOT_FOUND");

        write_log("GET failed: file not found");

        return -1;
    }


    if (fseek(file,
              0,
              SEEK_END) != 0)
    {
        fclose(file);

        send_response(socket_fd,
                      "ERR 007 FILE_SEEK_FAILED");

        return -1;
    }


    long file_size_long;


    file_size_long =
        ftell(file);


    if (file_size_long < 0)
    {
        fclose(file);

        send_response(socket_fd,
                      "ERR 008 FILE_SIZE_FAILED");

        return -1;
    }


    rewind(file);


    unsigned long long file_size;


    file_size =
        (unsigned long long)file_size_long;


    char header[BUFFER_SIZE];


    snprintf(header,
             sizeof(header),
             "OK FILE_SEND %s %llu SID:%s\n",
             filename,
             file_size,
             SID);


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


        if (bytes_to_read > sizeof(buffer))
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

            write_log(
                "GET failed: file read error");

            return -1;
        }


        if (send_all(socket_fd,
                     buffer,
                     bytes_read) != 0)
        {
            fclose(file);

            write_log(
                "GET failed: network send error");

            return -1;
        }


        total_sent +=
            (unsigned long long)bytes_read;
    }


    fclose(file);


    char log_message[512];


    snprintf(log_message,
             sizeof(log_message),
             "GET completed: %s (%llu bytes)",
             filename,
             file_size);


    write_log(log_message);


    return 0;
}


/*
 * ============================================================
 * UDP MONITOR THREAD
 * ============================================================
 */

void *monitor_thread_function(void *argument)
{
    MonitorData *monitor;


    monitor =
        (MonitorData *)argument;


    monitor->udp_socket =
        socket(AF_INET,
               SOCK_DGRAM,
               0);


    if (monitor->udp_socket < 0)
    {
        write_log(
            "UDP monitoring failed: socket creation");


        monitor->running = 0;


        return NULL;
    }


    struct sockaddr_in destination;


    memset(&destination,
           0,
           sizeof(destination));


    destination.sin_family =
        AF_INET;


    destination.sin_port =
        htons(monitor->udp_port);


    if (inet_pton(AF_INET,
                  monitor->controller_ip,
                  &destination.sin_addr) <= 0)
    {
        close(monitor->udp_socket);


        monitor->running = 0;


        write_log(
            "UDP monitoring failed: invalid controller IP");


        return NULL;
    }


    /*
     * Send monitoring data every 5 seconds.
     */

    while (monitor->running)
    {
        char system_info[BUFFER_SIZE];


        if (get_system_info(system_info,
                            sizeof(system_info)) == 0)
        {
            char packet[BUFFER_SIZE];


            snprintf(packet,
                     sizeof(packet),
                     "SYSINFO %.3950s SID:%s",
                     system_info,
                     SID);


            sendto(monitor->udp_socket,
                   packet,
                   strlen(packet),
                   0,
                   (struct sockaddr *)&destination,
                   sizeof(destination));


            printf("UDP sent: %s\n",
                   packet);


            char log_message[BUFFER_SIZE];


            snprintf(log_message,
                     sizeof(log_message),
                     "UDP monitoring sent: %.450s",
                     packet);


            write_log(log_message);
        }


        /*
         * Sleep one second at a time.
         */

        int second;


        for (second = 0;
             second < MONITOR_INTERVAL;
             second++)
        {
            if (!monitor->running)
            {
                break;
            }


            sleep(1);
        }
    }


    close(monitor->udp_socket);


    return NULL;
}


/*
 * ============================================================
 * START MONITORING
 * ============================================================
 */

int start_monitoring(MonitorData *monitor,
                     const char *controller_ip,
                     int udp_port)
{
    if (monitor->running)
    {
        return -1;
    }


    monitor->running = 1;

    monitor->udp_socket = -1;

    monitor->udp_port = udp_port;


    strncpy(monitor->controller_ip,
            controller_ip,
            sizeof(monitor->controller_ip) - 1);


    monitor->controller_ip[
        sizeof(monitor->controller_ip) - 1
    ] = '\0';


    if (pthread_create(&monitor->thread_id,
                       NULL,
                       monitor_thread_function,
                       monitor) != 0)
    {
        monitor->running = 0;

        return -1;
    }


    printf("UDP monitoring started: %s:%d\n",
           monitor->controller_ip,
           monitor->udp_port);


    char log_message[512];


    snprintf(log_message,
             sizeof(log_message),
             "UDP monitoring started: %s:%d",
             monitor->controller_ip,
             monitor->udp_port);


    write_log(log_message);


    return 0;
}


/*
 * ============================================================
 * STOP MONITORING
 * ============================================================
 */

void stop_monitoring(MonitorData *monitor)
{
    if (!monitor->running)
    {
        return;
    }


    monitor->running = 0;


    pthread_join(monitor->thread_id,
                 NULL);


    printf("UDP monitoring stopped: %s:%d\n",
           monitor->controller_ip,
           monitor->udp_port);


    char log_message[512];


    snprintf(log_message,
             sizeof(log_message),
             "UDP monitoring stopped: %s:%d",
             monitor->controller_ip,
             monitor->udp_port);


    write_log(log_message);
}


/*
 * ============================================================
 * CONTROLLER THREAD
 * ============================================================
 */

void *handle_client(void *argument)
{
    int client_socket;


    client_socket =
        *((int *)argument);


    free(argument);


    struct sockaddr_in client_address;


    socklen_t address_length;


    address_length =
        sizeof(client_address);


    memset(&client_address,
           0,
           sizeof(client_address));


    getpeername(client_socket,
                (struct sockaddr *)&client_address,
                &address_length);


    char client_ip[INET_ADDRSTRLEN];


    inet_ntop(AF_INET,
              &client_address.sin_addr,
              client_ip,
              sizeof(client_ip));


    int client_port;


    client_port =
        ntohs(client_address.sin_port);


    printf("Controller connected: %s:%d\n",
           client_ip,
           client_port);


    char log_message[512];


    snprintf(log_message,
             sizeof(log_message),
             "Controller connected: %s:%d",
             client_ip,
             client_port);


    write_log(log_message);


    MonitorData monitor;


    memset(&monitor,
           0,
           sizeof(monitor));


    monitor.running = 0;

    monitor.udp_socket = -1;


    int authenticated;


    authenticated = 0;


    char command[BUFFER_SIZE];


    while (1)
    {
        memset(command,
               0,
               sizeof(command));


        if (recv_line(client_socket,
                      command,
                      sizeof(command)) != 0)
        {
            break;
        }


        command[strcspn(command,
                        "\r\n")] = '\0';


        printf("Command received: %s\n",
               command);


        snprintf(log_message,
                 sizeof(log_message),
                 "Command from %s:%d: %.450s",
                 client_ip,
                 client_port,
                 command);


        write_log(log_message);


        /*
         * HELLO
         */

        if (strcmp(command,
                   "HELLO") == 0)
        {
            send_response(client_socket,
                          "HELLO FROM AGENT");

            continue;
        }


        /*
         * AUTH
         */

        if (strncmp(command,
                    "AUTH ",
                    5) == 0)
        {
            char token[128];


            memset(token,
                   0,
                   sizeof(token));


            if (sscanf(command + 5,
                       "%127s",
                       token) != 1)
            {
                send_response(
                    client_socket,
                    "ERR 001 AUTH_REQUIRED");

                continue;
            }


            if (strcmp(token,
                       AUTH_TOKEN) == 0)
            {
                authenticated = 1;


                printf("Controller authenticated.\n");


                write_log(
                    "Controller authenticated");


                send_response(
                    client_socket,
                    "OK AUTHENTICATED");
            }
            else
            {
                write_log(
                    "Authentication failed");


                send_response(
                    client_socket,
                    "ERR 001 AUTH_FAILED");
            }


            continue;
        }


        /*
         * Authentication required.
         */

        if (!authenticated)
        {
            send_response(
                client_socket,
                "ERR 001 AUTH_REQUIRED");

            continue;
        }


        /*
         * SYSINFO
         */

        if (strcmp(command,
                   "SYSINFO") == 0)
        {
            char system_info[BUFFER_SIZE];


            if (get_system_info(
                    system_info,
                    sizeof(system_info)) == 0)
            {
                char response[BUFFER_SIZE];


                snprintf(response,
                         sizeof(response),
                         "OK SYSINFO %.3950s",
                         system_info);


                send_response(
                    client_socket,
                    response);
            }
            else
            {
                send_response(
                    client_socket,
                    "ERR 010 SYSINFO_FAILED");
            }


            continue;
        }


        /*
         * LISTPROC
         */

        if (strcmp(command,
                   "LISTPROC") == 0)
        {
            FILE *process;


            process =
                popen("ps -eo pid,comm --no-headers",
                      "r");


            if (process == NULL)
            {
                send_response(
                    client_socket,
                    "ERR 011 LISTPROC_FAILED");

                continue;
            }


            char response[BUFFER_SIZE];


            strcpy(response,
                   "OK PROCS ");


            size_t used;


            used =
                strlen(response);


            char line[256];


            while (fgets(line,
                         sizeof(line),
                         process) != NULL)
            {
                line[strcspn(line,
                             "\r\n")] = '\0';


                if (used + strlen(line) + 2 <
                    sizeof(response) - 64)
                {
                    strcat(response,
                           line);


                    strcat(response,
                           "; ");


                    used =
                        strlen(response);
                }
            }


            pclose(process);


            send_response(
                client_socket,
                response);


            continue;
        }


        /*
         * EXEC
         */

        if (strncmp(command,
                    "EXEC ",
                    5) == 0)
        {
            char exec_command[128];


            memset(exec_command,
                   0,
                   sizeof(exec_command));


            if (sscanf(command + 5,
                       "%127s",
                       exec_command) != 1)
            {
                send_response(
                    client_socket,
                    "ERR 002 COMMAND_NOT_ALLOWED");

                continue;
            }


            char command_output[BUFFER_SIZE];


            int result;


            result =
                execute_allowed_command(
                    exec_command,
                    command_output,
                    sizeof(command_output));


            if (result == 0)
            {
                write_log(
                    "EXEC rejected: command not allowed");


                send_response(
                    client_socket,
                    "ERR 002 COMMAND_NOT_ALLOWED");


                continue;
            }


            if (result < 0)
            {
                send_response(
                    client_socket,
                    "ERR 012 EXEC_FAILED");


                continue;
            }


            char response[BUFFER_SIZE];


            snprintf(response,
                     sizeof(response),
                     "OK EXEC_RESULT %.3900s",
                     command_output);


            send_response(
                client_socket,
                response);


            continue;
        }


        /*
         * PUT
         */

        if (strncmp(command,
                    "PUT ",
                    4) == 0)
        {
            char filename[MAX_FILENAME];


            unsigned long long file_size;


            memset(filename,
                   0,
                   sizeof(filename));


            file_size = 0;


            int parsed;


            parsed =
                sscanf(command + 4,
                       "%255s %llu",
                       filename,
                       &file_size);


            if (parsed != 2)
            {
                send_response(
                    client_socket,
                    "ERR 013 INVALID_PUT");

                continue;
            }


            receive_file(
                client_socket,
                filename,
                file_size);


            continue;
        }


        /*
         * GET
         */

        if (strncmp(command,
                    "GET ",
                    4) == 0)
        {
            char filename[MAX_FILENAME];


            memset(filename,
                   0,
                   sizeof(filename));


            if (sscanf(command + 4,
                       "%255s",
                       filename) != 1)
            {
                send_response(
                    client_socket,
                    "ERR 014 INVALID_GET");

                continue;
            }


            send_file(
                client_socket,
                filename);


            continue;
        }


        /*
         * MONITOR START
         */

        if (strncmp(command,
                    "MONITOR START ",
                    14) == 0)
        {
            int udp_port;


            udp_port =
                atoi(command + 14);


            if (udp_port < 1 ||
                udp_port > 65535)
            {
                send_response(
                    client_socket,
                    "ERR 015 INVALID_UDP_PORT");

                continue;
            }


            if (monitor.running)
            {
                send_response(
                    client_socket,
                    "ERR 016 MONITOR_ALREADY_RUNNING");

                continue;
            }


            if (start_monitoring(
                    &monitor,
                    client_ip,
                    udp_port) == 0)
            {
                send_response(
                    client_socket,
                    "OK MONITOR_STARTED");
            }
            else
            {
                send_response(
                    client_socket,
                    "ERR 017 MONITOR_START_FAILED");
            }


            continue;
        }


        /*
         * MONITOR STOP
         */

        if (strcmp(command,
                   "MONITOR STOP") == 0)
        {
            if (!monitor.running)
            {
                send_response(
                    client_socket,
                    "ERR 018 MONITOR_NOT_RUNNING");

                continue;
            }


            stop_monitoring(&monitor);


            send_response(
                client_socket,
                "OK MONITOR_STOPPED");


            continue;
        }


        /*
         * QUIT
         */

        if (strcmp(command,
                   "QUIT") == 0)
        {
            printf(
                "Controller requested QUIT.\n");


            write_log(
                "Controller requested QUIT");


            if (monitor.running)
            {
                stop_monitoring(&monitor);
            }


            send_response(
                client_socket,
                "OK BYE");


            break;
        }


        /*
         * UNKNOWN COMMAND
         */

        send_response(
            client_socket,
            "ERR 019 UNKNOWN_COMMAND");
    }


    if (monitor.running)
    {
        stop_monitoring(&monitor);
    }


    close(client_socket);


    printf(
        "Controller thread finished.\n");


    write_log(
        "Controller connection closed");


    return NULL;
}


/*
 * ============================================================
 * MAIN
 * ============================================================
 */

int main(void)
{
    /*
     * Ignore SIGPIPE.
     */

    signal(SIGPIPE,
           SIG_IGN);


    /*
     * Create storage directory.
     */

    char command[512];


    snprintf(command,
             sizeof(command),
             "mkdir -p %s",
             STORAGE_DIRECTORY);


    system(command);


    /*
     * Create TCP socket.
     */

    int server_socket;


    server_socket =
        socket(AF_INET,
               SOCK_STREAM,
               0);


    if (server_socket < 0)
    {
        perror("socket");

        return 1;
    }


    /*
     * Allow address reuse.
     */

    int option;


    option = 1;


    if (setsockopt(server_socket,
                   SOL_SOCKET,
                   SO_REUSEADDR,
                   &option,
                   sizeof(option)) < 0)
    {
        perror("setsockopt");


        close(server_socket);


        return 1;
    }


    /*
     * Server address.
     */

    struct sockaddr_in server_address;


    memset(&server_address,
           0,
           sizeof(server_address));


    server_address.sin_family =
        AF_INET;


    server_address.sin_addr.s_addr =
        htonl(INADDR_ANY);


    server_address.sin_port =
        htons(SERVER_PORT);


    /*
     * Bind.
     */

    if (bind(server_socket,
             (struct sockaddr *)&server_address,
             sizeof(server_address)) < 0)
    {
        perror("bind");


        close(server_socket);


        return 1;
    }


    /*
     * Listen.
     */

    if (listen(server_socket,
               5) < 0)
    {
        perror("listen");


        close(server_socket);


        return 1;
    }


    /*
     * Startup information.
     */

    printf(
        "RemoteOps Agent started.\n");


    printf(
        "TCP listening port: %d\n",
        SERVER_PORT);


    printf(
        "SID: %s\n",
        SID);


    printf(
        "Authentication token: %s\n",
        AUTH_TOKEN);


    printf(
        "UDP monitoring interval: %d seconds\n",
        MONITOR_INTERVAL);


    write_log(
        "RemoteOps Agent started");


    write_log(
        "TCP server listening on port 9410");


    /*
     * Accept Controller connections.
     */

    while (1)
    {
        struct sockaddr_in client_address;


        socklen_t client_address_length;


        client_address_length =
            sizeof(client_address);


        int *client_socket_pointer;


        client_socket_pointer =
            malloc(sizeof(int));


        if (client_socket_pointer == NULL)
        {
            fprintf(
                stderr,
                "Memory allocation failed.\n");


            continue;
        }


        *client_socket_pointer =
            accept(
                server_socket,
                (struct sockaddr *)&client_address,
                &client_address_length);


        if (*client_socket_pointer < 0)
        {
            free(client_socket_pointer);


            if (errno == EINTR)
            {
                continue;
            }


            perror("accept");


            continue;
        }


        /*
         * Create Controller thread.
         */

        pthread_t thread_id;


        if (pthread_create(
                &thread_id,
                NULL,
                handle_client,
                client_socket_pointer) != 0)
        {
            perror("pthread_create");


            close(*client_socket_pointer);


            free(client_socket_pointer);


            continue;
        }


        pthread_detach(thread_id);


        printf(
            "New Controller thread created.\n");


        write_log(
            "New Controller thread created");
    }


    close(server_socket);


    return 0;
}
