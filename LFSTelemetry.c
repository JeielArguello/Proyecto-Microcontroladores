#include <stdint.h>
#include <stdio.h>
#include <winsock2.h>


#define ISF_MCI			  32	// bit  5: receive MCI packets
#define INSIM_VERSION 9
#define MCI_MAX_CARS  16


#pragma pack(push,1)

typedef struct  // Car info in 28 bytes - there is an array of these in the MCI (below)
{
	uint16_t	Node;		// current path node
	uint16_t	Lap;		// current lap
	uint8_t	    PLID;		// player's unique id
	uint8_t	    Position;	// current race position: 0 = unknown, 1 = leader, etc...
	uint8_t	    Info;		// flags and other info - see below
	uint8_t 	Sp3;
	int		    X;			// X map (65536 = 1 metre)
	int		    Y;			// Y map (65536 = 1 metre)
	int		    Z;			// Z alt (65536 = 1 metre)
	uint16_t	Speed;		// speed (32768 = 100 m/s)
	uint16_t	Direction;	// car's motion if Speed > 0: 0 = world y direction, 32768 = 180 deg
	uint16_t	Heading;	// direction of forward axis: 0 = world y direction, 32768 = 180 deg
	short	    AngVel;		// signed, rate of change of heading: (16384 = 360 deg/s)
}CompCar;
#pragma pack(pop)

#pragma pack(push,1)
struct IS_ISI{
    uint8_t  Size;       // 44
    uint8_t  Type;       // ISP_ISI = 1
    uint8_t  ReqI;       // if non-zero LFS will send IS_VER
    uint8_t  Zero;       // 0

    uint16_t UDPPort;    // puerto para respuestas UDP (0..65535)
    uint16_t Flags;     // flags InSim
    
    uint8_t  InSimVer;    //The INSIM_VERSION used by your program
    uint8_t  Prefix;     // host message prefix char
    uint16_t Interval;   // ms between NLP or MCI (0 = none)

    char Admin[16];      // admin password (si LFS la tiene)
    char IName[16];      // nombre corto del programa
};

struct IS_TINY {
    uint8_t Size;   // siempre 4
    uint8_t Type;   // ISP_TINY
    uint8_t ReqI;   // request ID (hay que devolverlo igual)
    uint8_t SubT;   // subtipo: TINY_NONE, TINY_PING, etc.
};

struct IS_NPL // New PLayer joining race (if PLID already exists, then leaving pits)
{
	uint8_t 	Size;		// 76
	uint8_t	    Type;		// ISP_NPL
	uint8_t	    ReqI;		// 0 unless this is a reply to an TINY_NPL request
	uint8_t 	PLID;		// player's newly assigned unique id

	uint8_t 	UCID;		// connection's unique id
	uint8_t	    PType;		// bit 0: female / bit 1: AI / bit 2: remote
	uint16_t	Flags;		// player flags

	char	    PName[24];	// nickname
	char	    Plate[8];	// number plate - NO ZERO AT END!

	char	    CName[4];	// car name
	char	    SName[16];	// skin name - MAX_CAR_TEX_NAME
	uint8_t 	Tyres[4];	// compounds

	uint8_t 	H_Mass;		// added mass (kg)
	uint8_t	    H_TRes;		// intake restriction
	uint8_t 	Model;		// driver model
	uint8_t	    Pass;		// passengers byte

	uint8_t 	RWAdj;		// low 4 bits: tyre width reduction (rear)
	uint8_t	    FWAdj;		// low 4 bits: tyre width reduction (front)
	uint8_t	    Sp2;
	uint8_t	    Sp3;

	uint8_t	    SetF;		// setup flags (see below)
	uint8_t	    NumP;		// number in race - ZERO if this is a join request
	uint8_t	    Config;		// configuration (see below)
	uint8_t	    Fuel;		// /showfuel yes: fuel percent / no: 255
};

struct IS_MCI  // Multi Car Info - if more than MCI_MAX_CARS in race then more than one is sent
{
	byte	Size;		// 4 + NumC * 28
	byte	Type;		// ISP_MCI
	byte	ReqI;		// 0 unless this is a reply to an TINY_MCI request
	byte	NumC;		// number of valid CompCar structs in this packet

	CompCar	Info[MCI_MAX_CARS]; // car info for each player, 1 to MCI_MAX_CARS (NumC)
};
#pragma pack(pop)




#pragma comment(lib, "ws2_32.lib")

// Packet types
enum PacketType {
    ISP_NONE,		//  0					: not used
	ISP_ISI,		//  1 - instruction		: insim initialise
	ISP_VER,		//  2 - info			: version info
	ISP_TINY,       //  3 - tiny			: multipropose packet
    ISP_NPL = 21, 
    ISP_MCI = 38       
};

// Tiny subtypes
enum TinySubType {
    TINY_NONE = 0,
   TINY_VER,		//  1 - info request	: get version
	TINY_CLOSE,		//  2 - instruction		: close insim
	TINY_PING,
};

unsigned short rpm = 0;
unsigned short speed = 0;
unsigned char gear = 0;


void printCompCar(const CompCar *c)
{
    printf("PLID: %d | Pos: %d | Lap: %d | Node: %d\n",
           c->PLID, c->Position, c->Lap, c->Node);

    printf("  X: %.3f m | Y: %.3f m | Z: %.3f m\n",
           c->X / 65536.0, c->Y / 65536.0, c->Z / 65536.0);

    printf("  Speed: %.2f km/h | Heading: %.2f deg | AngVel: %.2f deg/s\n",
           c->Speed * 100.0 / 32768.0 *3.6,
           c->Heading * 180.0 / 32768.0,
           c->AngVel * 360.0 / 16384.0);

    printf("  Flags: ");
    if (c->Info & 1)   printf("[BLUE] ");
    if (c->Info & 2)   printf("[YELLOW] ");
    if (c->Info & 32)  printf("[LAG] ");
    if (c->Info & 64)  printf("[FIRST] ");
    if (c->Info & 128) printf("[LAST] ");
    printf("\n\n");
}

void process_packet(unsigned char *buf, SOCKET s) {
    unsigned char type = buf[1];
    int len = buf[0] * 4;

    printf("DEBUG: Received packet type=%d\n", type);

    // IS_TINY
    if (type == ISP_TINY) {
        struct IS_TINY tiny;
        memcpy(&tiny, buf, len);
        if (tiny.SubT == TINY_NONE) {
            // respond with same packet
            send(s, (char*)&tiny, sizeof(tiny), 0);
        }
    }

    //IS_VER
    if (type == ISP_VER) {
        printf("DEBUG: Received IS_VER packet\n");
    }


    // IS_MCI -> RPM, Gear
    if (type == ISP_MCI) {
        struct IS_MCI mci;
        memcpy(&mci, buf, len);
        
        printf("DEBUG: Packet size=%d \n",mci.Size);
        printCompCar(mci.Info);

    }

    // IS_NPL -> speed
    if (type == ISP_NPL) {
        struct IS_NPL npl;
        memcpy(&npl, buf, len);


        
        // Speed está en 5to byte del primer carro
        printf("DEBUG: NumP=%d\n", npl.NumP);

        printf("DEBUG: Player unique ID=%d\n", npl.PLID);
    }
}



int main() {
    WSADATA wsa;
    SOCKET s;
    struct sockaddr_in server;
    unsigned char buf[2048];

    WSAStartup(MAKEWORD(2,2), &wsa);
    s = socket(AF_INET, SOCK_STREAM, 0);

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    server.sin_port = htons(20777);

    int res_connect = connect(s, (struct sockaddr *)&server, sizeof(server));
    if (res_connect != 0) {
        printf("Error conectando a LFS InSim\n");
        return 1;
    }

    // IS_ISI para iniciar InSim
    struct IS_ISI isi;
    isi.Size = sizeof(struct IS_ISI)/4;
    isi.Type = 1;        // ISP_ISI
    isi.ReqI = 1;
    isi.Zero = 0;

    isi.UDPPort = 0; // little endian OK
    isi.Flags = ISF_MCI; // pedir NLP y MCI

    isi.InSimVer = INSIM_VERSION;
    isi.Prefix = 0;
    isi.Interval = 10;  // 100 ms → opcional

    strcpy(isi.Admin, "");
    strcpy(isi.IName, "test");

    printf("ISI.flags=%d\n", isi.Flags);
   
    printf("Enviando IS_ISI a LFS... size=%d\n", sizeof(isi));
    // Enviar IS_ISI a LFS
    int res = send(s, (char*)&isi, sizeof(isi), 0);

    if (res < 0) {
        printf("Error enviando IS_ISI\n");
        return 1;
    }

    printf("Conectado a LFS InSim\n");

    while (1) {
        int len = recv(s, (char*)buf, sizeof(buf), 0);
        if (len < 0) {
            printf("Error recibiendo datos\n");
            break;
        }
        
        if (len > 0) {
            process_packet(buf,s);

            // Crear paquete de 6 bytes (tu formato)
            unsigned char pkt[6];
            pkt[0] = 0x01;                           // Telemetría
            pkt[1] = rpm & 0xFF;
            pkt[2] = (rpm >> 8) & 0xFF;
            pkt[3] = speed & 0xFF;
            pkt[4] = (speed >> 8) & 0xFF;
            pkt[5] = gear;

            //printf("RPM=%d  Speed=%d  Gear=%d\n", rpm, speed, gear);

            // acá enviás por HID a tu STM32
            // hid_write(device, pkt, 6);
        }

       
    }

    closesocket(s);
    WSACleanup();
    return 0;
}
