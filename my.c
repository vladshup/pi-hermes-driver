/*
      Hermes Lite 
	  
	  Radioberry implementation
	  
	  2016 Johan PA3GSB
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <math.h>
#include <semaphore.h>
#include <pigpio.h>

#define SERVICE_PORT	1024

//Declare
void *spiReader(void *arg);
void *packetreader(void *arg);
void *spiWriter(void *arg);
int spi_init (unsigned long speed);
int udp_init (void);
unsigned char readPackets(void);
void handlePacket(char* buffer);
void fillDiscoveryReplyMessage();


//Some?
#define TX_MAX 3200 
unsigned char tx_buffer[TX_MAX];
int fill_tx = 0; 
int use_tx  = 0;
unsigned char drive_level;
int MOX = 0;

unsigned char audiooutputbuffer[4096];
int audiocounter = 0;
sem_t tx_empty;
sem_t tx_full;
sem_t mutex;

//SPI
static int rx1_spi_handler;
static int rx2_spi_handler;

//UDP
#define TIMEOUT_MS      100
int running = 0;
int fd;									/* our socket */
struct sockaddr_in myaddr;				/* our address */
struct sockaddr_in remaddr;				/* remote address */
socklen_t addrlen = sizeof(remaddr);	/* length of addresses */
int recvlen;							/* # bytes received */


//Hendler UDP
int hold_nrx=0;
int nrx = 2; // n Receivers

int holdfreq = 0;
int holdfreq2 = 0;
int holdtxfreq = 0;
int freq = 4706000;
int freq2 = 1008000;
int txfreq = 3630000;

int att = 0;
int holdatt =128;
int holddither=128;
int dither = 0;
int rando = 0;
int sampleSpeed = 0;

unsigned char SYNC = 0x7F;
int last_sequence_number = 0;

unsigned char hpsdrdata[1032];
unsigned char broadcastReply[60];



int main(int argc, char **argv)
{

	pthread_t pid0, pid1, pid2;
    pthread_create(&pid0, NULL, spiReader, NULL);  
	pthread_create(&pid2, NULL, packetreader, NULL); 
	pthread_create(&pid1, NULL, spiWriter, NULL);	
	spi_init(1);
	udp_init();
	
while(1)
{
	usleep(1);
}

return 0;	
}

/*******************************Functions**************************/

void put_tx_buffer(unsigned char  value) {
    tx_buffer[fill_tx] = value;    
    fill_tx = (fill_tx + 1) % TX_MAX; 
}


int isValidFrame(char* data) {
	return (data[8] == SYNC && data[9] == SYNC && data[10] == SYNC && data[520] == SYNC && data[521] == SYNC && data[522] == SYNC);
}


void fillDiscoveryReplyMessage() {
	int i = 0;
	for (i; i < 60; i++) {
		broadcastReply[i] = 0x00;
	}
	i = 0;
	broadcastReply[i++] = 0xEF;
	broadcastReply[i++] = 0xFE;
	broadcastReply[i++] = 0x02;

	broadcastReply[i++] =  0x00; // MAC
	broadcastReply[i++] =  0x01;
	broadcastReply[i++] =  0x02;
	broadcastReply[i++] =  0x03;
	broadcastReply[i++] =  0x04;
	broadcastReply[i++] =  0x05;
	broadcastReply[i++] =  31;
	broadcastReply[i++] =  6; //0x10; // Hermes boardtype public static final
									// int DEVICE_HERMES_LITE = 6;
}


int att11 = 0;
int prevatt11 = 0;
int att523 = 0;
int prevatt523 = 0;

void handlePacket(char* buffer){

	if (buffer[2] == 2) {
		printf("Discovery packet received \n");
		printf("IP-address %d.%d.%d.%d  \n", 
							remaddr.sin_addr.s_addr&0xFF,
                            (remaddr.sin_addr.s_addr>>8)&0xFF,
                            (remaddr.sin_addr.s_addr>>16)&0xFF,
                            (remaddr.sin_addr.s_addr>>24)&0xFF);
		printf("Discovery Port %d \n", ntohs(remaddr.sin_port));
		
		fillDiscoveryReplyMessage();
		
		if (sendto(fd, broadcastReply, sizeof(broadcastReply), 0, (struct sockaddr *)&remaddr, addrlen) < 0)
			printf("error sendto");
		
	} else if (buffer[2] == 4) {
			if (buffer[3] == 1 || buffer[3] == 3) {
				printf("Start Port %d \n", ntohs(remaddr.sin_port));
				running = 1;
				printf("SDR Program sends Start command \n");
				return;
			} else {
				running = 0;
				last_sequence_number = 0;
				printf("SDR Program sends Stop command \n");
				return;
			}
		}
	if (isValidFrame(buffer)) {
	
		 MOX = ((buffer[11] & 0x01)==0x01) ? 1:0;
	
		if ((buffer[11] & 0xFE)  == 0x14) {
			att = (buffer[11 + 4] & 0x1F);
			att11 = att;
		}
		
		if ((buffer[523] & 0xFE)  == 0x14) {
			att = (buffer[523 + 4] & 0x1F);
			att523 = att;
		}
	
		if ((buffer[11] & 0xFE)  == 0x00) {
			nrx = (((buffer[11 + 4] & 0x38) >> 3) + 1);
			
			sampleSpeed = (buffer[11 + 1] & 0x03);
			
			dither = 0;
			if ((buffer[11 + 3] & 0x08) == 0x08)
				dither = 1; 
						
			rando = 0;
			if ((buffer[11 + 3] & 0x10) == 0x10)
				rando = 1;
		}
		
		if ((buffer[523] & 0xFE)  == 0x00) {
			
			dither = 0;
			if ((buffer[523 + 3] & 0x08) == 0x08)
				dither = 1; 
					
			rando = 0;
			if ((buffer[523 + 3] & 0x10) == 0x10)
				rando = 1;
		}
		// Powersdr and Alans software are following a different patttern
		// this program does not known which package is calling 
		// by looking which value is changing... that will be the att value
		if (prevatt11 != att11) 
		{
			att = att11;
			prevatt11 = att11;
		}
		if (prevatt523 != att523) 
		{
			att = att523;
			prevatt523 = att523;
		}
			
		if ((buffer[11] & 0xFE)  == 0x00) {
			nrx = (((buffer[11 + 4] & 0x38) >> 3) + 1);
		}
		if ((buffer[523] & 0xFE)  == 0x00) {
			nrx = (((buffer[523 + 4] & 0x38) >> 3) + 1);
		}
		if (hold_nrx != nrx) {
			hold_nrx=nrx;
			printf("aantal rx %d \n", nrx);
		}
		
		// select Command
		if ((buffer[11] & 0xFE) == 0x02)
        {
            txfreq = ((buffer[11 + 1] & 0xFF) << 24) + ((buffer[11+ 2] & 0xFF) << 16)
                    + ((buffer[11 + 3] & 0xFF) << 8) + (buffer[11 + 4] & 0xFF);
        }
        if ((buffer[523] & 0xFE) == 0x02)
        {
            txfreq = ((buffer[523 + 1] & 0xFF) << 24) + ((buffer[523+ 2] & 0xFF) << 16)
                    + ((buffer[523 + 3] & 0xFF) << 8) + (buffer[523 + 4] & 0xFF);
        }
		
		if ((buffer[11] & 0xFE) == 0x04)
        {
            freq = ((buffer[11 + 1] & 0xFF) << 24) + ((buffer[11+ 2] & 0xFF) << 16)
                    + ((buffer[11 + 3] & 0xFF) << 8) + (buffer[11 + 4] & 0xFF);
        }
        if ((buffer[523] & 0xFE) == 0x04)
        {
            freq = ((buffer[523 + 1] & 0xFF) << 24) + ((buffer[523+ 2] & 0xFF) << 16)
                    + ((buffer[523 + 3] & 0xFF) << 8) + (buffer[523 + 4] & 0xFF);
        }
		
		if ((buffer[11] & 0xFE) == 0x06)
        {
            freq2 = ((buffer[11 + 1] & 0xFF) << 24) + ((buffer[11+ 2] & 0xFF) << 16)
                    + ((buffer[11 + 3] & 0xFF) << 8) + (buffer[11 + 4] & 0xFF);
        }
        if ((buffer[523] & 0xFE) == 0x06)
        {
            freq2 = ((buffer[523 + 1] & 0xFF) << 24) + ((buffer[523+ 2] & 0xFF) << 16)
                    + ((buffer[523 + 3] & 0xFF) << 8) + (buffer[523 + 4] & 0xFF);
        }

        // select Command
        if ((buffer[523] & 0xFE) == 0x12)
        {
            drive_level = buffer[524];  
        }
		
		if ((holdatt != att) || (holddither != dither)) {
			holdatt = att;
			holddither = dither;
			printf("att =  %d ", att);printf("dither =  %d ", dither);printf("rando =  %d ", rando);
			printf("code =  %d \n", (((rando << 6) & 0x40) | ((dither <<5) & 0x20) |  (att & 0x1F)));
			printf("att11 = %d and att523 = %d\n", att11, att523);
		}
		if (holdfreq != freq) {
			holdfreq = freq;
			printf("frequency %d en aantal rx %d \n", freq, nrx);
		}
		if (holdfreq2 != freq2) {
			holdfreq2 = freq2;
			printf("frequency %d en aantal rx %d \n", freq2, nrx);
		}
		if (holdtxfreq != txfreq) {
			holdtxfreq = txfreq;
			printf("TX frequency %d\n", txfreq);
		}
		
		//lees data en vul buffers
		int frame = 0;
		for (frame; frame < 2; frame++)
		{
			int coarse_pointer = frame * 512 + 8;

			int j = 8;
			for (j; j < 512; j += 8)
			{
				int k = coarse_pointer + j;

				// M  (MSB first) L and R channel 2 * 16 bits
				// send data to audio driver...beter latency......than using VAC.
				audiooutputbuffer[audiocounter++] = buffer[k + 0];
				audiooutputbuffer[audiocounter++] = buffer[k + 1];
				audiooutputbuffer[audiocounter++] = buffer[k + 2];
				audiooutputbuffer[audiocounter++] = buffer[k + 3];
				if (audiocounter ==  1024) {
					audiocounter = 0;
				}
	
				// TX IQ
				//MSB first according to protocol. (I and Q samples 2 * 16 bits)
				if (MOX) {
				
					while ( gpioRead(20) == 1) {};	// wait if TX buffer is full.
					
					sem_wait(&tx_empty);
					int i = 0;
					for (i; i < 4; i++){
						put_tx_buffer(buffer[k + 4 + i]);			
					}
					sem_post(&tx_full);
				}
			}
		}
	}
}



unsigned char readPackets(void) {
	unsigned char buffer[2048];
	
	recvlen = recvfrom(fd, buffer, sizeof(buffer), 0, (struct sockaddr *)&remaddr, &addrlen);
	if (recvlen > 0)
	printf(buffer);	
    return *buffer;				//handlePacket(buffer);
}


int udp_init (void)
{
		/* create a UDP socket */
	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("cannot create socket\n");
		return 0;
	}
	struct timeval timeout;      
    timeout.tv_sec = 0;
    timeout.tv_usec = TIMEOUT_MS;

	if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO,(char*)&timeout,sizeof(timeout)) < 0)
		perror("setsockopt failed\n");
		
	/* bind the socket to any valid IP address and a specific port */
	memset((char *)&myaddr, 0, sizeof(myaddr));
	myaddr.sin_family = AF_INET;
	myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	myaddr.sin_port = htons(SERVICE_PORT);

	if (bind(fd, (struct sockaddr *)&myaddr, sizeof(myaddr)) < 0) {
		perror("bind failed");
		return 0;
	}
}



int spi_init (unsigned long speed)
{
		if (gpioInitialise() < 0) {
		fprintf(stderr,"hpsdr_protocol (original) : gpio could not be initialized. \n");
		exit(-1);
	}
	
	gpioSetMode(13, PI_INPUT); 	//rx1_FIFOEmpty
	gpioSetMode(16, PI_INPUT);	//rx2_FIFOEmpty
	gpioSetMode(20, PI_INPUT); 
	gpioSetMode(21, PI_OUTPUT); 
	
	rx1_spi_handler = spiOpen(0, 15625000, 49155);  //channel 0
	if (rx1_spi_handler < 0) {
		fprintf(stderr,"radioberry_protocol: spi bus rx1 could not be initialized. \n");
		exit(-1);
	}
	
	rx2_spi_handler = spiOpen(1, 15625000, 49155); 	//channel 1
	if (rx2_spi_handler < 0) {
		fprintf(stderr,"radioberry_protocol: spi bus rx2 could not be initialized. \n");
		exit(-1);
	}

	printf("init done \n");
	
}


void *spiWriter(void *arg) {
while (1)
{
	//printf("Writer\r\n");
	usleep(1);
}
}

void *spiReader(void *arg) {
while (1)	
{
	//printf("Reader\r\n");
	usleep(1);
}
}


void *packetreader(void *arg) {
while (1)	
{
	readPackets();
	//printf("PacketReader\r\n");
	usleep(1);
}
}


