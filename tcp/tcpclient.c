#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <iostream>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <arpa/inet.h>

#define WEBHOST "192.168.247.1"

using namespace std;

struct data
{
	int number = -1;
	int d;
	int m;
	unsigned short y;
	int AA;
	char *num;
	char *message = NULL;
};

bool parsing(char *text, struct data *tmp)
{
	tmp->d = atoi(strtok(text, "."));
	if (tmp->d > 31 || tmp->d < 1)
		return 0;
	tmp->m = atoi(strtok(NULL, "."));
	if (tmp->m > 12 || tmp->m < 1)
		return 0;
	tmp->y = atoi(strtok(NULL, " "));
	if (tmp->y > 9999 || tmp->y < 0)
		return 0;
	tmp->AA = atoi(strtok(NULL, " "));
	if (tmp->AA > 32767 || tmp->AA < -32768)
		return 0;
	tmp->num = strtok(NULL, " ");
	if (strlen(tmp->num) > 12 || tmp->num[0] != '+' || tmp->num[1] != '7')
		return 0;
	tmp->message = strtok(NULL, "\n\0");
	tmp->number++;
	for (int i = 0; tmp->message[i] != '\0'; i++)
	{
		if (tmp->message[i] == '\n')
			tmp->message[i] = '\0';
	}
	return 1;
}

int sock_err(const char *function, int s)
{
	int err = errno;
	fprintf(stderr, "%s: socket error: %d\n", function, err);
	return -1;
}

void s_close(int s)
{
	close(s);
}

void recv_response(int s, int n)
{
	char buffer[256] ="\0";
	for (int i = 0; i <= n; i++)
	{
		recv(s, buffer, 2, 0);
		//printf("%d,%c%c\n",i, buffer[0],buffer[1]);
	}
}

int send_data(int s, struct data *tmp)
{
	int n = htonl(tmp->number);
	char *bytes = (char *)calloc(4, sizeof(char));
	bytes[0] = (n >> 24) & 0xFF;
	bytes[1] = (n >> 16) & 0xFF;
	bytes[2] = (n >> 8) & 0xFF;
	bytes[3] = n & 0xFF;
	if (send(s, bytes, 4, MSG_NOSIGNAL) < 0)
		return sock_err("send num", s); // number of msg
	bytes = (char *)realloc(bytes, 1);
	*bytes = tmp->d;
	if (send(s, bytes, 1, MSG_NOSIGNAL) < 0)
		return sock_err("send day", s); //day
	*bytes = tmp->m;
	if (send(s, bytes, 1, MSG_NOSIGNAL) < 0)
		return sock_err("send month", s); // month
	n = htons(tmp->y);
	bytes = (char *)realloc(bytes, 2);
	bytes[0] = (n >> 8) & 0xFF;
	bytes[1] = n & 0xFF;
	if (send(s, bytes, 2, MSG_NOSIGNAL) < 0)
		return sock_err("send year", s); // year
	n = htonl(tmp->AA);
	bytes[0] = (n >> 8) & 0xFF;
	bytes[1] = n & 0xFF;
	if (send(s, bytes, 2, MSG_NOSIGNAL) < 0)
		return sock_err("send AA", s); // AA
	if (send(s, tmp->num, 12, MSG_NOSIGNAL) < 0)
		return sock_err("send tel num", s); // tel number
	if (send(s, tmp->message, strlen(tmp->message) + 1, MSG_NOSIGNAL) < 0)
		return sock_err("send msg", s);
	; //msg
	//printf("size of msg:%d\n",strlen(tmp->message));
	return 0;
}

int main(int argc, char *argv[])
{
	int s;
	struct sockaddr_in addr;
	s = socket(AF_INET, SOCK_STREAM, 0);
	if (s < 0)
	{
		for (int i = 0; i < 10; i++)
		{
			usleep(100);
			s = socket(AF_INET, SOCK_STREAM, 0);
			if (s != -1)
				break;
		}
		if (s < 0)
			return sock_err("socket", s);
	}
	// Заполнение структуры с адресом удаленного узла
	char dec[3] = ": ";
	char *ip, *port, *filename;
	ip = strtok(argv[1], dec);
	port = strtok(NULL, dec);
	filename = argv[2];
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(atoi(port));
	addr.sin_addr.s_addr = inet_addr(ip);
	// Установка соединения с удаленным хостом
	if (connect(s, (struct sockaddr *)&addr, sizeof(addr)) != 0)
	{
		s_close(s);
		return sock_err("connect", s);
	}
	struct data tmp;
	FILE *file = fopen(filename, "r");
	char a[1024] = "\0";
	bool end = 1;
	tmp.message = dec;
	if (send(s, "put", 3, MSG_NOSIGNAL) < 0)
		return sock_err("socket", s);
	do
	{
		if(fgets(a, 1024, file)){
			while (a[0] == '\n')
			{
				if (fgets(a, 1024, file))
				{
				}
				else
				{
					end = 0;
					break;
				}
			}
			if (parsing(a, &tmp)&&end)
			{
				send_data(s, &tmp);
				//printf("Number:%d\nDay:%d\nM:%d\nYear:%d\nAA:%d\nnum:%s\nmessage:%s\n", tmp.number, tmp.d, tmp.m, tmp.y, tmp.AA, tmp.num, tmp.message);
			}
		}
		else {
			break;
		}
	} while (strcmp(tmp.message, "stop")&&end);
	recv_response(s,tmp.number);
	fclose(file);
	s_close(s);
	return 0;
}
