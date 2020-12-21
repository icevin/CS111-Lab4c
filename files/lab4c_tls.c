// NAME: Kevin Tang
// EMAIL: kevintang2023@ucla.edu
// ID: 805419480

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <math.h>
#include <netdb.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <openssl/ssl.h>

#ifndef PC
#include <mraa.h>
#include <mraa/aio.h>
mraa_aio_context grove_sensor;
#endif

// argument options
int opt_period = 1;
int opt_scale  = 0;  // 0 = F, 1 = C
int opt_log    = 0;

FILE* opt_file = 0;
char* opt_host = NULL;
char* opt_id   = NULL;
int opt_debug  = 0;
int opt_port = -1;
// stdin Options
int opt_report = 1;

#define INPUT_SIZE 1023

// Some help from https://www.tutorialspoint.com/c_standard_library/c_function_localtime.htm
struct tm* time_info;
struct timeval time_now;
time_t last_report;
time_t current;

// Socket vars
int sock_fd = 0;
int rc      = 0;
struct sockaddr_in server_address;
struct hostent* host;

// SSL vars
SSL_CTX* newContext = NULL;
SSL *sslClient = NULL;

void log_and_print(char* input_s) {
    if (opt_log != 0) {
        fprintf(opt_file, "%s\n", input_s);
        fflush(opt_file);
    }
    // dprintf(sock_fd, "%s\n", input_s);
    char tempOut[200];
    sprintf(tempOut, "%s\n", input_s);
    SSL_write(sslClient, tempOut, strlen(tempOut));
    fprintf(stderr, "%s\n", input_s);
}

void error_msg(int returnValue, char* message) {
    if (returnValue < 0) {
        fprintf(stderr, "Error %s. errno %d: %s\r\n", message, errno, strerror(errno));
        exit(errno);
    }
    return;
}

void shut_down() {
    char temp[128];
    time_info = localtime(&time_now.tv_sec);
    sprintf(temp, "%02d:%02d:%02d SHUTDOWN", time_info->tm_hour, time_info->tm_min, time_info->tm_sec);
    log_and_print(temp);
    if (opt_log)
        fclose(opt_file);

#ifndef PC
    mraa_aio_close(grove_sensor);
#endif

    SSL_shutdown(sslClient);
    SSL_free(sslClient);

    exit(0);
}

double parse_reading(int reading) {
    // https://wiki.seeedstudio.com/Grove-Temperature_Sensor_V1.2/
    const double B     = 4275.;
    const double R0    = 100000.;
    double t           = 1023.0 / (double)reading - 1.0;
    t                  = R0 * t;
    double log_result  = log(t / R0);
    double temperature = 1.0 / (log_result / B + 1 / 298.15) - 273.15;  // Convert to temperature via datasheet

    if (opt_scale == 0)  // F
    {
        return (temperature * 9. / 5.) + 32;
    } else {
        return temperature;
    }
}

// option_string is a \0 terminated string w/ one of the options
void parse_option(char* option_string) {
    if (opt_log != 0) {
        fprintf(opt_file, "%s\n", option_string);
        fflush(opt_file);
    }
    if (strcmp(option_string, "SCALE=F") == 0) {
        opt_scale = 0;
    } else if (strcmp(option_string, "SCALE=C") == 0) {
        opt_scale = 1;
    } else if (strcmp(option_string, "STOP") == 0) {
        opt_report = 0;
    } else if (strcmp(option_string, "START") == 0) {
        opt_report = 1;
    } else if (strcmp(option_string, "OFF") == 0) {
        shut_down();
    } else {
        char* found_period = strstr(option_string, "PERIOD=");
        if (found_period != NULL) {
            // move found_period to beginning of arg
            // technically not needed for atoi(), but why not
            found_period += 7;
            opt_period = atoi(found_period);
            return;
        }
        // We don't technically need to look for LOG <message> since all non-valid commands are automatically outputted anyways
        // We just find it and do nothing
        char* found_log = strstr(option_string, "LOG ");
        if (found_log != NULL) {
            // move found_log to beginning of arg
            found_period += 4;
        }
    }
}

int main(int argc, char** argv) {
    // Process args
    static const struct option options[] =
        {
            {"period", required_argument, NULL, 1},
            {"scale", required_argument, NULL, 2},
            {"log", required_argument, NULL, 3},
            {"host", required_argument, NULL, 4},
            {"id", required_argument, NULL, 5},
            {0, 0, 0, 0}};

    for (;;) {
        int opt_index = 0;
        int c         = getopt_long(argc, argv, "", options, &opt_index);

        if (c == -1)
            break;

        switch (c) {
            case 1:
                opt_period = atoi(optarg);
                break;
            case 2:
                if (strcmp(optarg, "F") == 0) {
                    opt_scale = 0;
                } else if (strcmp(optarg, "C") == 0) {
                    opt_scale = 1;
                } else {
                    fprintf(stderr, "Invalid scale argument, please try again\n\n");
                    exit(1);
                }
                break;
            case 3:
                opt_log  = 1;
                opt_file = fopen(optarg, "a+");
                if (opt_file == NULL) {
                    fprintf(stderr, "Error attempting to create/open log file. errno %d: %s\r\n", errno, strerror(errno));
                    exit(1);
                }
                break;
            case 4:
                opt_host = optarg;
                if (strlen(opt_host) == 0) {
                    fprintf(stderr, "Invalid hostname argument, please try again\n");
                    exit(1);
                }
                break;
            case 5:
                opt_id = optarg;
                if (strlen(opt_id) != 9) {
                    fprintf(stderr, "Invalid ID argument, please try again\n");
                    exit(1);
                }
                break;

            default:
                fprintf(stderr, "Try \"lab4c [--period=<seconds>] [--log=<log_filename>] [--scale=F/C] [--id=ID] [--host=HOSTNAME] [--debug] port-number\"\n\n");
                exit(1);
                break;
        }
    }

    if (optind < argc) {
        opt_port = atoi(argv[optind]);
        if (opt_port < 0 || opt_port > 65535) {
            fprintf(stderr, "Invalid port number, please try again");
            exit(1);
        }
    } else {
        fprintf(stderr, "Missing port number. Try \"lab4c [--period=<seconds>] [--log=<log_filename>] [--scale=F/C] [--id=ID] [--host=HOSTNAME] [--debug] port-number\"\n\n");
        exit(1);
    }

    if (opt_log == 0 || opt_host == NULL || opt_id == NULL) {
        // Some help from:
        // https://www.gnu.org/software/libc/manual/html_node/Using-Getopt.html
        fprintf(stderr, "Missing required options. Try \"lab4c [--period=<seconds>] [--log=<log_filename>] [--scale=F/C] [--id=ID] [--host=HOSTNAME] [--debug] port-number\"\n\n");
        exit(1);
    }

#ifndef PC
    grove_sensor = mraa_aio_init(1);
    if (grove_sensor == NULL)  // || grove_button == NULL)
    {
        fprintf(stderr, "Error: failed to initialize sensors. errno %d: %s\r\n", errno, strerror(errno));
        exit(1);
    }
#endif

    // Create a socket

    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    error_msg(sock_fd, "while creating socket");

    bzero((char*)&server_address, sizeof(server_address));
    server_address.sin_family = AF_INET;

    host = gethostbyname(opt_host);
    if (host == NULL) {
        error_msg(-1, "while finding host");
    }

    bcopy((char*)host->h_addr, (char*)&server_address.sin_addr.s_addr, host->h_length);

    server_address.sin_port = htons(opt_port);


    int address_length = sizeof(server_address);

    // Connect to server using - connect()
    rc = connect(sock_fd, (struct sockaddr*)&server_address, address_length);
    error_msg(rc, "while connecting to server");


    // SSL
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();


    newContext = SSL_CTX_new(TLSv1_client_method());
    if (newContext == NULL)
        error_msg(-1, "getting SSL context");

    sslClient = SSL_new(newContext);
    if (sslClient == NULL)
        error_msg(-1, "completing SSL setup");

    rc = SSL_set_fd(sslClient, sock_fd);
    error_msg(rc, "associating FD with SSL client");

    rc = SSL_connect(sslClient);
    error_msg(rc, "connecting with SSL");

    char temp[16];
    sprintf(temp, "ID=%s\n", opt_id);
    SSL_write(sslClient, temp, strlen(temp));

    char buffer[INPUT_SIZE + 1];

    struct pollfd poll_fd;
    poll_fd.fd     = sock_fd;
    poll_fd.events = POLLIN | POLLERR;

    int poll_rc = 0;
    int read_rc = 0;

    time(&last_report);
    time(&current);

    for (;;) {
        time(&current);
        gettimeofday(&time_now, 0);

        if (opt_report == 1 && difftime(current, last_report) >= (double)opt_period) {
            time_info = localtime(&time_now.tv_sec);
#ifndef PC
            int sensor_reading = mraa_aio_read(grove_sensor);
#else
            int sensor_reading = 100;
#endif
            float temperature = parse_reading(sensor_reading);
            char temp[128];
            sprintf(temp, "%02d:%02d:%02d %.1f", time_info->tm_hour, time_info->tm_min, time_info->tm_sec, temperature);
            log_and_print(temp);
            time(&last_report);
        }

        poll_rc = poll(&poll_fd, 1, 0);
        if (poll_rc < 0) {
            fprintf(stderr, "Error polling. errno %d: %s\r\n", errno, strerror(errno));
            exit(1);
        }

        if (poll_fd.revents & POLLIN) {
            read_rc = SSL_read(sslClient, buffer, INPUT_SIZE);
            if (read_rc < 0) {
                fprintf(stderr, "Error reading. errno %d: %s\r\n", errno, strerror(errno));
                exit(1);
            }

            // lex input
            buffer[read_rc] = '\0';

            char* head = &buffer[0];

            // Ignore whitespace
            while (*head != '\0') {
                while (*head == ' ' || *head == '\t')
                    head++;

                char* tail = strstr(head, "\n");

                int size         = tail - head;
                char* arg_buffer = (char*)malloc((size + 1) * sizeof(char));
                strncpy(arg_buffer, head, size);
                arg_buffer[size] = '\0';
                parse_option(arg_buffer);
                free(arg_buffer);
                head = tail + 1;
            }
        }
    }



    SSL_shutdown(sslClient);
    SSL_free(sslClient);

    if (opt_log) {
        fclose(opt_file);
    }

#ifndef PC
    mraa_aio_close(grove_sensor);
#endif
}