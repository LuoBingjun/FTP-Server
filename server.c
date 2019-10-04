#include <sys/socket.h>
#include <netinet/in.h>

#include <unistd.h>
#include <errno.h>

#include <ctype.h>
#include <string.h>
#include <memory.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/select.h>
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
	if (inet_pton(AF_INET, ip, &addr->sin_addr) <= 0)
	{
		printf("Error inet_pton(): %s(%d)\n", strerror(errno), errno);
		return 1;
	}
	// addr->sin_addr.s_addr = inet_addr(ip); //监听"0.0.0.0"

	if (send_str(connfd, "200 PORT command successful.\r\n"))
		return 1;

	return 0;
}

int com_PASV(int connfd, int *PASV_connfd)
{
	// 获取本地地址
	int h1, h2, h3, h4;
	struct sockaddr_in local_addr;
	int addr_len = sizeof(local_addr);
	memset(&local_addr, 0, sizeof(local_addr));
	getsockname(connfd, (struct sockaddr *)&local_addr, &addr_len);
	char ip[16];
	if (inet_ntop(AF_INET, (void *)&local_addr.sin_addr, ip, 16) == 0)
	{
		printf("Error inet_ntop(): %s(%d)\n", strerror(errno), errno);
		return 1;
	}
	sscanf(ip, "%d.%d.%d.%d", &h1, &h2, &h3, &h4);

	int listenfd;
	if ((listenfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
	{
		printf("Error PASV socket(): %s(%d)\n", strerror(errno), errno);
		return 1;
	}

	struct sockaddr_in listen_addr;
	memset(&listen_addr, 0, sizeof(listen_addr));
	listen_addr.sin_family = AF_INET;
	listen_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	listen_addr.sin_port = htons(0);

	//将本机的ip和port与socket绑定
	if (bind(listenfd, (struct sockaddr *)&listen_addr, sizeof(listen_addr)) == -1)
	{
		printf("Error PASV bind(): %s(%d)\n", strerror(errno), errno);
		return 1;
	}

	//开始监听socket
	if (listen(listenfd, 10) == -1)
	{
		printf("Error PASV listen(): %s(%d)\n", strerror(errno), errno);
		return 1;
	}

	// 获取监听端口
	int port, p1, p2;
	getsockname(listenfd, (struct sockaddr *)&local_addr, &addr_len);
	port = ntohs(local_addr.sin_port);
	p1 = port / 256;
	p2 = port % 256;

	char ret[64];
	sprintf(ret, "227 Entering passive mode (%d,%d,%d,%d,%d,%d).\r\n", h1, h2, h3, h4, p1, p2);
	if (send_str(connfd, ret))
	{
		close(listenfd);
		return 1;
	}

	*PASV_connfd = listenfd;

	// while (1)
	// {
	// 	//等待client的连接 -- 阻塞函数
	// 	if ((*PASV_connfd = accept(listenfd, NULL, NULL)) == -1)
	// 		printf("Error PASV accept(): %s(%d)\n", strerror(errno), errno);
	// 	else
	// 		break;
	// }

	// close(listenfd);
	return 0;
}

int conn_PORT(int connfd, struct sockaddr_in *addr)
{
	int data_socket;

	if ((data_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
	{
		printf("Error dataSocket(): %s(%d)\n", strerror(errno), errno);
		if (send_str(connfd, "425 Can't open data connection.\r\n"))
			return -2;
		return -1;
	}

	if (connect(data_socket, addr, sizeof(struct sockaddr)) < 0)
	{
		printf("Error dataConnect(): %s(%d)\n", strerror(errno), errno);
		if (send_str(connfd, "425 Can't open data connection.\r\n"))
			return -2;
		return -1;
	}

	return data_socket;
}

int conn_PASV(int connfd, int *listenfd)
{
	int data_socket;
	//等待client的连接 -- 阻塞函数
	if ((data_socket = accept(*listenfd, NULL, NULL)) == -1)
		printf("Error PASV accept(): %s(%d)\n", strerror(errno), errno);

	close(*listenfd);
	listenfd = -1;
	return data_socket;
}

int com_RETR(int connfd, char recv_data[], int data_socket)
{
	if (data_socket == -1)
		return 0;
	else if(data_socket == -2)
		return 1;

	char *filename = recv_data + 5;
	FILE *fp;
	if ((fp = fopen(filename, "r")) == NULL)
	{
		close(data_socket);
		if (send_str(connfd, "451 Requested action aborted. Local error in processing.\r\n"))
			return 1;
		return 0;
	}

	if (send_str(connfd, "150 File status okay. About to send data.\r\n"))
		return 1;

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

void ctrl_connect(int connfd)
{
	char recv_data[STR_BUF_SIZE];

	int flag = 0; //-1为PASV, 1为PORT
	struct sockaddr_in PORT_addr;
	int PASV_connfd = -1;

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
			close(PASV_connfd);
			n = com_PORT(connfd, recv_data, &PORT_addr);
			flag = (n == 0) ? 1 : flag;
		}
		else if (strncmp(recv_data, "RETR", 4) == 0)
		{
			if (flag == 1)
			{
				int data_socket = conn_PORT(connfd, &PORT_addr);
				n = com_RETR(connfd, recv_data, data_socket);
			}
			else if (flag == -1)
			{
				int data_socket = conn_PASV(connfd, &PASV_connfd);
				n = com_RETR(connfd, recv_data, data_socket);
			}
			else
			{
			}
			flag = 0;
		}
		else if (strcmp(recv_data, "PASV") == 0)
		{
			close(PASV_connfd);
			n = com_PASV(connfd, &PASV_connfd);
			flag = (n == 0) ? -1 : flag;
		}
		else if (strcmp(recv_data, "SYST") == 0)
		{
			n = com_SYST(connfd);
		}
		else
		{
			n = send_str(connfd, "500 unrecongnized data\r\n");
		}

		printf("%s\n", recv_data);

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
		pthread_create(&thid, NULL, (void *)ctrl_connect, connfd);

		// //字符串处理
		// for (p = 0; p < len; p++) {
		// 	sentence[p] = toupper(sentence[p]);
		// }
	}

	close(listenfd);
}
