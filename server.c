#include "utils.h"

char dir_path[STR_BUF_SIZE] = "/tmp";

void ctrl_connect(int connfd)
{
	char recv_data[STR_BUF_SIZE];
	char cur_path[STR_BUF_SIZE] = "/";
	struct sockaddr_in PORT_addr;
	int flag = 0; // -1:PASV, 1:PORT
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
		else if (strcmp(recv_data, "PASV") == 0)
		{
			close(PASV_connfd);
			n = com_PASV(connfd, &PASV_connfd);
			flag = (n == 0) ? -1 : flag;
		}
		else if (strncmp(recv_data, "RETR", 4) == 0)
		{
			if (flag == 1)
			{
				int data_socket = conn_PORT(connfd, &PORT_addr);
				n = com_RETR(connfd, recv_data, data_socket, cur_path);
			}
			else if (flag == -1)
			{
				int data_socket = conn_PASV(connfd, &PASV_connfd);
				n = com_RETR(connfd, recv_data, data_socket, cur_path);
			}
			else
			{
				n = send_str(connfd, "503 Bad sequence of commands.\r\n");
			}
			flag = 0;
		}
		else if (strncmp(recv_data, "STOR", 4) == 0)
		{
			if (flag == 1)
			{
				int data_socket = conn_PORT(connfd, &PORT_addr);
				n = com_STOR(connfd, recv_data, data_socket, cur_path);
			}
			else if (flag == -1)
			{
				int data_socket = conn_PASV(connfd, &PASV_connfd);
				n = com_STOR(connfd, recv_data, data_socket, cur_path);
			}
			else
			{
				n = send_str(connfd, "503 Bad sequence of commands.\r\n");
			}
			flag = 0;
		}
		else if (strcmp(recv_data, "PWD") == 0)
		{
			n = com_PWD(connfd, recv_data, cur_path);
		}
		else if (strncmp(recv_data, "MKD", 3) == 0)
		{
			n = com_MKD(connfd, recv_data, cur_path);
		}
		else if (strncmp(recv_data, "CWD", 3) == 0)
		{
			n = com_CWD(connfd, recv_data, cur_path);
		}
		else if (strncmp(recv_data, "RMD", 3) == 0)
		{
			n = com_RMD(connfd, recv_data, cur_path);
		}
		else if (strncmp(recv_data, "RNFR", 4) == 0)
		{
			n = com_RNFR(connfd, recv_data, cur_path);
		}
		else if (strncmp(recv_data, "RNTO", 4) == 0)
		{
			n = send_str(connfd, "503 Bad sequence of commands.\r\n");
		}
		else if (strncmp(recv_data, "LIST", 4) == 0)
		{
			if (flag == 1)
			{
				int data_socket = conn_PORT(connfd, &PORT_addr);
				n = com_LIST(connfd, recv_data, data_socket, cur_path);
			}
			else if (flag == -1)
			{
				int data_socket = conn_PASV(connfd, &PASV_connfd);
				n = com_LIST(connfd, recv_data, data_socket, cur_path);
			}
			else
			{
				n = send_str(connfd, "503 Bad sequence of commands.\r\n");
			}
			flag = 0;
		}
		else if (strcmp(recv_data, "SYST") == 0)
		{
			n = com_SYST(connfd);
		}
		else if (strcmp(recv_data, "QUIT") == 0 || strcmp(recv_data, "ABOR") == 0)
		{
			n = com_QUIT(connfd);
			close(PASV_connfd);
		}
		else
		{
			n = send_str(connfd, "500 Unacceptable syntax.\r\n");
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
	int port = 21;
	for (int i = 2; i < argc; i += 2)
	{
		if (strcmp(argv[i - 1], "-port") == 0)
		{
			port = atoi(argv[i]);
		}
		else if (strcmp(argv[i - 1], "-root") == 0)
		{
			strcpy(dir_path, argv[i]);
		}
		else
		{
			printf("Error: Unacceptable arguments.\n");
			return 0;
		}
	}

	if (chdir(dir_path) == -1)
	{
		printf("Error chdir(): %s(%d)\n", strerror(errno), errno);
		return 0;
	}

	if (getcwd(dir_path, STR_BUF_SIZE) == NULL)
	{
		printf("Error getcwd(): %s(%d)\n", strerror(errno), errno);
		return 0;
	}

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
	addr.sin_port = htons(port);
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
