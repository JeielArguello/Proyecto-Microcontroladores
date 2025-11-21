#include <stdint.h>

#pragma pack(push,1)
typedef struct {
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
} IS_ISI;
#pragma pack(pop)

#pragma pack(push,1)
// IS_MCI (speed)
typedef struct {
    unsigned char PLID;
    short X;
    short Y;
    short Z;
    unsigned short Speed; // 0.1 m/s
    short Direction;
    short Heading;
    short AngVel;
} CarContOBJ;

// IS_NPL (rpm, gear)
typedef struct {
    IS_ISI H;
    unsigned char NumP;
    unsigned char Plid;
    char UName[24];
    char Plate[8];
    char CName[4];
    unsigned char Tyres[4];
    unsigned char H_Mass;
    unsigned char H_TRes;
    unsigned char Model;
    unsigned char Pass;
    unsigned short Spare;
    unsigned short SkinID;
    unsigned short RL_Traction;
    unsigned short RR_Traction;
    unsigned short Speed;
    unsigned short RPM;
    unsigned char Gear;
    unsigned char EngineLife;
    float Fuel;
    float FuelT;
} IS_NPL;
#pragma pack(pop)

#pragma pack(push,1)
struct IS_TINY {
    uint8_t Size;   // siempre 4
    uint8_t Type;   // ISP_TINY
    uint8_t ReqI;   // request ID (hay que devolverlo igual)
    uint8_t SubT;   // subtipo: TINY_NONE, TINY_PING, etc.
};
#pragma pack(pop)


#include <stdio.h>
#include <winsock2.h>

#pragma comment(lib, "ws2_32.lib")

#define ISF_NLP			  16	// bit  4: receive NLP packets
#define ISF_MCI			  32	// bit  5: receive MCI packets
#define INSIM_VERSION 9

// Packet types
enum PacketType {
    ISP_NONE,		//  0					: not used
	ISP_ISI,		//  1 - instruction		: insim initialise
	ISP_VER,		//  2 - info			: version info
	ISP_TINY,        //  3 - tiny			: multipropose packet
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

void process_packet(unsigned char *buf, SOCKET s) {
    unsigned char type = buf[1];

    printf("DEBUG: Received packet type=%d\n", type);

    // IS_TINY
    if (type == ISP_TINY) {
        struct IS_TINY tiny;
        memcpy(&tiny, buf, buf[0]);
        if (tiny.SubT == TINY_NONE) {
            // respond with same packet
            send(s, (char*)&tiny, sizeof(tiny), 0);
        }
    }

    //IS_VER
    if (type == ISP_VER) {
        printf("DEBUG: Received IS_VER packet\n");
    }


    // IS_NPL -> RPM, Gear
    if (type == 21) {
        unsigned short *rpm_ptr = (unsigned short *)(buf + 56);
        unsigned char *gear_ptr = (unsigned char *)(buf + 58);

        rpm = *rpm_ptr;
        gear = *gear_ptr;

        printf("DEBUG: RPM=%d Gear=%d\n", rpm, gear);
    }

    // IS_MCI -> speed
    if (type == 38) {
        // Speed está en 5to byte del primer carro
        unsigned short *speed_ptr = (unsigned short *)(buf + 8);
        speed = *speed_ptr;

        printf("DEBUG: Speed=%d\n", speed);
    }
}

int main() {
    WSADATA wsa;
    SOCKET s;
    struct sockaddr_in server;
    unsigned char buf[256];
    struct IS_TINY tiny;

    WSAStartup(MAKEWORD(2,2), &wsa);
    s = socket(AF_INET, SOCK_STREAM, 0);

    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    server.sin_family = AF_INET;
    server.sin_port = htons(20777);

    int res_connect = connect(s, (struct sockaddr *)&server, sizeof(server));
    if (res_connect != 0) {
        printf("Error conectando a LFS InSim\n");
        return 1;
    }

    // IS_ISI para iniciar InSim
    IS_ISI isi;
    isi.Size = sizeof(IS_ISI)/4;
    isi.Type = 1;        // ISP_ISI
    isi.ReqI = 1;
    isi.Zero = 0;

    isi.UDPPort = 0; // little endian OK
    isi.Flags = ISF_NLP; // | ISF_MCI; // pedir NLP y MCI

    isi.InSimVer = INSIM_VERSION;
    isi.Prefix = 0;
    isi.Interval = 100;  // 100 ms → opcional

    strcpy(isi.Admin, "");
    strcpy(isi.IName, "test");

    printf("ISI.flags=%d\n", isi.Flags);
    printf("ISI=%h\n", isi);
   
    printf("Enviando IS_ISI a LFS... size=%d\n", sizeof(isi));
    // Enviar IS_ISI a LFS
    int res = send(s, (char*)&isi, sizeof(isi), 0);

    //send(s, (char*)isi, sizeof(isi), 0);
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

            printf("RPM=%d  Speed=%d  Gear=%d\n", rpm, speed, gear);

            // acá enviás por HID a tu STM32
            // hid_write(device, pkt, 6);
        }

       
    }

    closesocket(s);
    WSACleanup();
    return 0;
}
