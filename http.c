#include "http.h"

void echo_error(int fd, int error_num)
{
}
int startup(const char *ip, int port)
{
	int sock = socket(AF_INET, SOCK_STREAM,0);
	if (sock < 0)
	{
		perror("sock");
		return -2;
	}

	int opt = 1;
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	struct sockaddr_in local;
	local.sin_family = AF_INET;
	local.sin_port = htons(port);
	local.sin_addr.s_addr= inet_addr(ip);
	if (bind(sock, (struct sockaddr*)&local, sizeof(local))< 0)
	{
		perror("bind");
		return 3;
	}

	if(listen(sock ,10) < 0)
	{
		perror("listen");
		return -4;
	}

	return sock;
}

static int get_line(int fd, char *buf, int len)
{
	char c = '\0';
	int i = 0;
	while (c != '\n' && i <len-1)
	{
		ssize_t s = recv(fd, &c, 1, 0);
		if( s > 0 )
		{
           if (c == '\r')
		   {
			   recv(fd, &c , 1 , MSG_PEEK);
			   if (c == '\n')
			   {
				   recv(fd, &c, 1 ,0);
			   }
			   else
			   {
				  c = '\n';
			   }
		   }
                   buf[i++]=c;
		}
	}
	buf[i] = 0;
	return i;
}

void print_log(const char* msg, int level)
{
	const char* level_msg[] = {
		"NOTICE",
		"WARNING",
		"FATAL",
	};

	printf("[%s] [%s] \n", msg , level_msg[level]);
}

int echo_www(int fd, const char* path, int size)
{
	int new_fd = open(path, O_RDONLY);
	if (new_fd < 0)
	{
		print_log("open file error", FATAL);
		return 404;
	}

	const char *echo_line = "HTTP/1.0 200 OK\r\n";
	send(fd,echo_line, strlen(echo_line), 0 );
	const char * blank_line = "\r\n";
	send(fd,blank_line, strlen(blank_line), 0 );

	if(sendfile(fd, new_fd, NULL, size) < 0)	
	{
		print_log("send file error", FATAL);
		return 200;
	}

	close(new_fd);
    
}

void drop_header(int fd)
{
	char buff[SIZE];
	int ret = -1;
	do
	{
		ret = get_line(fd , buff, sizeof(buff));
	}while(ret > 0 && strcmp(buff, "\n"));
}

int exe_cgi(int fd, const char*method, \
		const char *path, const char * query_string)
{
	int content_len = -1;
	char METHOD[SIZE/10];
	char QUERY_STRING[SIZE];
	char CONTENT_LENGTH[SIZE];
	if( strcasecmp(method, "GET") == 0)
	{
		drop_header(fd);
	}
	else
	{
		char buff[SIZE];
		int ret = -1;
		do{
			ret = get_line(fd, buff, sizeof(buff));
			if (strncasecmp(buff, "Content-length: ", 16) == 0)
			{
				content_len = atoi(&buff[16]);
			}

		}while(ret>0 && strcmp(buff, "\n"));
		if(content_len == -1)
		{
			echo_error(fd, 401);
			return -1;
		}
	}
	printf("cgi : path: %s\n", path);
	int input[2];
	int output[2];
	if (pipe(input) < 0)
	{
		echo_error(fd, 401);
		return -2;
	}

	if (pipe(output) < 0)
	{
		echo_error(fd, 401);
		return -3;
	}
	const char *echo_line = "HTTP/1.0 200 OK\r\n";
	send(fd, echo_line, strlen(echo_line), 0);
	const char *type = "Content-Type:text/html;charset=utf-8\r\n";
	send(fd, type, strlen(type), 0);
	const char* blank_line= "\r\n";
	send(fd,blank_line,strlen(blank_line), 0);

	pid_t id = fork();
	if (id < 0)
	{
		echo_error(fd, 501);
		return -2;
	}
	else if( id == 0)
	{
		close(input[1]);
		close(output[0]);
		sprintf(METHOD, "METHOD=%s", method);
		putenv(METHOD);
		if(strcasecmp(method, "GET") == 0)
		{
			sprintf(QUERY_STRING, "QUERY_STRING=%s", query_string);
			putenv(QUERY_STRING);  //增加环境变量
		}
		else
		{
			sprintf(CONTENT_LENGTH, "QUERY_LENGTH=%d", content_len);
			putenv(CONTENT_LENGTH);
		}

		dup2(input[0], 0);
		dup2(output[1], 1);
		execl(path,path,NULL);
		exit(1);
	}
	else
	{
		close(input[0]);
		close(output[1]);
	int i = 0;
	char c ='\0';
	for (;i < content_len; i++)
	{
		recv(fd, &c, 1, 0);
		write(input[1], &c, 1);
	}

	while(1)
	{
		ssize_t s = read(output[0], &c, 1);
		if(s > 0)
		{
			send(fd, &c, 1, 0);
		}
		else
		{
			break;
		}
	}
	waitpid(id, NULL, 0);
	close(input[1]);
	close(output[0]);
	}
}

void *handler_request(void *arg)
{
	int fd = (int)arg;
	int errno_num = 200;
	int cgi = 0;
	char *query_string = NULL;
#ifdef _DEBUG_
	printf("#################################\n");
	char buff[SIZE];
	int ret = -1;
	do {
		ret= get_line(fd, buff , sizeof(buff));
		printf("%s", buff);
	}while(ret > 0 && strcmp(buff, "\n"));
		printf("#################################\n");
#else
	char method[SIZE/10];
	char url[SIZE];
	char path[SIZE];
	char buff[SIZE];
	int i , j;
	if (get_line(fd, buff, sizeof(buff)) <= 0)
	{
		print_log("get request line error ", FATAL);
		errno_num = 501;
		goto end;
	}
	i =0 , j =0 ;
	while ( i< sizeof(method) - 1 && j <  sizeof(buff) && \
			!isspace(buff[j]))
	{
		method[i] = buff[j];
		i++, j++;
	}
	method[i] = 0;
	// GET
	while (isspace(buff[j]) && j < sizeof(buff))
	{
		j++;
	}

	i = 0;

	while ( i< sizeof(url) && j <  sizeof(buff) && \
			!isspace(buff[j]))
	{
		url[i] = buff[j];
		i++, j++;
	}
    url[i] = 0;
	printf("method； %s, url: %s\n", method, url);

	if(strcasecmp(method, "GET") && strcasecmp(method, "POST"))
    {
		print_log("method is not ok!", FATAL);
		errno_num = 501;
		goto end;
	}

	if (strcasecmp(method, "POST") == 0)
	{
		cgi = 1;
	}
	//url
	query_string = url;
	while(*query_string != 0)
	{
		if (*query_string == '?')
		{	
			cgi = 1;
			*query_string = '\0';
			query_string++;
			break;
		}
		query_string ++;
	}

	sprintf(path, "wwwroot%s", url);

//	if (path[strlen(path) -1] == '/')
//	{
//		strcat(path, "index.html");
//		printf("*********************************\n");
//	}
	printf("path: %s\n", path);
	struct stat st;
	if( stat(path, &st) < 0)
	{
		print_log("path not found!", FATAL);
		errno_num = 404;
		goto end;
	}
	else
	{
		if (S_ISDIR(st.st_mode))
		{
			strcat(path,"/index.html");
		}
		else
		{
			if (( st.st_mode & S_IXUSR) ||\
				( st.st_mode & S_IXGRP) || \
				( st.st_mode & S_IXOTH) )
			{
				cgi =1;

			}
		}
		printf("%d",cgi);
		if (cgi)
		{
			exe_cgi(fd, method, path, query_string);
		}
		else
		{
			drop_header(fd);
			errno_num = echo_www(fd, path, st.st_size);
		}
	}
end:
	echo_error(fd, errno_num);
	close(fd);
#endif
}
