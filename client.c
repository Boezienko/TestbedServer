#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include "packet.h"

#define PORT "3456"
#define SERVER_IP "144.96.151.55"
#define MAXDATASIZE 100 // max number of bytes we can get at once from server
#define MAXCOMMANDSIZE 100 // max number of bytes we can get at once for a command
#define MAXCMDARGS 5 // max number of bytes we can get at once for a command

// to make sending packets easier
void send_packet(int sockfd, Packet* pkt){
  char* spkt = pkt->serialize(pkt);
  
  printf("\nSending:\n%s\n", spkt);
  
  
  if(send(sockfd, spkt, strlen(spkt), 0) == -1){
    perror("send");
  }
  free(spkt);
  // so multiple sends streams aren't interpereted as one
  sleep(1);
}

// to send messages easier and encapsulate error handling
void send_file(int sockfd, char* filename){
  
  FILE* file = fopen(filename, "r");
  if(file == NULL){
    perror("fopen");
    return;
  }
  
  Packet packet;
  char pktBuf[MAXDATASIZE];
  size_t numBytes;
  int seqNum = 0;
  
  // prepare packet
  packet.initialize = initializePacket;
  
  while((numBytes = fread(pktBuf, 1, MAXDATASIZE-1, file)) > 0){
    pktBuf[numBytes] = '\0';
    packet.initialize(&packet, 2, seqNum, pktBuf);
    send_packet(sockfd, &packet);
    packet.free(&packet);
    seqNum++;
  }
  
  // send EOF packet
  packet.initialize(&packet, 3, seqNum, "EOF");
  send_packet(sockfd, &packet);
  packet.free(&packet);
  
  fclose(file);
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa){
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in*)sa)->sin_addr);
  }

  return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char *argv[]){
  int sockfd, numbytes;
  char buf[MAXDATASIZE];
  struct addrinfo hints, *servinfo, *p;
  int rv;
  char s[INET6_ADDRSTRLEN];
  bool connection = true;
  char command[MAXCOMMANDSIZE];
  char* cmdArgs[MAXCMDARGS];
  Packet tempPack;
  
  tempPack.initialize = initializePacket;
  tempPack.initialize(&tempPack, 0, 0, "");
  
  // ensure hints is clear and read to be filled
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  
  // get list of addrinfo structs to create socket descriptors
  if ((rv = getaddrinfo(SERVER_IP, PORT, &hints, &servinfo)) != 0){
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    return 1;
  }
  
  // loop through all the results and connect to the first one we can
  for (p = servinfo; p != NULL; p = p->ai_next){
  
    // create the socket
    if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1){
      perror("client: socket");
      // if error encountered, try creating socket with next addrinfo struct
      continue;
    }
    
    // connect to server
    if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      close(sockfd);
      perror("client: connect");
      // if error encountered, try again with next result
      continue;
    }
    
    // we have successfully created and connected to the socket, so leave loop
    break;
  }
  
  if (p == NULL) {
    fprintf(stderr, "client: failed to connect\n");
    return 2;
  }
  
  // check server IP we connected to
  inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);
  printf("client: connecting to %s\n", s);
  
  // start shell here so client can send things to server
  while(connection){
    printf("->");
    fgets(command, MAXCOMMANDSIZE, stdin);
    
    // replace newline character with terminating character
    command[strcspn(command, "\n")] = '\0';
    
    char commandcpy[MAXCOMMANDSIZE];
    strcpy(commandcpy, command);
    
    // tokenize command into arguments
    char* token = strtok(command, " ");
    int i = 0;
    while(token != NULL){
      cmdArgs[i] = token;
      token = strtok(NULL, " ");
      i++;
    }
    
    if(!strcmp(cmdArgs[0], "test")){
      // cmdArgs1: file to test
      // cmdArgs2: testcase file
      
      // send command to server
      printf("sending command: %s\n", commandcpy);
      tempPack.initialize(&tempPack, 0, 0, commandcpy);
      send_packet(sockfd, &tempPack);
      tempPack.free(&tempPack);
      
      // send name of file to test to server
      /*
      tempPack.initialize(&tempPack, 1, 0, cmdArgs[1]);
      send_packet(sockfd, &tempPack);
      tempPack.free(&tempPack);
      */
      
      // send file to test to server
      send_file(sockfd, cmdArgs[1]);
      /*
      // send testcase file
      tempPack.initialize(&tempPack, 2, 0, cmdArgs[2]);
      send_packet(sockfd, &tempPack);
      tempPack.free(&tempPack);
      */
      continue;
    } else if(!strcmp(cmdArgs[0], "ls-tests")){
      // just send this string and receive the testcase files
      tempPack.initialize(&tempPack, 1, 0, cmdArgs[0]);
      send_packet(sockfd, &tempPack);
      tempPack.free(&tempPack);
        
        /*
          need to make it receive here and show the client the testcase files
          we could maybe make a read, so you read the testcase and it's contents are
          sent back to the client
        */
    } else {
      printf("Invalid argument\n use the following\n test <fileToTest> <testcase>\n ls-tests\n");
      continue;
    }
  
    
    
  }
  
  
  
  // sending file to test's name
  tempPack.initialize(&tempPack, 1, 0, argv[1]);
  send_packet(sockfd, &tempPack);
  tempPack.free(&tempPack);
  
  // sending testcase file's name
  tempPack.initialize(&tempPack, 1, 1, argv[2]);
  send_packet(sockfd, &tempPack);
  tempPack.free(&tempPack);
  
  // send the actual file
  send_file(sockfd, argv[1]);
  
  // receive message from server as to weather the program passed or failed
  
  // print message recieved from server
  
  // freeing up this structure
  freeaddrinfo(servinfo);
  close(sockfd);
  
  return 0;
}
