/*
    2012036901 윤 진 한
    컴퓨터공학과
    Computer Network - Project 2 < Part 2 >
    2016.11.04
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

#define CACHE_COUNT 10
#define MAX_OBJECT_SIZE 524288

int nextCachingIndex = 0; // index of caching data.
char cacheData[CACHE_COUNT][MAX_OBJECT_SIZE]; // cache to store data.
char cacheLog[CACHE_COUNT][BUFFER_SIZE]; // cache to store requestAddress.
char cacheTime[CACHE_COUNT][BUFFER_SIZE]; // cache to store time.

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

void makeLogfile(struct sockaddr_in *endServ_addr,char **requestAddress,int datasize,int isInTheCache){
    int fd; // file descriptor to write proxylog.
    char *content = (char *)malloc(sizeof(char)*BUFFER_SIZE);
    char clientIP[INET_ADDRSTRLEN]; // client ip array.

    if (isInTheCache == -1){
        int clientIpAddr = endServ_addr->sin_addr.s_addr; // client ip address.
        inet_ntop(AF_INET,&clientIpAddr,clientIP,INET_ADDRSTRLEN); // convert binary address to decimal address.
    }
    
    sprintf(content,"%s: %s %s %i\n",getCurrentTime(), (isInTheCache == -1 ? clientIP : "127.0.0.1"), *requestAddress, datasize);
    // copy data to content.
    printf("\nLog : %s",content);
    // show the information at monitor.
    if ((fd = open("proxy.log", O_CREAT|O_RDWR|O_APPEND, 0644)) < 0) {
        error("ERROR on file");
    }// open proxy.log file.
    if(write(fd, content, strlen(content)) < 0){
        error("ERROR on write");
    }// write content to proxy.log file.
    free(content);
}

int isInTheCache(char **requestAddress){
    
    for (int i=0;i< ( nextCachingIndex == CACHE_COUNT - 1 ? CACHE_COUNT : nextCachingIndex );i++){
        if (!strcasecmp(cacheLog[i],*requestAddress)) return i;
    }
    return -1;
}// whether requestAddress is in cacheLog array.

int main(int argc, char *argv[]){
    char *token; // token of requested message by client.
    char *requestHostname; // tokenize hostname from requested message.

    int endServerSockfd, clientSockfd, proxySockfd; //descriptors rturn from socket and accept system calls
    int portno; // port number
    int n,n2,cachingIndex;
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
    char *tmpCachingData = (char *)malloc(sizeof(char)*MAX_OBJECT_SIZE);
    // using mallco method, make additional needed array to run program.

    bzero(cacheData, CACHE_COUNT*MAX_OBJECT_SIZE); 
    bzero(cacheLog, CACHE_COUNT*BUFFER_SIZE);
    // initialize caching array.

    while (1){

        webDataSize = 0;
        
        bzero(buffer,BUFFER_SIZE);
        bzero(webData_buffer,WEB_DATA_BUFFER_SIZE);
        bzero(requestAddress,BUFFER_SIZE);
        bzero(proxyServer,BUFFER_SIZE);
        bzero(tmpCachingData,MAX_OBJECT_SIZE);
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

        cachingIndex = isInTheCache(&requestAddress);

        if (cachingIndex != -1){

            int index = (nextCachingIndex == CACHE_COUNT-1 ? CACHE_COUNT-1 : nextCachingIndex - 1);

            printf("--------------- < Requested Data is in Cache > ---------------\n");

            printf("\n===== < Before Change Hit Data position in Cache > =====\n\n");
            printf("Least Recently-used Cache Number : %d\n",index);
            printf("%-9s%-70s%-20s\n","Index","Caching URL"," Caching Time");
            for (int i=0;i<CACHE_COUNT;i++){
                printf("LRU[%d] : %-70.70s %-20s",i,cacheLog[i],cacheTime[i]+11);
                if (i == cachingIndex) printf(" <<---[HIT DATA]");
                putchar('\n');
            }
            // show cacheLog and HIT DATA.

            if (cachingIndex != CACHE_COUNT - 1 && cachingIndex != nextCachingIndex - 1){

                strcpy(tmpCachingData,cacheData[cachingIndex]);
                bzero(cacheData[cachingIndex],MAX_OBJECT_SIZE);
                bzero(cacheTime[cachingIndex],BUFFER_SIZE);

                for (int i=cachingIndex; i <= (nextCachingIndex == (CACHE_COUNT-1) ? CACHE_COUNT - 2 : nextCachingIndex - 2); i++){
                    strcpy(cacheData[i],cacheData[i + 1]);
                    strcpy(cacheLog[i],cacheLog[i + 1]);
                    strcpy(cacheTime[i],cacheTime[i + 1]);
                }
                strcpy(cacheData[(nextCachingIndex == (CACHE_COUNT-1) ? CACHE_COUNT - 1 : nextCachingIndex - 1)],tmpCachingData);
                strcpy(cacheLog[(nextCachingIndex == (CACHE_COUNT-1) ? CACHE_COUNT - 1 : nextCachingIndex - 1)],requestAddress);
                strcpy(cacheTime[(nextCachingIndex == (CACHE_COUNT-1) ? CACHE_COUNT - 1 : nextCachingIndex - 1)],getCurrentTime());
                // move HIT DATA to last array index and rearrange array.
            }// HIT DATA index is not last index.
            else {
                strcpy(cacheTime[cachingIndex],getCurrentTime());
            }
            
            webDataSize = strlen(tmpCachingData);

            if ((n = write(clientSockfd, tmpCachingData, webDataSize)) < 0) error("ERROR on write");
            // send cached webData to client.

            printf("\n===== < After  Change Hit Data position in Cache > =====\n\n");
            printf("Least Recently-used Cache Number  : %d\n",index);
            printf("%-9s%-70s%-20s\n","Index","Caching URL"," Caching Time");
            for (int i=0;i<CACHE_COUNT;i++){
                printf("LRU[%d] : %-70.70s %-20s",i,cacheLog[i],cacheTime[i]+11);
                if (i == index) printf(" <<---[HIT DATA]");
                putchar('\n');
            }
            // show change Hit Data position and cacheLog.
        } // if requestAddress is in cache.
        else {

            printf("--------------- < Requested Data is NOT in Cache > ---------------\n");

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
                webDataSize += n;

                if (webDataSize <= MAX_OBJECT_SIZE){
                    strcat(tmpCachingData,webData_buffer); // copy webData_buffer to tmpCachingData;
                } 
                bzero(webData_buffer,WEB_DATA_BUFFER_SIZE);
            }
            if (n == -1) error("ERROR on write");
            // when error occurs during reading.
            if (webDataSize <= MAX_OBJECT_SIZE){

                if (nextCachingIndex == CACHE_COUNT - 1){
                    for (int i=0;i<CACHE_COUNT-1;i++){
                        strcpy(cacheData[i],cacheData[i + 1]);
                        strcpy(cacheLog[i],cacheLog[i + 1]);
                        strcpy(cacheTime[i],cacheTime[i + 1]);
                    }
                }   
                bzero(cacheData[(nextCachingIndex == CACHE_COUNT-1 ? CACHE_COUNT-1 : nextCachingIndex)],MAX_OBJECT_SIZE);
                memcpy(cacheData[(nextCachingIndex == CACHE_COUNT-1 ? CACHE_COUNT-1 : nextCachingIndex)],tmpCachingData,strlen(tmpCachingData) + 1);
                
                bzero(cacheLog[(nextCachingIndex == CACHE_COUNT-1 ? CACHE_COUNT-1 : nextCachingIndex)],BUFFER_SIZE);
                memcpy(cacheLog[(nextCachingIndex == CACHE_COUNT-1 ? CACHE_COUNT-1 : nextCachingIndex)],requestAddress,strlen(requestAddress) + 1);

                bzero(cacheTime[(nextCachingIndex == CACHE_COUNT-1 ? CACHE_COUNT-1 : nextCachingIndex)],BUFFER_SIZE);
                strcpy(cacheTime[(nextCachingIndex == CACHE_COUNT-1 ? CACHE_COUNT-1 : nextCachingIndex)],getCurrentTime());

                printf("< Store Data in Cache : cacheLog[%d] : %s > \n",(nextCachingIndex == CACHE_COUNT-1 ? CACHE_COUNT-1 : nextCachingIndex)
                    ,cacheLog[(nextCachingIndex == CACHE_COUNT-1 ? CACHE_COUNT-1 : nextCachingIndex)]);

            } // if wedDatasize is less than MAX_OBJECT_SIZE, insert wedData to cache.

            if (nextCachingIndex < CACHE_COUNT-1) nextCachingIndex++;

        } // if requestAddress is not in cache.

        makeLogfile(&endServ_addr,&requestAddress,webDataSize,cachingIndex);
        // write log to log text file.

        close(endServerSockfd); // close endServerSockfd.
        close(clientSockfd); // close clientSockfd.

        printf("==============================================================\n");
        printf("================= End Requesting for Client ==================\n");
        printf("==============================================================\n\n");
    }
    free(tmpCachingData);
    free(proxyServer);
    free(requestAddress);
    free(webData_buffer);
    free(buffer);
    // free memory to use.
    close(proxySockfd); // close proxySockfd.
    return 0; 
}