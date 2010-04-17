#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <linux/joystick.h>
#include "FC_Common.h"

#define JOY_DEV "/dev/js0"

unsigned int FC_Sequence = 0;

void Die(char *mess) { perror(mess); exit(1); }
void initPacket (struct joystickPacket *packet);
int buildPacket (struct joystickPacket *packet, struct position *joystick_pos);
int sendPacket (int socket, struct joystickPacket *packet);

int main (int argc, char *argv[]) {
	/* network stuff */
	int sock;
	struct sockaddr_in echoserver;
	unsigned int echolen;
	int received = 0;
	
	/* joystick stuff */
	int joy_fd, *axis=NULL, num_of_axis=0, num_of_buttons=0;
	int num_packets=0;
	char *button=NULL, name_of_joystick[80], time_buf[30], c;
	struct js_event js;
	
	/* both */
	struct joystickPacket currentData;
	struct position joystick_pos;
	
	if (argc != 3) {
	  fprintf(stderr, "USAGE: %s <server_ip> <port>\n", argv[0]);
	  exit(1);
	}
	
	/* Create the TCP socket */
	if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
	  Die("Failed to create socket");
	}
	
	/* Construct the server sockaddr_in structure */
	memset(&echoserver, 0, sizeof(echoserver));       /* Clear struct */
	echoserver.sin_family = AF_INET;                  /* Internet/IP */
	echoserver.sin_addr.s_addr = inet_addr(argv[1]);  /* IP address */
	echoserver.sin_port = htons(atoi(argv[2]));       /* server port */
	
	/* joystick init */
	if( ( joy_fd = open( JOY_DEV , O_RDONLY)) == -1 )
	{
		printf( "Couldn't open joystick\n" );
		return -1;
	}

	ioctl( joy_fd, JSIOCGAXES, &num_of_axis );
	ioctl( joy_fd, JSIOCGBUTTONS, &num_of_buttons );
	ioctl( joy_fd, JSIOCGNAME(80), &name_of_joystick );

	axis = (int *) calloc( num_of_axis, sizeof( int ) );
	button = (char *) calloc( num_of_buttons, sizeof( char ) );
	
	printf("Joystick detected: %s\n\t%d axis\n\t%d buttons\n\n"
		, name_of_joystick
		, num_of_axis
		, num_of_buttons );

	/* 
	If the line below is commented out then we have opened up in blocked mode 
	the read will wait for response 
	*/
  	//fcntl( joy_fd, F_SETFL, O_NONBLOCK );	/* use non-blocking mode */
	printf("Reading joystick in blocking mode\n");
	/* end joystick init */
	
	
	/* send updates as we get them from the controller */
	while (1){
		/* read the joystick state (blocking) */
		read(joy_fd, &js, sizeof(struct js_event));
		
		/* see what to do with the event */
		switch (js.type & ~JS_EVENT_INIT)
		{
			case JS_EVENT_AXIS:
				axis   [ js.number ] = js.value;
				break;
			case JS_EVENT_BUTTON:
				button [ js.number ] = js.value;
				break;
		}

		printf( "X: %6d  Y: %6d  ", axis[0], axis[1] );
		
		joystick_pos.x = axis[0];
		joystick_pos.y = axis[1];
		
		if( num_of_axis > 2 ) {
			printf("Z: %6d  ", axis[2] );
			joystick_pos.z = axis[2];
		}

		if( js.number == 3 )
			joystick_pos.z = 1;
		else
			joystick_pos.z = 0;

		num_packets++;				
		
		printf("   number packets: %d\r",num_packets);
		fflush(stdout);
			
		/* Establish connection */
		if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		  Die("Failed to create socket");
		}
		if (connect(sock, (struct sockaddr *) &echoserver,
		            sizeof(echoserver)) < 0) {
		  Die("1Failed to connect with server");
		}
		buildPacket(&currentData, &joystick_pos);
		sendPacket(sock, &currentData);
		close(sock);
	}
	
	
	fprintf(stdout, "\n");
	exit(0);
}

int sendPacket (int socket, struct joystickPacket *packet) {
	char buff[BUFFSIZE];
	buff[4] = '\0';
	
	// start at the 5th byte, the first four will hold the total size
	// of this buffer (from Start->End + 4 bytes of the size itself)
	// Why four bytes for a number that shouldn't be much more than 100?
	// Cause I'm waaaay overengineering this.
	snprintf(&buff[4], BUFFSIZE, "Start\n%u\n%u\n%u\n%u\n%u\n%d\n%d\n%d\n%d\nEnd",
				    (unsigned DWORD)packet->version, 
				    (unsigned DWORD)packet->length,
				    (unsigned DWORD)packet->seq,
                                    (unsigned DWORD)packet->ts_sec,
                                    (unsigned DWORD)packet->ts_usec,
                                    (DWORD)packet->d0,
                                    (DWORD)packet->d1,
                                    (DWORD)packet->d2,
	                            (DWORD)packet->d3
	);
	
	unsigned int count = strlen(&buff[4]) + 4;
	printf("\nCount: %u\nBuff: %s\n\n", count, &buff[4]);
	char length[5];
	snprintf(length, 5, "%04u", count); //zero padded
	buff[0] = length[0];
	buff[1] = length[1];
	buff[2] = length[2];
	buff[3] = length[3];
	
	//handle checksum?  Not while using TCP (for now)...
	
	if (send(socket, buff, count, 0) != count) {
	  Die("Mismatch in number of sent bytes");
	}
}

int buildPacket (struct joystickPacket *packet, struct position *joystick_pos) {
	struct timeval timestamp;
	
	initPacket(packet);
	gettimeofday(&timestamp, NULL);
	packet->ts_sec = timestamp.tv_sec;
	packet->ts_usec = timestamp.tv_usec;
	packet->d0 = joystick_pos->x;
	packet->d1 = joystick_pos->y;
	packet->d2 = joystick_pos->z;
	packet->d3 = (DWORD)NULL;
}

void initPacket (struct joystickPacket *packet) {
	packet->version = 0;
	packet->length = 3;
	packet->seq = FC_Sequence;
	FC_Sequence++;
}
