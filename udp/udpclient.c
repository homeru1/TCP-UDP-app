#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN 
#include <windows.h> 
#include <winsock2.h> // Директива линковщику: использовать библиотеку сокетов 
#pragma comment(lib, "ws2_32.lib")

#include <stdio.h> 
#include <string.h>
#include <malloc.h>
#include <stdlib.h>

int init() {
	WSADATA wsa_data;
	return (0 == WSAStartup(MAKEWORD(2, 2), &wsa_data));
}

void deinit() {
	WSACleanup();
}

int sock_err(const char* function, int s) {
	int err;
	err = WSAGetLastError();
	fprintf(stderr, "%s: socket error: %d\n", function, err);
	return -1;
}

void s_close(int s) {
	closesocket(s);
}

int GetSize(FILE* file) {
	int size = 0;
	char tmp = '0';
	for (size = 0; tmp != '\0'; size++) {
		tmp = fgetc(file);
		if (tmp == '\n' || tmp == EOF)break;
	}
	//if (tmp == EOF)size--;	
	size += 2;
	if (tmp == EOF)fseek(file, -(size - 1), SEEK_CUR);
	else fseek(file, -(size + 1), SEEK_CUR);
	return size;
}
int GetStart(FILE* file) {
	int size = 0;
	char tmp = '0';
	int space = 0;
	for (size = 0; space != 3; size++) {
		tmp = fgetc(file);
		if (tmp == ' ')space++;
	}
	fseek(file, -(size), SEEK_CUR);
	size += 1;
	return size;
}

int Getmsg(int s, int number, struct sockaddr_in addr) {
	struct timeval tv = { 0, 100000 };
	fd_set rfd;
	FD_ZERO(&rfd);
	FD_SET(s, &rfd);
	int res = select(s + 1, &rfd, 0, 0, &tv);
	if (res > 0) {
		int a = FD_ISSET(s, &rfd);
		if (a > 0) {
			int j = 0;
			int addrlen = sizeof(addr);
			char buffer[1024];
			int received = recvfrom(s, buffer, sizeof(buffer), 0, (struct sockaddr*)&addr, &addrlen);
			if (received == -1)return received;
			memcpy(&j, buffer, 4);
			j = ntohl(j);
			printf("N:%d J:%d\n", number, j);
			if (number == j) {
				printf("Here\n");
				return 1;
			}
			else return 0;
		}
		else if (a == 0) return 0;
		else return -1;
	}
	return 0;
}

int SendMsg(FILE* file, int s, struct sockaddr_in addr) {
	int current = 0;
	char tmp = '0';
	int size = 0;
	int len = 0;
	int n = 0;
	char a[131072];
	char datagram[131072];
	while (current < 20) {
		char *b = fgets(a, sizeof(a), file);
		if(b==NULL)break;
		n = htonl(current);
		memcpy(datagram, &n, 4);
		datagram[4] = atoi(strtok(a, "."));
		datagram[5] = atoi(strtok(NULL, "."));
		unsigned short year = atoi(strtok(NULL, " "));
		year = htons(year);
		memcpy(&datagram[6], &year, 2);
		n = atoi(strtok(NULL, " "));
		n = htons(n);
		memcpy(&datagram[8], &n, 2);
		char* T = strtok(NULL, " ");
		memcpy(&datagram[10], T, 12);
		T = strtok(NULL, "\n\0");
		memcpy(&datagram[22], T, strlen(T));
		datagram[strlen(T)+22] = '\0';
		int boo = 0;
		while (!boo) {
			int res = sendto(s, datagram, strlen(T) + 23, 0, (struct sockaddr*)&addr, sizeof(struct sockaddr_in));
			if (res < 0)return 0;
			boo = Getmsg(s, current, addr);
			if (boo < 0)return sock_err("socket", s);
		}
		current++;
		if (strcmp(&datagram[22], "stop") == 0)break;
	}
	return current;
}

int main(int argc, char* argv[]) {
	char dec[3] = ":";
	char* ip, * port, * filename;
	filename = argv[2];
	ip = strtok(argv[1], dec);
	port = strtok(NULL, dec);
	FILE* file = fopen(filename, "rb");
	int s;
	struct sockaddr_in addr;
	int max;
	int check[20];
	// Инициалиазация сетевой библиотеки 
	init();
	// Создание UDP-сокета 
	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s < 0)
		return sock_err("socket", s);
	// Заполнение структуры с адресом удаленного узла 
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(atoi(port));
	addr.sin_addr.s_addr = inet_addr(ip);
	max = SendMsg(file, s, addr);
	if (max < 0)return sock_err("send error", s);
	fclose(file);
	s_close(s);
	deinit();
}
