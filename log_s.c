// File: log_s.c
// Contributors:
//    Duc Nguyen        dqn150030
//    Luke Pettinger    flp081000
// Date: 04/23/2017
// Purpose: CS3376
// Description:
// 	This program initiates a UDP log server. Requires server_functions.c.

#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <netdb.h>
#include <stdio.h>
#include "server_functions.h"


int main(int argc, char *argv[])
{
	int udpfd, n;
	socklen_t clilen;
	struct sockaddr_in serv_addr, cli_addr;
	struct logMessage logMsg;
	char echoStop[1024] = "echo_s is stopping"; 
	
	//Error if no port number provided or if too many args.
	if(argc < 3 || argc > 3) {
		fprintf(stderr, "Usage: %s -port port \n", argv[0]);
		exit(0);
	}
	
	//If four args, see if argv[1] is -port
	if(argc == 4) {
		if(strcmp(argv[1], "-port") != 0) {
			fprintf(stderr, "Usage: %s -port port\n", argv[0]);
			exit(0);
		}
	}
	
	//Set serv_addr to all zeros.
	bzero(&serv_addr, sizeof(serv_addr));
	
	//Create the socket.
	udpfd = setupSocket(SOCK_DGRAM, serv_addr, atoi(argv[argc-1]) );
	if(udpfd < 0) error("ERROR opening socket ");
	
	//Bind the socket to the address.
	if(bind(udpfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr))<0)
		error("ERROR on binding ");
	
	//Get the size of the client address structure.
	clilen = sizeof(cli_addr);
	
	//Loop infinitely to receive connections.
	while(1) {
		//Set message to all zeros.
		bzero(&logMsg, sizeof(struct logMessage));
		
		//Receive the information from the client
		n = recvfrom(udpfd, &logMsg, sizeof(struct logMessage), 0, (struct sockaddr *)&cli_addr, &clilen);
		if(n < 0) error("ERROR receiving from socket ");

		//Convert the given time to a tm struct.
		struct tm * now = localtime(&logMsg.dateTime);
		
		//Find and clear the line return.
		for(int i = 0; (unsigned) i < (size_t) sizeof(logMsg.message); i++) {
			if(logMsg.message[i] == 10) logMsg.message[i] = 0;
		}
		
		//if the message from echo_s is "echo is stopping" then terminate
		if(strcmp(echoStop,logMsg.message) == 0)
		{
			n = sendto(udpfd, "Log_s is stopping.", 18, 0, (struct sockaddr *)&cli_addr, clilen);
			if(n < 0) error("ERROR sending to socket ");
			exit(0);
		}
		else
		{
			//print the message
			 printf("Logging: %.4d-%.2d-%.2d %.2d:%.2d:%.2d \"%s\" was received from %d.%d.%d.%d",
			 (now->tm_year + 1900), 
			 (now->tm_mon + 1),
			 now->tm_mday,
			 now->tm_hour,
			 now->tm_min,
			 now->tm_sec,
			 logMsg.message,
			 int(logMsg.address&0xFF),
			 int((logMsg.address&0xFF00)>>8),
			 int((logMsg.address&0xFF0000)>>16),
			 int((logMsg.address&0xFF000000)>>24));

			//Write to the log file.
			appendLog(logMsg);
		
			//Write the response to the client.
			n = sendto(udpfd, "Log file updated.", 17, 0, (struct sockaddr *)&cli_addr, clilen);
			if(n < 0) error("ERROR sending to socket ");
		}
		/*
		appendLog(logMsg);
		n = sendto(udpfd, "Log file updated.", 17, 0, (struct sockaddr *)&cli_addr, clilen);
		if(n < 0) error("ERROR sending to socket ");*/



	}
	
	return 0;
}
