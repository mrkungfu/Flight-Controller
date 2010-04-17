#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/time.h>
#include "FC_Common.h"
#include <NIDAQmx.h>

#define MAXPENDING 5    /* Max connection requests */
#define JRANGE 22000.0
#define MIN(a,b) ((a)>(b)?(b):(a))
#define MAX(a,b) ((a)>(b)?(a):(b))
#define JOYPERCENT(a) ((float)a/(JRANGE/100.0))
#define DAQmxErrChk(functionCall) if( DAQmxFailed(error=(functionCall)) ) goto Error; else

int         error=0;
TaskHandle  taskHandle=0;
char        errBuff[2048]={'\0'};
float64     data[3] = {0.004,0.020,0.012};
int 	    airOff = 0;

void Die(char *mess) { perror(mess); exit(1); }
void HandleClient(int sock);
void print_jPacket (struct joystickPacket *packet);
void process_data (struct joystickPacket *packet);
void positionElevons (int x, int y);
void drivePosition (float left, float right, float rudder);

int main(int argc, char *argv[]) {
	int serversock, clientsock;
	struct sockaddr_in echoserver, echoclient;
	
	struct timeval clockt1, clockt2, clockd;

	// DAQmx Configure Code
	//   Setup task and create three virtual channels
	//   for left and right elevons and the rudder
	//   Range: 0-2mA
	DAQmxErrChk (DAQmxCreateTask("",&taskHandle));
	DAQmxErrChk (DAQmxCreateAOCurrentChan(taskHandle,
					"Dev2/ao16,Dev2/ao18,Dev2/ao20",
					"left,right,rudder",
					0.0,
					0.02,
					DAQmx_Val_Amps,
					NULL));

	// DAQmx Start Code (probably not needed)
	DAQmxErrChk (DAQmxStartTask(taskHandle));
	
	if (argc != 2) {
	  fprintf(stderr, "USAGE: echoserver <port>\n");
	  exit(1);
	}
	/* Create the TCP socket */
	if ((serversock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
	  Die("Failed to create socket");
	}
	/* Construct the server sockaddr_in structure */
	memset(&echoserver, 0, sizeof(echoserver));       // Clear struct
	echoserver.sin_family = AF_INET;                  // Internet/IP
	echoserver.sin_addr.s_addr = htonl(INADDR_ANY);   // Incoming addr
	echoserver.sin_port = htons(atoi(argv[1]));       // server port
	
	/* Bind the server socket */
	if (bind(serversock, (struct sockaddr *) &echoserver,
	                             sizeof(echoserver)) < 0) {
	  Die("Failed to bind the server socket");
	}
	/* Listen on the server socket */
	if (listen(serversock, MAXPENDING) < 0) {
	  Die("Failed to listen on server socket");
	}
	
	/* Run until cancelled */
	while (1) {
	  unsigned int clientlen = sizeof(echoclient);
	  /* Wait for client connection */
	  if ((clientsock =
	       accept(serversock, (struct sockaddr *) &echoclient,
	              &clientlen)) < 0) {
	    Die("Failed to accept client connection");
	  }
	  gettimeofday(&clockt1, NULL);
	  fprintf(stdout, "Client connected: %s\n", inet_ntoa(echoclient.sin_addr));
	  HandleClient(clientsock);
	  fprintf(stdout, "Client disconnected. ");
	  gettimeofday(&clockt2, NULL);
	  //clockd = (clockt2 - clockt1);
	  clockd.tv_sec = clockt2.tv_sec - clockt1.tv_sec;
	  clockd.tv_usec = clockt2.tv_usec - clockt1.tv_usec;
	  fprintf(stdout, "(%us %luus)\n\n", clockd.tv_sec, clockd.tv_usec);
	}

Error:
	if( DAQmxFailed(error) )
	{
		DAQmxGetExtendedErrorInfo(errBuff,2048);
		printf("DAQmx Error %s\n",errBuff);
	}
}

void HandleClient(int sock) {
	char buffer[BUFFSIZE], temp[BUFFSIZE];
	int received = -1;
	struct timeval clockt1, clockt2, clockd;
	int count = 0;
	struct joystickPacket packet;
	
	/* Receive message */
	if ((received = recv(sock, buffer, BUFFSIZE, 0)) < 0) {
	  Die("Failed to receive initial bytes from client");
	}
	gettimeofday(&clockt1, NULL);
	
	/* check for more incoming data in loop.. eventually (todo) */
	while (received > 0) {
	
	  //if(count > 0)
	  //{}
	  buffer[received] = 0;
          
	  unsigned int length, x;
	  
	  sscanf(buffer, "%uStart\n%u\n%u\n%u\n%u\n%u\n%d\n%d\n%d\n%d\nEnd",
				    &length,
				    &packet.version, 
				    &packet.length,
				    &packet.seq,
                                    &packet.ts_sec,
                                    &packet.ts_usec,
                                    &packet.d0,
                                    &packet.d1,
                                    &packet.d2,
	                            &packet.d3);
	
	  printf(" Length: %u\n", length);
	  printf(" Data Captured: %d\n", received);
	  
	  print_jPacket(&packet);

	  //determine if joystick button was pressed to turn off air
	  airOff = packet.d2;
	  
  	  process_data(&packet);
	
	  if(received < length){
		int pos = received;
	  	/* Check for more data */
	  	if ((received = recv(sock, &buffer[pos], (BUFFSIZE-pos), 0)) < 0) {
	    		Die("Failed to receive additional bytes from client");
	  	}
	  } else {
		received = 0;
	  }
	  count++;
	}
	gettimeofday(&clockt2, NULL);
	//clockd = (clockt2 - clockt1);
	clockd.tv_sec = clockt2.tv_sec - clockt1.tv_sec;
	clockd.tv_usec = clockt2.tv_usec - clockt1.tv_usec;
	
	fprintf(stdout, " Time: %us %luus\n", clockd.tv_sec, clockd.tv_usec);
	close(sock);
}

void print_jPacket (struct joystickPacket *packet) {
	fprintf(stdout, "  Version: %u\n  Length: %u\n  Seq: %u\n  TS: %u(s) %u(us)\n  Data0: %d\n  Data1: %d\n  Data2: %d\n  Data3: %d\n",
				    packet->version, 
				    packet->length,
				    packet->seq,
                                    packet->ts_sec,
                                    packet->ts_usec,
                                    packet->d0,
                                    packet->d1,
                                    packet->d2,
	                            packet->d3);
}


void process_data (struct joystickPacket *packet) {
	positionElevons (packet->d0, packet->d1);
}

//For elevon controls we use up/down (y) input
//to affect elevator movements.  For left/right
//(x) control, we treat the elevon as an
//aileron.  This function converts the joystick
//positions with a span of +/-32000 to percentages
//of elevon position (+/-100 of center/flat).
void positionElevons (int x, int y) {
	float le, re, xper, yper;

	xper = -JOYPERCENT(x);
	yper = -JOYPERCENT(y);

	le = yper;
	re = yper;

	le += xper;
	re -= xper;

	le = MIN(le, 100.0);
	le = MAX(le, -100.0);

	re = MIN(re, 100.0);
	re = MAX(re, -100.0);

	drivePosition(le, re, MAX(MIN(xper, 100.0), -100.0));//xper here is used in place of the rudder
}


void drivePosition (float left, float right, float rudder) {
	float leftmA, rightmA, ruddermA;
	//setup NI data structures in main perhaps...

	printf("  Left: %f%%, Right: %f%%, Rudder:%f%%\n", left, right, rudder);

	//Convert passed percentages to current -
	//We're working with 4-20mA here (0-100%).
	//As such, center position is 12mA. Add or
	//subtract from there based on a percentage
	//of the +/- 8mA span.
	leftmA = (left/100.0) * 2.8 + 10.5;
	rightmA = (-right/100.0) * 2 + 12;
	ruddermA = (-rudder/100.0) * 4 + 9;

	data[0] = leftmA/1000;
	data[1] = rightmA/1000;
	data[2] = ruddermA/1000;

	if(airOff)
	{
		data[0] = data[1] = data[2] = 0;
	}

	// DAQmx Write Code
	//   Samples per channel: 1
	//   AutoStart: true
	//   Timeout: 0
	DAQmxErrChk (DAQmxWriteAnalogF64(taskHandle,
				1,
				1,
				0,
				DAQmx_Val_GroupByChannel,
				data,
				NULL,
				NULL));

	// hope for the best!

Error:
	if( DAQmxFailed(error) )
	{
		DAQmxGetExtendedErrorInfo(errBuff,2048);
		printf("DAQmx Error %s\n",errBuff);
	}
}
