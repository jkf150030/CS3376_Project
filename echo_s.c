// File: echo_s.c
// Contributors:
//    James Fritz       jkf150030
//    Chase Vriezema    cmv140030
//    Duc Nguyen	dqn150030
// Date: 04/23/2017
// Purpose: CS3376
// Description:
// 	This program initiates a TCP/UDP echo server. Requires server_functions.c.

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include "server_functions.h"


//global variables
int g_argc;
char **g_argv;
int32_t g_address;

void signalCatch(int sig_num)
{
	struct logMessage msg;
	char buffer[1024] =  "echo_s is stopping";
	
	//Record the time of the call.
	msg.dateTime = time(0);
	
	//Copy the stop message to the logMessage.
	bcopy((char *)buffer, (char *)msg.message, sizeof(buffer) - 1);
	
	//Store the client address.
	msg.address = g_address;
	
	//Call the log_s.
	callLogServer(msg, g_argv, g_argc);
	
	//Exit.
	_exit(0);
}

int main(int argc, char *argv[])
{
	int tcpfd[argc - 5];
	int udpfd[argc - 5];
	int newtcpfd, nready, maxfdp1;
	pid_t childpid;
	fd_set rset;
	ssize_t n;
	socklen_t clilen;
	struct sockaddr_in cli_addr, serv_addr;
	struct logMessage message;
	char buffer[1024];
	
	
	//Error if too few or too many port numbers provided.
	if(argc < 6 || argc > 8) {
		fprintf(stderr, "Usage: %s <port1> [<port2> <port3>] -logip <log_serv_address> -logport <log_port_address>\n", argv[0]);
		exit(0);
	}
	
	argv++;
	
	//Set signal for waitpid - handles zombie processes.
	signal(SIGCHLD, sigCatcher);

	for(int i = 0; i < argc - 5; i++) {
		/**********    This section creates the TCP socket.    **********/
		//Set serv_addr to all zeros.
		bzero((char *) &serv_addr, sizeof(serv_addr));
		
		//Create the TCP socket.
		tcpfd[i] = setupSocket(SOCK_STREAM, serv_addr, atoi(argv[i]));
		if(tcpfd[i] < 0) error(strncat((char *) "ERROR opening TCP socket on port ", argv[i], 35));
		
		//Bind the TCP socket to the address.
		if(bind(tcpfd[i], (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
			error("ERROR on binding TCP ");
		
		//Listen on the socket for connections.
		listen(tcpfd[i], 5);
		
		/**********    This section creates the UDP socket.    **********/
		//Set serv_addr to all zeros.
		bzero((char *) &serv_addr, sizeof(serv_addr));
		
		//Create the UDP socket.
		udpfd[i] = setupSocket(SOCK_DGRAM, serv_addr, atoi(argv[i]));
		if(udpfd[i] < 0) error(strncat((char *) "ERROR opening UDP socket on port ", argv[i], 35));
		
		//Bind the UDP socket to the address.
		if(bind(udpfd[i], (struct sockaddr *)&serv_addr, sizeof(serv_addr))<0)
			error("ERROR on binding UDP ");
		/****************************************************************/
	}
	
	//Clear the fd set.
	FD_ZERO(&rset);
	
	//Add one to the larger of the two file descriptors.
	maxfdp1 = -1;
	
	//sets the sigint handle to the function sigintHandler
	g_argc = argc;
	g_argv = argv;
	signal(SIGINT, signalCatch);

	//Loop infinitely to handle connections.
	while(1) {
		//Add both file descriptors to the set.
		for(int i = 0; i < argc -5; i++) {
			FD_SET(tcpfd[i], &rset);
			FD_SET(udpfd[i], &rset);
			
			if(tcpfd[i] > maxfdp1) maxfdp1 = tcpfd[i];
			if(udpfd[i] > maxfdp1) maxfdp1 = udpfd[i];
		}
		
		//Use select to determine which fd responds to the call.
		if((nready = select(maxfdp1 + 1, &rset, NULL, NULL, NULL)) < 0) {
			if(errno == EINTR) continue;
			else error("ERROR on select ");
		}
		
		int tfd, ufd;
		for (int i = 0; i < argc - 5; i++) {
			if(FD_ISSET(tcpfd[i], &rset)) {
				tfd = tcpfd[i];
				break;
			}
			if(FD_ISSET(udpfd[i], &rset)) {
				ufd = udpfd[i];
				break;
			}
		}
		
		//Record the time of the call.
		message.dateTime = time(0);
		
		//If tcpfd was added, process TCP communication.
		if(FD_ISSET(tfd, &rset)) {
			clilen = sizeof(cli_addr);
			
			//Accept the connection with a new file descriptor.
			newtcpfd = accept(tfd, (struct sockaddr *) &cli_addr, &clilen);
			if (newtcpfd < 0) error("ERROR on accept ");
			
			//Fork the new connection.
			if((childpid = fork()) < 0) error("ERROR on fork ");
			
			//Handle the transmission in the child.
			if(childpid == 0) {
				close(tfd);
				
				//Get the client address.
				message.address = cli_addr.sin_addr.s_addr;
				
				procTransT(newtcpfd, message, argv, argc);
				exit(0);
			}
			
			//Close the new fd in the parent.
			close(newtcpfd);
		}
		
		//If udpfd was added, process UDP communication.
		if(FD_ISSET(ufd, &rset)) {
			clilen = sizeof(cli_addr);
			
			//Reset the buffer.
			bzero(buffer, sizeof(buffer));
			
			//Receive the information from the client.
			n = recvfrom(ufd, buffer, sizeof(buffer), 0, (struct sockaddr *)&cli_addr, &clilen);
			if(n < 0) error("ERROR receiving from socket ");
			
			//Print the message.
			printf("UDP message: %s", buffer);
			
			//Create the logMessage struct.
			message.address = cli_addr.sin_addr.s_addr;
			bcopy((char *)buffer, (char *)message.message, sizeof(buffer) - 1);
			
			//Set g_address equal to message.address.
			g_address = message.address;
			
			//Call the log_s.
			callLogServer(message, argv, argc);
			
			//Write the response to the client.
			n = sendto(ufd, buffer, sizeof(buffer), 0, (struct sockaddr *)&cli_addr, clilen);
			if(n < 0) error("ERROR sending to socket ");
		}
	}
	
	return 0;	
}
