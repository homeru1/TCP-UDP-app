#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/select.h>
#include <netdb.h>
#include <errno.h>

#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/poll.h>
#include <malloc.h>
#include <stdlib.h>

int set_non_block_mode(int s)
{
    int fl = fcntl(s, F_GETFL, 0);
    return fcntl(s, F_SETFL, fl | O_NONBLOCK);
}
void s_close(int s)
{
    close(s);
}

int sock_err(const char *function, int s)
{
    int err = errno;
    fprintf(stderr, "%s: socket error: %d\n", function, err);
    return -1;
}

struct client
{
    char id[30];
    int msgs[20];
    struct client *next;
    char send[81];
    int amount;
    int answer;
};

struct client *addnew(struct client *head)
{
    struct client *tmp = (struct client *)calloc(1, sizeof(struct client));
    memset(tmp->msgs, 0, 20);
    memset(tmp->send, '\0', 81);
    memset(tmp->id, '\0', 30);
    tmp->amount = 0;
    tmp->answer = 0;
    tmp->next = NULL;
    return tmp;
}

struct client *search(struct client *head, unsigned int ip, int port)
{
    char str[30];
    sprintf(str, "%u.%u.%u.%u:%d", (ip >> 24) & 0xFF, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, (ip)&0xFF, port);
    struct client *tmp = head;
    if (tmp!=NULL){
        while (1)
        {
            if (strcmp(tmp->id, str) == 0)
            {
                return tmp;
            }
            if(tmp->next==NULL)break;
            tmp = tmp->next;
        }
        tmp->next = addnew(head);
        tmp = tmp->next;
        strcpy(tmp->id, str);
        return tmp;
    } else {
        tmp = addnew(head);
        strcpy(tmp->id, str);
        return tmp;
    }
}

int parcing(FILE *text, char *buffer, int s, struct sockaddr_in *addr, struct client *head, int port)
{
    int flags = MSG_NOSIGNAL;
    int addrlen = sizeof(*addr);
    unsigned int ip = ntohl((*addr).sin_addr.s_addr);
	struct client *tmp = search(head, ip, port);
    int n;
    memcpy(&n, buffer, 4);
    n = ntohl(n);
    if (tmp->msgs[n] == 0)
    {
        tmp->msgs[n] = 1;
        tmp->answer++;
        n = htonl(n);
        memcpy(&tmp->send[4 * tmp->amount], tmp->send, 4 * tmp->amount);
        tmp->amount++;
        memcpy(tmp->send, &n, 4);
        fprintf(text, "%s ", tmp->id);
		if(buffer[4]<10)fprintf(text, "0%d.", (int)buffer[4]);
		else fprintf(text, "%d.", (int)buffer[4]); //day
		if (buffer[5] < 10)fprintf(text, "0%d.", (int)buffer[5]);
		else fprintf(text, "%d.", (int)buffer[5]); //month
        memcpy(&n, &buffer[6], 2);
        n = ntohs(n);
        fprintf(text, "%hu ", n);
        memcpy(&n, &buffer[8], 2);
        n = ntohs(n);
        fprintf(text, "%hi ", n);
        for (int i = 10; i < 22; i++)
            fprintf(text, "%c", buffer[i]);
        fprintf(text, " %s\n", &buffer[22]);
        sendto(s, tmp->send, 4 * tmp->amount, flags, (struct sockaddr *)addr, addrlen);
        if (strcmp(&buffer[22], "stop") == 0)
            return 0;
        return 1;
    }
    else
    {
        sendto(s, tmp->send, 4 * tmp->amount, flags, (struct sockaddr *)addr, addrlen);
    }
}

int main(int argc, char *argv[])
{
    int i;
    int flags = MSG_NOSIGNAL;
    int start, end;
    if (argc == 3)
    {
        start = atoi(argv[1]);
        end = atoi(argv[2]);
    }
    else
    {
        start = atoi(argv[1]);
        end = start;
    }
    //start = 8035;
    //end = start;
    int AmountOfPorts = end - start + 1;
    struct pollfd *pfd = (struct pollfd *)calloc(AmountOfPorts, sizeof(struct pollfd));
    int *s = (int *)calloc(AmountOfPorts, sizeof(int));
    struct sockaddr_in *addr = (struct sockaddr_in *)calloc(AmountOfPorts, sizeof(struct sockaddr_in));
    for (int i = 0; i < AmountOfPorts; i++)
    {
        s[i] = socket(AF_INET, SOCK_DGRAM, 0);
        if (s[i] < 0)
            return sock_err("socket", s[i]);
        set_non_block_mode(s[i]);
        memset(&addr[i], 0, sizeof(addr));
        addr[i].sin_family = AF_INET;
        addr[i].sin_port = htons(start + i);
        addr[i].sin_addr.s_addr = htonl(INADDR_ANY);
        if (bind(s[i], (struct sockaddr *)&addr[i], sizeof(addr[i])) < 0)
            return sock_err("bind", s[i]);
        pfd[i].fd = s[i];
        pfd[i].events = POLLIN | POLLOUT;
    }
    int g = 1;
    struct client *head = NULL;
    struct client *tmp = NULL;
    FILE *text = fopen("msg.txt", "w");
    char buffer[131072] = {0};
    socklen_t addrlen = 0;
    do
    {
        int ev_cnt = poll(pfd, AmountOfPorts, 100000);
        if (ev_cnt > 0)
        {
            for (i = 0; i < AmountOfPorts; i++)
            {
                if (pfd[i].revents & POLLHUP)
                { // Сокет cs[i] - клиент отключился. Можно закрывать сокет
                    s_close(s[i]);
                }
                if (pfd[i].revents & POLLERR)
                { // Сокет cs[i] - возникла ошибка. Можно закрывать сокет
                    s_close(s[i]);
                }
                if (pfd[i].revents & POLLIN)
                { // Сокет cs[i] доступен на чтение, можно вызывать recv/recvfrom
                    addrlen = sizeof(addr[i]);
                    int rcv = recvfrom(s[i], buffer, sizeof(buffer), 0, (struct sockaddr *)(&addr[i]), &addrlen);
                    if (rcv > 0)
                    {
                        if (!parcing(text, buffer, s[i], &addr[i], head, start + i))
                        {
                            g = 0;
                            break;
                        }
                    }
                    if (pfd[i].revents & POLLOUT)
                    { // Сокет cs[i] доступен на запись, можно вызывать send/sendto
                        addrlen = sizeof(addr[i]);
                        unsigned int ip = ntohl((*addr).sin_addr.s_addr);
                        tmp = search(head, ip, start+i);
                        sendto(s[i], tmp->send, 4 * tmp->amount, flags, (struct sockaddr *)&addr[i], addrlen);
                    }
                }
            }
        }
        else
        {
        }
    } while (g);
    fclose(text);
    for (int i = 0; i < AmountOfPorts; i++)
        s_close(s[i]);
    return 0;
}