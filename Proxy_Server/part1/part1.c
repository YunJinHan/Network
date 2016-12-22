/*
    2012036901 윤 진 한
    컴퓨터공학과
    Computer Network - Project 2 < Part 1 >
    2016.11.03
*/

#include <stdio.h>
#include <sys/types.h>   // definitions of a number of data types used in socket.h and netinet/in.h
#include <sys/socket.h>  // definitions of structures needed for sockets, e.g. sockaddr
#include <netinet/in.h>  // constants and structures needed for internet domain addresses, e.g. sockaddr_in
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>     // definitions of that deal attribute of a opened file in fcntl.h
#include <unistd.h>    // definitions of using file pointer in unistd.h
#include <sys/types.h> // definitions of a file function in sys/types.h

#include <time.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <netdb.h>

#define BUFFER_SIZE 1024
#define WEB_DATA_BUFFER_SIZE 60000
#define END_SEVER_PORT_NUM 80

void error(char *msg){
    perror(msg);
    exit(1);
}

char *getCurrentTime(void){
    char timeString[3][2];
    char week[7][4] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
    char month[12][4] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
    // day and month string.
    char *TIME = (char *)malloc(sizeof(char)*100);
    // array to write time.
    struct tm *t;
    time_t timer;
    timer = time(NULL);
    t = localtime(&timer);
    timeString[0][0] = (t->tm_hour / 10 > 0 ? t->tm_hour/10 + 48 : 0 + 48);
    timeString[0][1] = (t->tm_hour / 10 > 0 ? t->tm_hour%10 + 48 : t->tm_hour + 48);
    timeString[1][0] = (t->tm_min / 10 > 0 ? t->tm_min/10 + 48 : 0 + 48);
    timeString[1][1] = (t->tm_min / 10 > 0 ? t->tm_min%10 + 48 : t->tm_min + 48);
    timeString[2][0] = (t->tm_sec / 10 > 0 ? t->tm_sec/10 + 48 : 0 + 48);
    timeString[2][1] = (t->tm_sec / 10 > 0 ? t->tm_sec%10 + 48 : t->tm_sec + 48);
    sprintf(TIME,"%s %d %s %d %c%c:%c%c:%c%c EST",week[t->tm_wday],t->tm_mday,month[t->tm_mon],
        t->tm_year + 1900,timeString[0][0],timeString[0][1],timeString[1][0],timeString[1][1],timeString[2][0],timeString[2][1]);
    // wirte time data to TIME array.
    return TIME;
}

void makeLogfile(struct sockaddr_in *endServ_addr,char **requestAddress,int datasize){

    int fd; // file descriptor to write proxylog.
    char clientIP[INET_ADDRSTRLEN]; // client ip array.
    int clientIpAddr = endServ_addr->sin_addr.s_addr; // client ip address.
    inet_ntop(AF_INET,&clientIpAddr,clientIP,INET_ADDRSTRLEN); // convert binary address to decimal address.

    char *content = (char *)malloc(sizeof(char)*BUFFER_SIZE);

    sprintf(content,"%s: %s %s %i\n",getCurrentTime(), clientIP, *requestAddress, datasize);
    // copy data to content.
    printf("%s",content);
    // show the information at monitor.
    if ((fd = open("proxy.log", O_CREAT|O_RDWR|O_APPEND, 0644)) < 0) {
        error("ERROR on file");
    }// open proxy.log file.
    if(write(fd, content, strlen(content)) < 0){
        error("ERROR on write");
    }// write content to proxy.log file.
    free(content);
}

int main(int argc, char *argv[]){
    char *token; // token of requested message by client.
    char *requestHostname; // tokenize hostname from requested message.

    int endServerSockfd, clientSockfd, proxySockfd; //descriptors rturn from socket and accept system calls
    int portno; // port number
    int n,n2;
    socklen_t clilen;
    struct hostent *endServer; // it has infomation about endServer. 
    /*sockaddr_in: Structure Containing an Internet Address*/
    struct sockaddr_in endServ_addr, cli_addr, proxy_addr;
    
    if (argc < 2) {
        fprintf(stderr,"ERROR, no port provided\n");
        exit(1);
    }
    /*Create a new socket
      AF_INET: Address Domain is Internet 
      SOCK_STREAM: Socket Type is STREAM Socket */
    if ((proxySockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) error("ERROR opening socket");
    
    bzero((char *) &proxy_addr, sizeof(proxy_addr));
    portno = atoi(argv[1]); //atoi converts from String to Integer
    proxy_addr.sin_family = AF_INET;
    proxy_addr.sin_addr.s_addr = INADDR_ANY; //for the server the IP address is always the address that the server is running on
    proxy_addr.sin_port = htons(portno); //convert from host to network byte order

    if (bind(proxySockfd, (struct sockaddr *) &proxy_addr, sizeof(proxy_addr)) < 0) //Bind the socket to the server address
        error("ERROR on binding");
    
    if (listen(proxySockfd,5) < 0) // Listen for socket connections. Backlog queue (connections to wait) is 5
        error("ERROR on listening");

    clilen = sizeof(cli_addr);

    int webDataSize;
    char *buffer = (char *)malloc(sizeof(char)*BUFFER_SIZE);
    char *webData_buffer = (char *)malloc(sizeof(char)*WEB_DATA_BUFFER_SIZE);
    char *requestAddress = (char *)malloc(sizeof(char)*BUFFER_SIZE);
    char *proxyServer = (char *)malloc(sizeof(char)*BUFFER_SIZE);
    // using mallco method, make additional needed array to run program.

    while (1){

        webDataSize = 0;
        
        bzero(buffer,BUFFER_SIZE);
        bzero(webData_buffer,WEB_DATA_BUFFER_SIZE);
        bzero(requestAddress,BUFFER_SIZE);
        bzero(proxyServer,BUFFER_SIZE);
        // initialize array.

        printf("==============================================================\n");
        printf("====================  Waiting for Client  ====================\n");
        printf("==============================================================\n");
        /*accept function: 
          1) Block until a new connection is established
          2) the new socket descriptor will be used for subsequent communication with the newly connected client.
        */
        if ((clientSockfd = accept(proxySockfd, (struct sockaddr *) &cli_addr, &clilen)) < 0) error("ERROR on accept");

        if ((n = read(clientSockfd,buffer,BUFFER_SIZE)) < 0) error("ERROR reading from socket");
        //Read is a block function. It will read at most 1024 bytes
        else if (n == 0){
            close(clientSockfd);
            continue;
        }// if reading data size = 0 , go back to waiting client.

        token = (char *)strtok( buffer, " ");
        token = (char *)strtok( NULL, " ");

        strcpy(requestAddress,token);

        requestHostname = (char *)strtok( token, "//");
        requestHostname = (char *)strtok( NULL, "//");
        
        sprintf(proxyServer, "GET %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n", requestAddress, requestHostname);

        if ((endServer = gethostbyname(requestHostname)) == NULL ) error("creating endServer error");
        // create endServer using gethostbyname method.

        bzero((char *) &endServ_addr, sizeof(endServ_addr));
        endServ_addr.sin_family = AF_INET;
        memcpy((char *) &endServ_addr.sin_addr.s_addr, (char *)endServer->h_addr, endServer->h_length);
        endServ_addr.sin_port = htons(END_SEVER_PORT_NUM); //convert from host to network byte order
        // create endServer.

        if ((endServerSockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0 ) error("ERROR opening socket");
        // create endServer socket.
        if (connect(endServerSockfd, (struct sockaddr*)&endServ_addr, sizeof(endServ_addr)) < 0) error("ERROR on connecting");
        // connect to endServerSockfd.
        if (write(endServerSockfd, proxyServer, strlen(proxyServer)) < 0) error("ERROR on write");
        // send proxyServer data to endServer.

        while((n = read(endServerSockfd, webData_buffer, WEB_DATA_BUFFER_SIZE)) > 0){

            if ((n2 = write(clientSockfd, webData_buffer, n)) < 0) error("ERROR on write");
            // send webData to client.
            bzero(webData_buffer,WEB_DATA_BUFFER_SIZE);
            webDataSize += n;
        }
        if (n == -1) error("ERROR on write");
        // when error occurs during reading.
        makeLogfile(&endServ_addr,&requestAddress,webDataSize);
        // write log to log text file.

        close(endServerSockfd); // close endServerSockfd.
        close(clientSockfd); // close clientSockfd.

        printf("==============================================================\n");
        printf("================= End Requesting for Client ==================\n");
        printf("==============================================================\n\n");
    }
    free(proxyServer);
    free(requestAddress);
    free(webData_buffer);
    free(buffer);
    // free memory to use.
    close(proxySockfd); // close proxySockfd.
    return 0; 
}