#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define COUNT_RESPONSE "HTTP/1.1 404 OK\r\nContent-Type: text/html\r\n\r\n<html><body><h1>Hello, World! count %d </h1></body></html>"

#define S_PORT 58080
int time = 0;

int main() {

    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    char response[1024] = {0};

    // 创建Socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // 绑定地址和端口
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(S_PORT);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_socket);
        return -1;
    }

    // 监听连接
    if (listen(server_socket, 0) < 0) {
        perror("Listen failed");
        close(server_socket);
        return -1;
    }

    printf("Server listening on port %d...\n",S_PORT);

NEW_CON:
    // 接受客户端连接
    client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len);
    if (client_socket < 0) {
        perror("Accept failed");
        close(server_socket);
        return -1;
    }

    printf("Connection accepted from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

    // 处理客户端请求
    char buffer[1024] = {0};
    read(client_socket, buffer, 1024);
    printf("Request:\n%s\n", buffer);

    if(!strncmp(buffer,"GET / HTTP/1.1",strlen("GET / HTTP/1.1")))
    {
        // 发送响应
        sprintf(response,COUNT_RESPONSE,++time);
    }else{
        sprintf(response,COUNT_RESPONSE,time);
        
    }
    send(client_socket, response, strlen(response), 0);
    // 关闭连接
goto NEW_CON;
    close(server_socket);
    return 0;
}
