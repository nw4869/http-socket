#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <string.h>
#include <unistd.h>

#define MAXBUFF 4096
#define LISTENQ 1024


void makeResponse(char *sendbuff, size_t buffLen, const char *recvLine, const char *ipbuff, int port) {
    char header[MAXBUFF], contentLength[MAXBUFF], data[MAXBUFF];

    strncpy(header,
                    "HTTP/1.1 200 OK\r\n"
                            "Server: nwhttpd/0.1\r\n"
                            //"Connection: close\r\n"
//                            "Connection: keep-alive\r\n",

                    , sizeof(header));
    snprintf(data, sizeof(data), "%sip: %s, port: %d",
                     recvLine, ipbuff, port);
    //add content-length to header
    snprintf(contentLength, sizeof(contentLength) - 40, "Content-Length: %zu\r\n", strlen(data));
    strncat(header, contentLength, sizeof(header) - strlen(contentLength));
    snprintf(sendbuff, buffLen, "%s\r\n%s\r\n\r\n", header, data);
}

void doit(int connfd)
{
    char sendbuff[MAXBUFF], recvLine[MAXBUFF], ipbuff[INET_ADDRSTRLEN];
    struct sockaddr_in addr;
    int port;
    ssize_t nRead;
    size_t nLeft;
    socklen_t len;

    printf("call doit(%d);\n", connfd);


    // get ip and port
    len = sizeof(addr);
    getpeername(connfd, (struct sockaddr*)&addr, &len);
    inet_ntop(AF_INET, &addr.sin_addr, ipbuff, sizeof(addr));
    port = ntohs(addr.sin_port);


    // read request
    char* ptr = recvLine;
    nLeft = sizeof(recvLine);
    while (nLeft > 0 && (nRead = read(connfd, ptr, nLeft)) > 0)
    {
        nLeft -= nRead;
        ptr += nRead;
        *ptr = 0;
        printf("read %zd\n", nRead);

        // find \r\n\r\n
        char *requestEnd = strstr(ptr- nRead, "\r\n\r\n");
        if (requestEnd)
        {
            printf("found request end.\n");
            makeResponse(sendbuff, sizeof(sendbuff), recvLine, ipbuff, port);

            printf("write: %s", sendbuff);
            write(connfd, sendbuff, strlen(sendbuff));

            // clear buffer
            ptr = recvLine;
            nLeft = sizeof(recvLine);
            nRead = 0;
            continue;
        }
    }
    if (nRead < 0)
    {
        printf("read error\n");
        return;
    }
    printf("doit end.\n");

}

int main(int argc, char **argv)
{
    int listenfd, connfd;
    socklen_t len;
    struct sockaddr_in servaddr, cliaddr;
    pid_t pid;

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0)
    {
        printf("listen error\n");
        return 1;
    }

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(4869);

    if (bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0)
    {
        printf("bind error\n");
        return 1;
    }

    printf("listening...\n");
    if (listen(listenfd, LISTENQ) < 0)
    {
        printf("listen error\n");
        return 1;
    }

    for (; ;)
    {
        len = sizeof(cliaddr);
        connfd = accept(listenfd, (struct sockaddr*)&cliaddr, &len);

        if ((pid = fork()) == 0)
        {
            close(listenfd);
            doit(connfd);
            close(connfd);
            return (0);
        }
        close(connfd);
    }
    close(listenfd);

    return 0;
}