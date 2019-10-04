#include <sys/socket.h>
#include <netinet/in.h>

#include <unistd.h>
#include <errno.h>

#include <ctype.h>
#include <string.h>
#include <memory.h>
#include <stdio.h>
#include <pthread.h>

#define STR_BUF_SIZE 8192
#define DATA_BUF_SIZE 81920

int recv_str(int connfd, char data[], int size)
{
	//榨干socket传来的内容
	int p = 0;
	while (1)
	{
		int n = read(connfd, data + p, size - 1 - p);
		if (n <= 0)
		{
			printf("Error read(): %s(%d)\n", strerror(errno), errno);
			return 1;
		}
		else if (n == 0)
		{
			break;
		}
		else
		{
			p += n;
			if (data[p - 1] == '\n')
			{
				break;
			}
		}
	}

	//socket接收到的字符串并不会添加'\0'
	data[p - 2] = data[p - 1] = '\0';
	return 0;
}

int send_str(int connfd, char data[])
{
	int p = 0;
	int len = strlen(data);
	while (p < len)
	{
		int n = write(connfd, data + p, len - p);
		if (n < 0)
		{
			printf("Error write(): %s(%d)\n", strerror(errno), errno);
			return 1;
		}
		else
		{
			p += n;
		}
	}
	return 0;
}

int send_data(int connfd, char data[], int len)
{
	int p = 0;
	while (p < len)
	{
		int n = write(connfd, data + p, len - p);
		if (n < 0)
		{
			printf("Error write(): %s(%d)\n", strerror(errno), errno);
			return 1;
		}
		else
		{
			p += n;
		}
	}
	return 0;
}

int com_USER(int connfd, char recv_data[])
{
	// username

	if (strcmp(recv_data + 5, "anonymous"))
	{
		send_str(connfd, "530 Authentication failed.\r\n");
		return 0;
	}

	if (send_str(connfd, "331 Guest login ok, send your complete e-mail address as password.\r\n"))
		return 1;

	if (recv_str(connfd, recv_data, STR_BUF_SIZE))
		return 1;

	if (strncmp(recv_data, "PASS", 4))
		return 1;

	// password
	if (send_str(connfd, "230 Guest login ok, access restrictions apply.\r\n"))
		return 1;

	return 0;
}

int com_TYPE(int connfd, char recv_data[])
{
	if (strcmp(recv_data + 5, "I"))
		return 1;
	if (send_str(connfd, "200 Type set to I.\r\n"))
		return 1;
	return 0;
}

int com_PORT(int connfd, char recv_data[], struct sockaddr_in *addr)
{
	const char *s = ",";
	char *h1 = strtok(recv_data + 5, s);
	char *h2 = strtok(NULL, s);
	char *h3 = strtok(NULL, s);
	char *h4 = strtok(NULL, s);
	char *p1 = strtok(NULL, s);
	char *p2 = strtok(NULL, s);
	if (!(h1 && h2 && h3 && h4 && p1 && p2))
		return 1;

	char ip[16];
	sprintf(ip, "%s.%s.%s.%s", h1, h2, h3, h4);
	int port = atoi(p1) * 256 + atoi(p2);

	memset(addr, 0, sizeof(struct sockaddr_in));
	addr->sin_family = AF_INET;
	addr->sin_port = htons(port);
	addr->sin_addr.s_addr = inet_addr(ip); //监听"0.0.0.0"

	if (send_str(connfd, "200 PORT command successful.\r\n"))
		return 1;

	return 0;
}

int com_RETR(int connfd, char recv_data[], struct sockaddr_in *addr)
{
	char *filename = recv_data + 5;
	if (send_str(connfd, "150 File status okay. About to open data connection.\r\n"))
		return 1;

	int data_socket;

	if ((data_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
	{
		printf("Error dataSocket(): %s(%d)\n", strerror(errno), errno);
		if (send_str(connfd, "425 Can't open data connection.\r\n"))
			return 1;
		return 0;
	}

	if (connect(data_socket, addr, sizeof(struct sockaddr)) < 0)
	{
		printf("Error dataConnect(): %s(%d)\n", strerror(errno), errno);
		if (send_str(connfd, "425 Can't open data connection.\r\n"))
			return 1;
		return 0;
	}

	FILE *fp;

	if ((fp = fopen(filename, "r")) == NULL)
	{
		close(data_socket);
		if (send_str(connfd, "451 Requested action aborted. Local error in processing.\r\n"))
			return 1;
		return 0;
	}

	char buf[DATA_BUF_SIZE];
	int ret;
	// memset(buf, 1, sizeof(buf));

	while (1)
	{
		ret = fread(buf, 1, DATA_BUF_SIZE, fp);

		if (send_data(data_socket, buf, ret))
		{
			close(data_socket);
			fclose(fp);
			if (send_str(connfd, "426 Connection closed; transfer aborted.\r\n"))
				return 1;
			return 0;
		}

		if (ferror(fp))
		{
			close(data_socket);
			fclose(fp);
			if (send_str(connfd, "451 Requested action aborted. Local error in processing.\r\n"))
				return 1;
			return 0;
		}

		if (feof(fp))
			break;
	}

	close(data_socket);
	fclose(fp);

	if (send_str(connfd, "226 Transfer complete.\r\n"))
		return 1;

	return 0;
}

int com_SYST(int connfd)
{
	if (send_str(connfd, "215 UNIX Type: L8\r\n"))
		return 1;
	return 0;
}

void new_connect(int connfd)
{
	int p, len;
	char recv_data[STR_BUF_SIZE];
	struct sockaddr_in data_addr;

	if (send_str(connfd, "220 Anonymous FTP server ready.\r\n"))
	{
		close(connfd);
		return;
	}

	if (recv_str(connfd, recv_data, STR_BUF_SIZE))
	{
		close(connfd);
		return;
	}

	if (strncmp(recv_data, "USER", 4))
	{
		close(connfd);
		return;
	}
	else if (com_USER(connfd, recv_data))
	{
		close(connfd);
		return;
	}

	while (1)
	{
		if (recv_str(connfd, recv_data, STR_BUF_SIZE))
		{
			close(connfd);
			return;
		}

		int n = 0;

		if (strncmp(recv_data, "USER", 4) == 0)
		{
			n = com_USER(connfd, recv_data);
		}
		else if (strncmp(recv_data, "TYPE", 4) == 0)
		{
			n = com_TYPE(connfd, recv_data);
		}
		else if (strncmp(recv_data, "PORT", 4) == 0)
		{
			n = com_PORT(connfd, recv_data, &data_addr);
		}
		else if (strncmp(recv_data, "RETR", 4) == 0)
		{
			n = com_RETR(connfd, recv_data, &data_addr);
		}
		else if (strcmp(recv_data, "SYST") == 0)
		{
			n = com_SYST(connfd);
		}
		else
		{
			n = 1;
		}

		printf("%s", recv_data);

		if (n)
		{
			close(connfd);
			return;
		}
	}
}

int main(int argc, char **argv)
{
	int listenfd, connfd; //监听socket和连接socket不一样，后者用于数据传输
	struct sockaddr_in addr;

	//创建socket
	if ((listenfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
	{
		printf("Error socket(): %s(%d)\n", strerror(errno), errno);
		return 1;
	}

	//设置本机的ip和port
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(6789);
	addr.sin_addr.s_addr = htonl(INADDR_ANY); //监听"0.0.0.0"

	//将本机的ip和port与socket绑定
	if (bind(listenfd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
	{
		printf("Error bind(): %s(%d)\n", strerror(errno), errno);
		return 1;
	}

	//开始监听socket
	if (listen(listenfd, 10) == -1)
	{
		printf("Error listen(): %s(%d)\n", strerror(errno), errno);
		return 1;
	}

	//持续监听连接请求
	while (1)
	{
		//等待client的连接 -- 阻塞函数
		if ((connfd = accept(listenfd, NULL, NULL)) == -1)
		{
			printf("Error accept(): %s(%d)\n", strerror(errno), errno);
			continue;
		}

		pthread_t thid;
		pthread_create(&thid, NULL, (void *)new_connect, connfd);

		// //字符串处理
		// for (p = 0; p < len; p++) {
		// 	sentence[p] = toupper(sentence[p]);
		// }
	}

	close(listenfd);
}
