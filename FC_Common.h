//please redefine these properly per platform
//use sizeof or stdint.h, but do it.
//Note: I'm using non C99 standards because I
//      haven't reserched the effect of applying
//      signed to uintX_t or vis-versa
#define BYTE char
#define WORD short
#define DWORD int
#define QWORD long long

#define BUFFSIZE 132

struct joystickPacket {
	unsigned DWORD  version;
	unsigned DWORD  length;
	unsigned DWORD seq;
	unsigned DWORD  ts_sec;
	unsigned DWORD ts_usec;
	signed DWORD   d0;
	signed DWORD   d1;
	signed DWORD   d2;
	signed DWORD   d3;
	//unsigned WORD chksum;
};

struct position {
	signed DWORD x;
	signed DWORD y;
	signed DWORD z;
};
