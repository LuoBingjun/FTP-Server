#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <memory.h>
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <regex.h>

#include <sys/socket.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#define STR_BUF_SIZE 8192
#define DATA_BUF_SIZE 8192

extern char dir_path[STR_BUF_SIZE];

int recv_str(int connfd, char data[], int size);
int send_str(int connfd, char data[]);
int send_data(int connfd, char data[], int len);
int recv_data(int connfd, char data[], int size);

int com_USER(int connfd, char recv_data[]);
int com_TYPE(int connfd, char recv_data[]);
int com_PORT(int connfd, char recv_data[], struct sockaddr_in *addr);
int com_PASV(int connfd, int *PASV_connfd);
int conn_PORT(int connfd, struct sockaddr_in *addr);
int conn_PASV(int connfd, int *listenfd);
int com_REST(int connfd, char recv_data[], int* REST);
int com_RETR(int connfd, char recv_data[], int data_socket, char cur_path[], int REST);
int com_STOR(int connfd, char data[], int data_socket, char cur_path[]);
int com_APPE(int connfd, char data[], int data_socket, char cur_path[]);
int com_CWD(int connfd, char data[], char cur_path[]);
int com_PWD(int connfd, char data[], char cur_path[]);
int com_MKD(int connfd, char data[], char cur_path[]);
int com_RMD(int connfd, char data[], char cur_path[]);
int com_DELE(int connfd, char data[], char cur_path[]);
int com_RNFR(int connfd, char data[], char cur_path[]);
int com_LIST(int connfd, char data[], int data_socket, char cur_path[]);
int com_SYST(int connfd);
int com_ABOR(int connfd);
int com_QUIT(int connfd);