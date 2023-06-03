//#pragma warning(disable:4996)
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <windows.h> 
#include <winsock2.h> // Директива линковщику: использовать библиотеку сокетов 
#pragma comment(lib, "ws2_32.lib")

#include <stdio.h> 
#include <string.h>
#include<malloc.h>
#include<stdlib.h>

#include <stdio.h>
#include <stdint.h>


struct client {
	int cs;// = 0;
	int mode;// = 0; // 0 - new , 1 - put 2 - get
	int answer;// = 0;
	struct client *next;// = NULL;
	unsigned int ip;// = 0;
	char port[6];// = "\0";
};

int init() {
	WSADATA wsa_data; 
	return (0 == WSAStartup(MAKEWORD(2, 2), &wsa_data)); 
 }

void deinit() {
	WSACleanup(); 
}

void s_close(int s) {
	closesocket(s);
}

int set_non_block_mode(int s) {
	unsigned long mode = 1;
	return ioctlsocket(s, FIONBIO, &mode);
}

int sock_err(const char* function, int s) {
	int err; 
	err = WSAGetLastError();
	fprintf(stderr, "%s: socket error: %d\n", function, err); 
	return -1;
}
void close(struct client c) {

}
int parcing1(char* msg, FILE* open, struct client *cur) {
	fprintf(open, "%u.%u.%u.%u:%s ", (cur->ip >> 24) & 0xFF, (cur->ip >> 16) & 0xFF, (cur->ip >> 8) & 0xFF, (cur->ip) & 0xFF, cur->port);
	int no = 0; msg[0] & 0xff;
	memcpy(&no, msg, 4);
	no = htonl(no);
	if(msg[4]<10)fprintf(open, "0%d.", (int)msg[4]);
	else fprintf(open, "%d.", (int)msg[4]); //day
	if (msg[5] < 10)fprintf(open, "0%d.", (int)msg[5]);
	else fprintf(open, "%d.", (int)msg[5]); //month
	memcpy(&no, &msg[6], 2);
	no = ntohs(no);
	fprintf(open,"%hu ", no); // year
	memcpy(&no, &msg[8], 2);
	no = ntohs(no);
	fprintf(open,"%hi ", no); // AA
	for (int i = 0; i < 12; i++)fprintf(open,"%c",msg[10 + i]); // tel
	fprintf(open," ");
	return 0;
	
}
void end(struct client* head) {
	while (head) {
		s_close(head->cs);
		head = head->next;
	}
}

int recv_string(struct client *cl,FILE *open) {
	char buffer[1024];
	int curlen = 0; int rcv;
	rcv = recv(cl->cs, buffer, sizeof(buffer), 0);
	if (rcv == 0) {
		return 1;
	}
	if (cl->mode == 0) {
		if (rcv == 3) {
			buffer[3] = '\0';
			if (!strcmp(buffer, "put"))cl->mode = 1;
			return 1;
		}
		else {
			char tmp = buffer[3];
			buffer[3] = '\0';
			if (!strcmp(buffer, "put")) {
				cl->mode = 1;
				buffer[3] = tmp;
				cl->answer++;
				parcing1(&buffer[3],open,cl);
				if (rcv < 1024) {
					fprintf(open, "%s", &buffer[25]);
					char* text = &buffer[25];
					int a = strcmp(text, "stop");
					if (a) {
						fprintf(open, "\n");
						return 1;
					}
					return 0;
				}
				for (int i = 25; i<1024; i++)fprintf(open, "%c", buffer[i]);
				while (rcv == 1024) {
					rcv = recv(cl->cs, buffer, sizeof(buffer), 0);
					for (int i = 0; i < rcv; i++) {
						if (buffer[i] == '\0')buffer[i] = '\n';
						fprintf(open, "%c", buffer[i]);
					}
				}
				return 1;
			}
		}
	}
	cl->answer++;
	parcing1(&buffer[0], open, cl);
	if (rcv < 1024) {
		fprintf(open, "%s", &buffer[22]);
		char* text = &buffer[22];
		int x = strcmp(text, "stop");
		if (x) {
			fprintf(open, "\n");
			return 1;
		}
		return 0;
	}
	for (int i = 22; i<1024; i++)fprintf(open, "%c", buffer[i]);
	while (rcv == 1024) {
		rcv = recv(cl->cs, buffer, sizeof(buffer), 0);
		for (int i = 0; i < rcv; i++) {
			if (buffer[i] == '\0')buffer[i] = '\n';
			fprintf(open, "%c", buffer[i]);
		}
	}
	return 1;
}

struct client* addnew(struct client* head) {
	struct client* tmp = (struct client*)calloc(1, sizeof(struct client));
	tmp->next = head;
	tmp->mode = 0;
	tmp->cs = 0;
	tmp->answer = 0;
	return tmp;
}
//
int main(int argc, char* argv[]) {
	int ls;
	char port[7];// = "9000";
	strcpy(port, argv[1]);
	FILE* open = fopen("msg.txt", "w");
	struct client* head = NULL;
	struct client* runner;
	struct sockaddr_in addr;
	// Инициалиазация сетевой библиотеки 
	init();
	// Создание TCP-сокета 
	ls = socket(AF_INET, SOCK_STREAM, 0);
	if (ls < 0)
		return sock_err("socket", ls);
	set_non_block_mode(ls);
	// Заполнение адреса прослушивания 
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(atoi(port)); // Сервер прослушивает порт 9000 
	addr.sin_addr.s_addr = htonl(INADDR_ANY); // Все адреса
	// Связывание сокета и адреса прослушивания 
	if (bind(ls, (struct sockaddr*)&addr, sizeof(addr)) < 0)
		return sock_err("bind", ls);


	struct timeval tv = { 1, 0 };
	fd_set rfd, wfd;
	int nfds = ls;
	// Начало прослушивания 
	bool go = 1;
	if (listen(ls, 1) < 0) return sock_err("listen", ls);
	do { // Принятие очередного подключившегося клиента 
		FD_ZERO(&rfd);
		FD_ZERO(&wfd);
		FD_SET(ls, &rfd);
		runner = head;
		while (runner) {
			FD_SET(runner->cs, &rfd);
			FD_SET(runner->cs, &wfd);
			if (nfds < runner->cs) nfds = runner->cs;
			runner = runner->next;
		}
		if (select(nfds + 1, &rfd, &wfd, 0, &tv) > 0) {
			if (FD_ISSET(ls, &rfd)) { // Есть события на прослушивающем сокете, можно вызвать accept, принять // подключение и добавить сокет подключившегося клиента в массив cs 
				int addrlen = sizeof(addr);
				head = addnew(head);
				head->cs = accept(ls, (struct sockaddr*)&addr, &addrlen);
				unsigned int ip;
				if (head->cs < 0) {
					sock_err("accept", ls);
					break;
				}
				set_non_block_mode(head->cs);
				ip = ntohl(addr.sin_addr.s_addr);
				head->ip = ip;
				strcpy(head->port,port);
			}
			runner = head;
			while (runner) {
				int a = FD_ISSET(runner->cs, &rfd);
				if (a&&runner) { // Сокет cs[i] доступен для чтения. Функция recv вернет данные, recvfrom - дейтаграмму 
					if (!recv_string(runner,open)) {
						go = 0;
						while (runner->answer) {
							send(runner->cs, "ok", 2, 0);
							runner->answer--;
						}
						break;
					}
				}
				int b = FD_ISSET(runner->cs, &wfd);
				if (b && runner->answer) { // Сокет cs[i] доступен для записи. Функция send и sendto будет успешно завершена 
					send(runner->cs, "ok", 2, 0);
					runner->answer--;
				}
				runner = runner->next;
			}
		}
	} while (go); // Повторение этого алгоритма в беск. цикле 
	end(head);
	runner = head;
	while (runner) {
		s_close(runner->cs);
		runner = runner->next;
	}
	s_close(ls);
	deinit();
	printf("Here");
	fclose(open);
	printf("close");
	return 0;
}
