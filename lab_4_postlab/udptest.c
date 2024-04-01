#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

#define PORT 4501 // Port number to listen on

int main() {
    int sockfd;
    struct sockaddr_in6 servaddr, cliaddr;
    char buffer[1024];
    socklen_t len = sizeof(cliaddr);

    if ((sockfd = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&servaddr, 0, sizeof(servaddr));
    memset(&cliaddr, 0, sizeof(cliaddr));

    servaddr.sin6_family = AF_INET6; 
    servaddr.sin6_addr = in6addr_any; 
    servaddr.sin6_port = htons(PORT);


    if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", PORT);

    while (1) {
        int n = recvfrom(sockfd, (char *)buffer, sizeof(buffer), MSG_WAITALL, (struct sockaddr *)&cliaddr, &len);
        buffer[n] = '\0'; 
        printf("Received from client: %s\n", buffer);
        
        if(strcmp(buffer, "on\n") == 0){
            printf("on command received\n");
            sendto(sockfd, (const char *)"SUCCESS\n", strlen("SUCCESS\n"), 0, (const struct sockaddr *)&cliaddr, len);
        }
        else if(strcmp(buffer, "off\n")== 0 ){
            printf("off command received\n");
            sendto(sockfd, (const char *)"SUCCESS\n", strlen("SUCCESS\n"), 0, (const struct sockaddr *)&cliaddr, len);
        }
        else if(strcmp(buffer, "name\n")==0){
            printf("name command received\n");
            sendto(sockfd, (const char *)"Group 10\n", strlen("Group 10\n"), 0, (const struct sockaddr *)&cliaddr, len);
        }
        else{
             sendto(sockfd, (const char *)"Failed Command\n", strlen("Failed Command\n"), 0, (const struct sockaddr *)&cliaddr, len);
        }
    }


    return 0;
}
