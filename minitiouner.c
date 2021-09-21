/*
 * Remote Control for minitiouner
 *
 * send via UDP to the PC where minitiouner is running, port: 6789
 * 
 * using a bash command:
 * echo "[GlobalMsg],Freq=10492527,Offset=9360000,Doppler=0,Srate=2000,WideScan=0,LowSR=0,DVBmode=Auto,FPlug=A,Voltage=0,22kHz=Off" > /dev/udp/IPADR/PORT
 * 
 * minimal sentence accepted my minitiouner software:
 * "[GlobalMsg],Freq=10492527,Offset=9360000,Doppler=0,Srate=2000,DVBmode=Auto"
 * 
 * same function for ryde:
 * -----------------------
 * 1) open TCP connection to ryde's IP and port 8765
 * 2) send the following:
 * {
 *      "request":"setTune",
 *      "tune":
 *      {
 *          "band":{"lofreq":9750000,"loside":"LOW","pol":"HORIZONTAL","port":"TOP","gpioid":7},
 *          "freq":10491530,
 *          "sr":1500
 *      }
 * }
 * 
 {"request":"setTune","tune":{"band":{"lofreq":9750000,"loside":"LOW","pol":"HORIZONTAL","port":"TOP","gpioid":7},"freq":10491805,"sr":1500}}
 * 
 * */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <sys/file.h>
#include <pthread.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <pwd.h>
#include <errno.h>
#include "../qo100websdr.h"
#include "../setup.h"

int mt_port;
int mt_sock;

void init_udp_minitiouner()
{
	srand(time(NULL));
    mt_sock = socket(PF_INET, SOCK_DGRAM, 0);
    if (mt_sock == -1)
    {
        printf("Failed to create UDP socket,errno=%d\n", errno);
        return;
    }

    struct sockaddr_in mysockadr;
    memset(&mysockadr, 0, sizeof(struct sockaddr_in));
    mysockadr.sin_family = AF_INET;
    mt_port = (rand() % 1000) + 40000;
    mysockadr.sin_port = htons(mt_port);
    printf("open sock: %d\n", mt_port);
    mysockadr.sin_addr.s_addr = INADDR_ANY;

    if (bind(mt_sock, (struct sockaddr *)&mysockadr, sizeof(struct sockaddr_in)) != 0)
    {
        printf("Failed to bind PIPE socket, errno=%d\n", errno);
        close(mt_sock);
        mt_sock = -1;
        return;
    }
}

void remove_udp_minitiouner()
{
    if (mt_sock != -1)
        close(mt_sock);
}

char write_udppipe(unsigned char *data, int len)
{
	if (mt_sock != -1)
	{
		struct sockaddr_in sin;
		sin.sin_family = AF_INET;
		sin.sin_port = htons(minitiouner_port);
		sin.sin_addr.s_addr = inet_addr(mtip);
		sendto(mt_sock, (char *)data, len, 0, (struct sockaddr *)&sin, sizeof(struct sockaddr_in));
	}
	return 1;
}

void write_tcp(char *s)
{
    // to mtip,minitiouner_port
    struct addrinfo hints;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = 0;
	hints.ai_protocol = 0;
	struct addrinfo* results;

	if (getaddrinfo(mtip, "8765", &hints, &results) != 0) {
		printf("error getaddrinfo()'ing, translation error\n");
		return;
	}

	struct addrinfo* addr_okay;
	int succeeded = 0;
	int sck_fd = socket(AF_INET, SOCK_STREAM, 0);
	for (addr_okay = results; addr_okay != NULL; addr_okay = addr_okay->ai_next) {
		if (connect(sck_fd, addr_okay->ai_addr, addr_okay->ai_addrlen) >= 0) {
			succeeded = 1;
			//printf("connected successfully!\n");
			break;
		}
	}
	freeaddrinfo(results);

	if (!succeeded) {
		printf("error, coudln't connect() to any of possible addresses\n");
		return;
	}

	int ret = write(sck_fd, s, strlen(s));
	if (ret) {}

    read(sck_fd,s,999);

	close(sck_fd);
}

int mtmode = 1; // 0=Minitiouner(Windows) 1=ryde (Raspi4)

void setMinitiouner(char *msg)
{
char s[1000];
char qrg[20];
char bw[20];

    printf("set Minitiouner:<%s>\n",msg);

    memcpy(qrg,msg,8);
    qrg[8] = 0;

    char *hp = strchr(msg,',');
    if(hp)
    {

        hp++;
        if(strlen(hp) > 1 && strlen(hp) < 6)
        {
            strcpy(bw,hp);
            if(mtmode == 0)
            {
                sprintf(s,"echo \"[GlobalMsg],Freq=%8.8s,Offset=%d,Doppler=0,Srate=%s,DVBmode=Auto\n\" > /dev/udp/%s/%d",qrg,minitiouner_offset,bw,mtip,minitiouner_port);
                write_udppipe((unsigned char *)s, strlen(s)+1); // +1 because we need to send including the null terminator
            }
            else
            {
                sprintf(s,"{\"request\":\"setTune\",\"tune\":{\"band\":{\"lofreq\":%d,\"loside\":\"LOW\",\"pol\":\"HORIZONTAL\",\"port\":\"TOP\",\"gpioid\":7},\"freq\":%s,\"sr\":%s}}",minitiouner_offset,qrg,bw);
                write_tcp(s);
            }
        }
    }
}
