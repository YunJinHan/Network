/*
    2012036901 윤 진 한
    컴퓨터공학과
    Computer Network - Project 1
    2016.10.11
*/
/* 
   A simple server in the internet domain using TCP
   Usage:./server port (E.g. ./server 10000 )
*/
#include <stdio.h>
#include <sys/types.h>   // definitions of a number of data types used in socket.h and netinet/in.h
#include <sys/socket.h>  // definitions of structures needed for sockets, e.g. sockaddr
#include <netinet/in.h>  // constants and structures needed for internet domain addresses, e.g. sockaddr_in
#include <stdlib.h>
#include <strings.h>

// additional header file.
#include <fcntl.h>     // definitions of that deal attribute of a opened file in fcntl.h
#include <unistd.h>    // definitions of using file pointer in unistd.h
#include <sys/types.h> // definitions of a file function in sys/types.h

#define BSIZE 1024

off_t getFileSize(int fd){
    off_t filesize = lseek(fd,(off_t)0,SEEK_END);
    lseek(fd,(off_t)0,SEEK_SET); // set fileDescriptor to 0.
    return filesize;
} // return long type value that is size of file that the file descriptor indicate.

void error(char *msg){
    perror(msg);
    exit(1);
}

int main(int argc, char *argv[]){
    char *token,*token_type; // token of requested message by client.
    int fd; // file descriptor.
    int n;
    int sockfd, newsockfd; //descriptors rturn from socket and accept system calls
    int portno; // port number
    socklen_t clilen;
     
    char *buffer = (char *)malloc(sizeof(char)*BSIZE);
     
    /*sockaddr_in: Structure Containing an Internet Address*/
    struct sockaddr_in serv_addr, cli_addr;
    
    if (argc < 2) {
        fprintf(stderr,"ERROR, no port provided\n");
        exit(1);
    }
    /*Create a new socket
      AF_INET: Address Domain is Internet 
      SOCK_STREAM: Socket Type is STREAM Socket */
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) error("ERROR opening socket");
    
    bzero((char *) &serv_addr, sizeof(serv_addr));
    portno = atoi(argv[1]); //atoi converts from String to Integer
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY; //for the server the IP address is always the address that the server is running on
    serv_addr.sin_port = htons(portno); //convert from host to network byte order

    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) //Bind the socket to the server address
        error("ERROR on binding");
    
    if (listen(sockfd,15) < 0) // Listen for socket connections. Backlog queue (connections to wait) is 5
        error("ERROR on listening");

    clilen = sizeof(cli_addr);

    while (1){
        printf("--------------------- Waiting for Client ---------------------\n");
        /*accept function: 
          1) Block until a new connection is established
          2) the new socket descriptor will be used for subsequent communication with the newly connected client.
        */
        if ((newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen)) < 0) error("ERROR on accept");
        
        pid_t pid = fork(); // Execute fork.

        if (pid < 0){
            error("ERROR on fork"); // Error on fork.
        } else if (pid==0){ // Running in child process.

            bzero(buffer,BSIZE); // initialize buffer.

            if ((n = read(newsockfd,buffer,BSIZE)) < 0) error("ERROR reading from socket");
            //Read is a block function. It will read at most 255 bytes

            printf("Here is the message: %s\n",buffer);

            token = strtok( buffer, "/" );
            token = strtok( NULL, " " );
            // tokenize of requested message from client.

            fd = open(token,O_RDONLY); // find requested file.

            if (fd < 0){
                printf("There is no file that have requseted file.\n");
                n = write(newsockfd,"There is no file that have requseted file name.",47);
                if (n < 0) error("ERROR writing to socket");
            } // there is no file.
            else {
                token_type = strtok( token, ".");
                token_type = strtok( NULL, " ");
                // tokenize of requested file format.

                if (!strcmp(token_type,"html")){
                    char *header = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: Keep-Alive\r\n\r\n";
                    n = write(newsockfd,header,strlen(header));
                } else if (!strcmp(token_type,"mp3")){
                    char *header = "HTTP/1.1 200 OK\r\nContent-Type: audio/mpeg\r\nConnection: Keep-Alive\r\n\r\n";
                    n = write(newsockfd,header,strlen(header));
                } else if (!strcmp(token_type,"gif")){
                    char *header = "HTTP/1.1 200 OK\r\nContent-Type: image/gif\r\nConnection: Keep-Alive\r\n\r\n";
                    n = write(newsockfd,header,strlen(header));
                } else if (!strcmp(token_type,"jpeg") || !strcmp(token_type,"jpg")){
                    char *header = "HTTP/1.1 200 OK\r\nContent-Type: image/jpeg\r\nConnection: Keep-Alive\r\n\r\n";
                    n = write(newsockfd,header,strlen(header));
                } else if (!strcmp(token_type,"pdf")){
                    char *header = "HTTP/1.1 200 OK\r\nContent-Type: application/pdf\r\nConnection: Keep-Alive\r\n\r\n";
                    n = write(newsockfd,header,strlen(header));
                }
                // send requset file data format to client.
                if (n < 0){
                    error("ERROR writing to socket");
                }

                long filesize = getFileSize(fd); // get a size of file.
                printf("Requested File Size (Byte) : %ld\n",filesize);
                char *filedata = (char *)malloc(sizeof(char)*(filesize+1)); // create data array of file.
                bzero(filedata,filesize); // initialize filedata.

                n = read(fd,filedata,filesize); // copy data from origin file to data array.

                if (n < 0 ){
                    fprintf(stderr,"ERROR reading from file data\n");
                } // error ocurrs during copying data from origin file to data array.
                else {
                    n = write(newsockfd,filedata,filesize);
                    if (n < 0){
                        error("ERROR writing to socket");
                    } // send the data of requested file to client.
                    
                    printf("Complete handing client request : %s.%s\n",token,token_type);
                    // finish client request.
                }
                free(filedata); // free filedata memory.
            }
            exit(0); // child process exit.
        }   
        close(fd); // close File descriptor.
        close(newsockfd); // close newsockfd.
    }
    close(sockfd); // close server socket.
    return 0; 
}