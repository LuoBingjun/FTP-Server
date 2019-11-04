#include "utils.h"

// 以下为数据收发函数

int recv_str(int connfd, char data[], int size)
{
	//榨干socket传来的内容
	int p = 0;
	while (1)
	{
		int n = read(connfd, data + p, size - 1 - p);
		if (n < 0)
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
		int n = send(connfd, data + p, len - p, MSG_NOSIGNAL);
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
		int n = send(connfd, data + p, len - p, MSG_NOSIGNAL);
		if (n <= 0)
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

int recv_data(int connfd, char data[], int size)
{
	//榨干socket传来的内容
	int p = 0;
	while (1)
	{
		int n = read(connfd, data + p, size - 1 - p);
		if (n < 0)
		{
			printf("Error read(): %s(%d)\n", strerror(errno), errno);
			return -1;
		}
		else if (n == 0)
		{
			break;
		}
		else
		{
			p += n;
		}
	}

	return p;
}

// 以下为命令处理函数

int com_USER(int connfd, char recv_data[])
{
	// username

	if (strcmp(recv_data + 5, "anonymous"))
	{
		send_str(connfd, "530 Authentication failed.\r\n");
		return 1;
	}

	if (send_str(connfd, "331 Guest login ok, send your complete e-mail address as password.\r\n"))
		return 1;

	if (recv_str(connfd, recv_data, STR_BUF_SIZE))
		return 1;

	if (strncmp(recv_data, "PASS", 4))
		return 1;

	recv_data += 5;
	regex_t reg;
	regmatch_t pmatch[1];
	regcomp(&reg, "^[a-zA-Z0-9_-]+@[a-zA-Z0-9_-]*(\\.[a-zA-Z0-9_-]+)*$", REG_EXTENDED);
	if (regexec(&reg, recv_data, 1, pmatch, 0) == REG_NOMATCH)
	{
		send_str(connfd, "530 Authentication failed.\r\n");
		return 1;
	}

	if (send_str(connfd, "230 Guest login ok, access restrictions apply.\r\n"))
		return 1;

	return 0;
}

int com_TYPE(int connfd, char recv_data[])
{
	if (strcmp(recv_data + 5, "I") == 0)
	{
		if (send_str(connfd, "200 Type set to I.\r\n"))
			return 1;
	}
	else
	{
		if (send_str(connfd, "504 Unsupported type.\r\n"))
			return 1;
	}
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
	unsigned addr_len = sizeof(local_addr);
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

	if (connect(data_socket, (struct sockaddr *)addr, sizeof(struct sockaddr)) < 0)
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
	*listenfd = -1;
	return data_socket;
}

int com_REST(int connfd, char recv_data[], int *REST)
{
	recv_data += 4;
	while (*recv_data == ' ' && *recv_data != '\0')
		recv_data++;

	*REST = atoi(recv_data);
	if (send_str(connfd, "350 Requested file action pending further information.\r\n"))
		return 1;
	return 0;
}

int com_RETR(int connfd, char recv_data[], int data_socket, char cur_path[], int REST)
{
	if (data_socket == -1)
		return 0;
	else if (data_socket == -2)
		return 1;

	char *filename = recv_data + 4;
	while (*filename == ' ' && *filename != '\0')
		filename++;

	if (strstr(filename, ".."))
	{
		close(data_socket);
		if (send_str(connfd, "550 Access denied.\r\n"))
			return 1;
		return 0;
	}

	FILE *fp;
	char buf[DATA_BUF_SIZE] = {0};
	int ret;

	if (*filename == '/')
		sprintf(buf, "./%s", filename);
	else
		sprintf(buf, "./%s/%s", cur_path, filename);

	if ((fp = fopen(buf, "r")) == NULL)
	{
		close(data_socket);
		sprintf(buf, "550 %s.\r\n", strerror(errno));
		if (send_str(connfd, buf))
			return 1;
		return 0;
	}

	fseek(fp, REST, SEEK_SET);

	if (send_str(connfd, "150 File status okay. About to send data.\r\n"))
		return 1;

	while (1)
	{
		ret = fread(buf, 1, DATA_BUF_SIZE, fp);

		if (send_data(data_socket, buf, ret))
		{

			if (send_str(connfd, "426 Connection closed; transfer aborted.\r\n"))
			{
				fclose(fp);
				close(data_socket);
				return 1;
			}
			break;
		}

		if (ferror(fp))
		{
			if (send_str(connfd, "451 Requested action aborted. Local error in processing.\r\n"))
			{
				fclose(fp);
				close(data_socket);
				return 1;
			}
			break;
		}

		if (feof(fp))
			break;
	}

	fclose(fp);
	close(data_socket);

	if (send_str(connfd, "226 Transfer complete.\r\n"))
		return 1;

	return 0;
}

int com_STOR(int connfd, char data[], int data_socket, char cur_path[])
{
	if (data_socket == -1)
		return 0;
	else if (data_socket == -2)
		return 1;

	char *filename = data + 4;
	while (*filename == ' ' && *filename != '\0')
		filename++;

	if (strstr(filename, ".."))
	{
		close(data_socket);
		if (send_str(connfd, "550 Access denied.\r\n"))
			return 1;
		return 0;
	}

	FILE *fp;
	char buf[DATA_BUF_SIZE] = {0};

	if (*filename == '/')
		sprintf(buf, "./%s", filename);
	else
		sprintf(buf, "./%s/%s", cur_path, filename);

	if ((fp = fopen(buf, "wb")) == NULL)
	{
		close(data_socket);
		sprintf(buf, "550 %s.\r\n", strerror(errno));
		if (send_str(connfd, buf))
			return 1;
		return 0;
	}

	if (send_str(connfd, "150 File status okay. About to receive data.\r\n"))
		return 1;

	while (1)
	{

		int p = recv_data(data_socket, buf, DATA_BUF_SIZE);
		if (p > 0)
		{
			fwrite(buf, 1, p, fp);
			if (ferror(fp))
			{
				close(data_socket);
				fclose(fp);
				if (send_str(connfd, "451 Requested action aborted. Local error in processing.\r\n"))
					return 1;
				break;
			}
		}
		else if (p == 0)
		{
			break;
		}
		else
		{
			if (send_str(connfd, "426 Connection closed; transfer aborted.\r\n"))
				return 1;
			break;
		}
	}

	close(data_socket);
	fclose(fp);

	if (send_str(connfd, "226 Transfer complete.\r\n"))
		return 1;

	return 0;
}

int com_APPE(int connfd, char data[], int data_socket, char cur_path[])
{
	if (data_socket == -1)
		return 0;
	else if (data_socket == -2)
		return 1;

	char *filename = data + 4;
	while (*filename == ' ' && *filename != '\0')
		filename++;

	if (strstr(filename, ".."))
	{
		close(data_socket);
		if (send_str(connfd, "550 Access denied.\r\n"))
			return 1;
		return 0;
	}

	FILE *fp;
	char buf[DATA_BUF_SIZE] = {0};

	if (*filename == '/')
		sprintf(buf, "./%s", filename);
	else
		sprintf(buf, "./%s/%s", cur_path, filename);

	if ((fp = fopen(buf, "ab")) == NULL)
	{
		close(data_socket);
		sprintf(buf, "550 %s.\r\n", strerror(errno));
		if (send_str(connfd, buf))
			return 1;
		return 0;
	}

	if (send_str(connfd, "150 File status okay. About to receive data.\r\n"))
		return 1;

	while (1)
	{

		int p = recv_data(data_socket, buf, DATA_BUF_SIZE);
		if (p > 0)
		{
			fwrite(buf, 1, p, fp);
			if (ferror(fp))
			{
				close(data_socket);
				fclose(fp);
				if (send_str(connfd, "451 Requested action aborted. Local error in processing.\r\n"))
					return 1;
				break;
			}
		}
		else if (p == 0)
		{
			break;
		}
		else
		{
			if (send_str(connfd, "426 Connection closed; transfer aborted.\r\n"))
				return 1;
			break;
		}
	}

	close(data_socket);
	fclose(fp);

	if (send_str(connfd, "226 Transfer complete.\r\n"))
		return 1;

	return 0;
}

int com_PWD(int connfd, char data[], char cur_path[])
{
	char res[STR_BUF_SIZE] = {0};
	sprintf(res, "257 \"%s\" is the current directory.\r\n", cur_path);
	if (send_str(connfd, res))
		return 1;
	return 0;
}

int com_MKD(int connfd, char data[], char cur_path[])
{
	char *subpath = data + 3;
	while (*subpath == ' ' && *subpath != '\0')
		subpath++;

	if (strstr(subpath, ".."))
	{
		if (send_str(connfd, "550 Access denied.\r\n"))
			return 1;
		return 0;
	}

	char dirname[STR_BUF_SIZE] = {0};
	char buf[STR_BUF_SIZE] = {0};

	if (*subpath == '/')
		sprintf(dirname, "./%s", subpath);
	else
		sprintf(dirname, "./%s/%s", cur_path, subpath);

	if (mkdir(dirname, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == -1)
	{
		sprintf(buf, "550 %s.\r\n", strerror(errno));
		if (send_str(connfd, buf))
			return 1;
		return 0;
	}

	realpath(dirname, buf);
	strcpy(dirname, buf + strlen(dir_path));

	sprintf(buf, "257 \"%s\" directory created.\r\n", dirname);
	if (send_str(connfd, buf))
		return 1;

	return 0;
}

int com_CWD(int connfd, char data[], char cur_path[])
{
	char *subpath = data + 3;
	while (*subpath == ' ' && *subpath != '\0')
		subpath++;

	char buf[STR_BUF_SIZE] = {0};
	char abs_path[STR_BUF_SIZE] = {0};

	if (*subpath == '/')
		sprintf(buf, "./%s", subpath);
	else
		sprintf(buf, "./%s/%s", cur_path, subpath);

	if (realpath(buf, abs_path) == NULL)
	{
		sprintf(buf, "550 %s.\r\n", strerror(errno));
		if (send_str(connfd, buf))
			return 1;
		return 0;
	}

	int p = strlen(dir_path);

	if (strncmp(dir_path, abs_path, p) == 0)
	{
		if (strlen(abs_path) == p)
			strcpy(cur_path, "/");
		else
			strcpy(cur_path, abs_path + p);
	}

	sprintf(buf, "250 directory changed to %s\r\n", cur_path);
	// printf("directory changed to %s\n", cur_path);
	if (send_str(connfd, buf))
		return 1;

	return 0;
}

int com_RMD(int connfd, char data[], char cur_path[])
{
	char *subpath = data + 3;
	while (*subpath == ' ' && *subpath != '\0')
		subpath++;

	if (strstr(subpath, ".."))
	{
		if (send_str(connfd, "550 Access denied.\r\n"))
			return 1;
		return 0;
	}

	char dirname[STR_BUF_SIZE] = {0};
	char buf[STR_BUF_SIZE] = {0};

	if (*subpath == '/')
		sprintf(dirname, "./%s", subpath);
	else
		sprintf(dirname, "./%s/%s", cur_path, subpath);

	if (realpath(dirname, buf) == NULL)
	{
		sprintf(buf, "550 %s.\r\n", strerror(errno));
		if (send_str(connfd, buf))
			return 1;
		return 0;
	}

	if (strcmp(buf, dir_path) == 0)
	{
		if (send_str(connfd, "550 Access denied.\r\n"))
			return 1;
		return 0;
	}

	if (rmdir(dirname) == -1)
	{
		sprintf(buf, "550 %s.\r\n", strerror(errno));
		if (send_str(connfd, buf))
			return 1;
		return 0;
	}

	if (send_str(connfd, "250 Directory removed.\r\n"))
		return 1;
	return 0;
}

int com_DELE(int connfd, char data[], char cur_path[])
{
	char *subpath = data + 4;
	while (*subpath == ' ' && *subpath != '\0')
		subpath++;

	if (strstr(subpath, ".."))
	{
		if (send_str(connfd, "550 Access denied.\r\n"))
			return 1;
		return 0;
	}

	char filename[STR_BUF_SIZE] = {0};
	char buf[STR_BUF_SIZE] = {0};

	if (*subpath == '/')
		sprintf(filename, "./%s", subpath);
	else
		sprintf(filename, "./%s/%s", cur_path, subpath);

	if (realpath(filename, buf) == NULL)
	{
		sprintf(buf, "550 %s.\r\n", strerror(errno));
		if (send_str(connfd, buf))
			return 1;
		return 0;
	}

	if (strcmp(buf, dir_path) == 0)
	{
		if (send_str(connfd, "550 Access denied.\r\n"))
			return 1;
		return 0;
	}

	if (unlink(filename) == -1)
	{
		sprintf(buf, "550 %s.\r\n", strerror(errno));
		if (send_str(connfd, buf))
			return 1;
		return 0;
	}

	if (send_str(connfd, "250 File removed.\r\n"))
		return 1;
	return 0;
}

int com_RNFR(int connfd, char data[], char cur_path[])
{
	char *subpath = data + 4;
	while (*subpath == ' ' && *subpath != '\0')
		subpath++;

	if (strstr(subpath, ".."))
	{
		if (send_str(connfd, "550 Access denied.\r\n"))
			return 1;
		return 0;
	}

	char oldname[STR_BUF_SIZE] = {0};
	char newname[STR_BUF_SIZE] = {0};
	char buf[STR_BUF_SIZE] = {0};

	if (*subpath == '/')
		sprintf(buf, "./%s", subpath);
	else
		sprintf(buf, "./%s/%s", cur_path, subpath);

	if (realpath(buf, oldname) == NULL)
	{
		sprintf(buf, "550 %s.\r\n", strerror(errno));
		if (send_str(connfd, buf))
			return 1;
		return 0;
	}

	if (strcmp(dir_path, oldname) == 0)
	{
		if (send_str(connfd, "550 Access denied.\r\n"))
			return 1;
		return 0;
	}

	if (send_str(connfd, "350 Ready for destination name.\r\n"))
		return 1;

	if (recv_str(connfd, data, STR_BUF_SIZE))
		return 1;

	if (strncmp(data, "RNTO", 4))
	{
		if (send_str(connfd, "503 Bad sequence of commands.\r\n"))
			return 1;
		return 0;
	}

	subpath = data + 4;
	while (*subpath == ' ' && *subpath != '\0')
		subpath++;

	if (strstr(subpath, ".."))
	{
		if (send_str(connfd, "550 Access denied.\r\n"))
			return 1;
		return 0;
	}

	if (*subpath == '/')
		sprintf(newname, "./%s", subpath);
	else
		sprintf(newname, "./%s/%s", cur_path, subpath);

	if (rename(oldname, newname) == -1)
	{
		sprintf(buf, "550 %s.\r\n", strerror(errno));
		if (send_str(connfd, buf))
			return 1;
		return 0;
	}

	if (send_str(connfd, "250 Renaming ok.\r\n"))
		return 1;
	return 0;
}

int com_LIST(int connfd, char data[], int data_socket, char cur_path[])
{
	if (data_socket == -1)
		return 0;
	else if (data_socket == -2)
		return 1;

	char *subpath = data + 4;
	while (*subpath == ' ' && *subpath != '\0')
		subpath++;

	char buf[DATA_BUF_SIZE] = {0};
	char abs_path[STR_BUF_SIZE] = {0};

	sprintf(buf, "./%s/%s", cur_path, subpath);
	if (realpath(buf, abs_path) == NULL)
	{
		close(data_socket);
		sprintf(buf, "550 %s.\r\n", strerror(errno));
		if (send_str(connfd, buf))
			return 1;
		return 0;
	}

	if (strncmp(dir_path, abs_path, strlen(dir_path)))
	{
		close(data_socket);
		if (send_str(connfd, "550 No such file or directory.\r\n"))
			return 1;
		return 0;
	}

	sprintf(buf, "ls -l '%s'", abs_path);
	FILE *fp;

	if ((fp = popen(buf, "r")) == NULL)
	{
		close(data_socket);
		if (send_str(connfd, "451 Requested action aborted. Local error in processing.\r\n"))
			return 1;
		return 0;
	}

	if (send_str(connfd, "150 Data status okay. About to send data.\r\n"))
		return 1;

	while (1)
	{

		if (fgets(buf, DATA_BUF_SIZE, fp) == NULL)
		{
			if (feof(fp))
				break;
			else
			{
				close(data_socket);
				pclose(fp);
				if (send_str(connfd, "451 Requested action aborted. Local error in processing.\r\n"))
					return 1;
				return 0;
			}
		}

		if (buf[0] != 'd' && buf[0] != '-')
			continue;

		buf[strlen(buf) - 1] = '\0';
		strcat(buf, "\r\n");

		if (send_str(data_socket, buf))
		{
			close(data_socket);
			fclose(fp);
			if (send_str(connfd, "426 Connection closed; transfer aborted.\r\n"))
				return 1;
			return 0;
		}
	}

	pclose(fp);
	close(data_socket);

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

int com_ABOR(int connfd)
{
	if (send_str(connfd, "426 Connection closed; transfer aborted.\r\n"))
		return 1;
	return 0;
}

int com_QUIT(int connfd)
{
	send_str(connfd, "221 Goodbye.\r\n");
	return 1;
}