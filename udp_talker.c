#include <assert.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#include <termios.h>
#include <arpa/inet.h>

fd_set set;
//sockets descriptors
int fd_input;
struct termios terminal_settings;
struct sockaddr_in in, out;
const int MESSAGE_LENGTH = 1000;
const int MESSAGES_LIMIT = 50;
int can_print_msg = 1;
int position_in_message = 0;
char MESSAGES[50000];

void solve_error(char *error_message, int error_value);
void make_sockets(char *ip_address, char *port);
void receive_message();
void send_message();
void exit_talker(int sig);
void set_exit_signal();
void canon_mode_off();
void canon_mode_on();

int main(int argc, char **args)
{
    printf("Starting UDP talker...\n");

    if (argc != 3)
    {
        fprintf(stderr, "Error: Bad format of arguments\nCorrect arguments:./talker.c <ip_address> <port>\n");
        exit(1);
    }

    int error_value = tcgetattr(0, &terminal_settings);
    solve_error("Error: Failed to save terminal settings\n", error_value);
    canon_mode_off();
    set_exit_signal();
    make_sockets(args[1], args[2]);
    printf("UDP talker has started, you can click Ctrl+C to exit\n");

    char SENDING_MESSAGE[MESSAGE_LENGTH + 1];
    memset(SENDING_MESSAGE, 0, MESSAGE_LENGTH + 1);

    while (1)
    {
        FD_ZERO(&set);
        FD_SET(0, &set);
        FD_SET(fd_input, &set);

        int error_value = select(fd_input + 1, &set, NULL, NULL, NULL);
        solve_error("Selecting error\n", error_value);

        if (FD_ISSET(fd_input, &set))
            receive_message();
        if (FD_ISSET(0, &set))
        {
            int lengthofmessage = strlen(SENDING_MESSAGE);
            char c = getc(stdin);
	    if ((int)c != 127 && (int)c != 8)
            {
                write(STDOUT_FILENO, &c, 1);
            }
            if ((int)c == 127 || (int)c == 8)
            {
                write(STDOUT_FILENO, "\b \b", 3);
                SENDING_MESSAGE[lengthofmessage - 1] = '\0';
            }
            else if ((int)c == 10)
            {
                send_message(SENDING_MESSAGE);
                memset(SENDING_MESSAGE, 0, MESSAGE_LENGTH);
                can_print_msg = 1;
                fflush(stdin);
            }
            else if (lengthofmessage < MESSAGE_LENGTH - 1)
            {
                SENDING_MESSAGE[lengthofmessage] = c;
                can_print_msg = 0;
            }
        }

        if (position_in_message > 0 && can_print_msg == 1)
        {
            printf("%.*s", position_in_message, MESSAGES);
            position_in_message = 0;
            memset(MESSAGES, 0, MESSAGE_LENGTH * MESSAGES_LIMIT);
        }
    }
}

void solve_error(char *error_message, int error_value)
{
    if (error_value < 0)
    {
        perror(error_message);
        exit(1);
    }
}

void make_sockets(char *ip_address, char *port)
{
    int error_value;

    socklen_t len_of_in = sizeof(in);

    out.sin_family = AF_INET;
    error_value = inet_pton(AF_INET, ip_address, &(out.sin_addr));

    switch (error_value)
    {
    case -1:
        fprintf(stderr, "Error: Invalid network address in the specified address family\n");
        exit(1);
        break;
    case 0:
        fprintf(stderr, "Error: Invalid IP address!\n");
        exit(1);
    }

    int _port = atoi(port);
    if (_port <= 0)
    {
        fprintf(stderr, "Error: Invalid port\n");
        exit(1);
    }

    out.sin_port = htons(_port);

    in.sin_family = AF_INET;
    in.sin_addr.s_addr = INADDR_ANY;
    in.sin_port = htons(_port);
    fd_input = socket(AF_INET, SOCK_DGRAM, 0);

    error_value = bind(fd_input, (struct sockaddr *)&in, len_of_in);
    solve_error("Error: Binding failed\n", error_value);
}

void receive_message()
{
    char ACCEPTED_MESSAGE[MESSAGE_LENGTH + 1];
    memset(ACCEPTED_MESSAGE, 0, MESSAGE_LENGTH + 1);
    //Receiving message from network
    int num_chars = read(fd_input, ACCEPTED_MESSAGE, MESSAGE_LENGTH);
    solve_error("Error: Failed to read data from network!\n", num_chars);
    ACCEPTED_MESSAGE[num_chars] = '\0';
    if (num_chars > 1)
    {
        if (can_print_msg == 1)
        {
            printf("%.*s", num_chars, ACCEPTED_MESSAGE);
        }
        else
        {
            int i;
            for (i = 0; i < num_chars && position_in_message + i < MESSAGE_LENGTH * MESSAGES_LIMIT; i++)
                MESSAGES[position_in_message + i] = ACCEPTED_MESSAGE[i];
            position_in_message += i;
        }
    }
}

void send_message(char *SENDING_MESSAGE)
{
    int lengthofmessage = strlen(SENDING_MESSAGE);
    SENDING_MESSAGE[lengthofmessage] = '\n';
    lengthofmessage++;
    if (lengthofmessage < 2)
    {
        printf("Dont send empty line!!!\n");
        return;
    }
    int error_value = sendto(fd_input, SENDING_MESSAGE, lengthofmessage, 0, (struct sockaddr *)&out, sizeof(out));
    solve_error("Error: Failed to send message!\n", error_value);
}

void exit_talker(int sig)
{
    assert(sig == SIGINT);
    close(fd_input);
    canon_mode_on();
    printf("\nUDP talker has been closed\n");
    exit(1);
}

void set_exit_signal()
{
    void (*handler)(int);
    handler = signal(SIGINT, exit_talker);
    solve_error("Error: Setting signal failed!\n", handler == SIG_ERR ? -1 : 0);
}

void canon_mode_off()
{
    terminal_settings.c_lflag &= ~ICANON;
    terminal_settings.c_lflag &= ~ECHO;
    int error_value = tcsetattr(0, TCSANOW, &terminal_settings);
    solve_error("Error: Failed to set canon mode off\n", error_value);
}
void canon_mode_on()
{
    terminal_settings.c_lflag |= ICANON;
    terminal_settings.c_lflag |= ECHO;
    int error_value = tcsetattr(0, TCSANOW, &terminal_settings);
    solve_error("Error: Failed to set canon mode on\n", error_value);
}
