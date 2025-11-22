#include <stdint.h>
#include <stdio.h>
#include <Winsock2.h>
#include "hidapi.h"
#include <wchar.h>


#define MAX_STR 255

#define UDP_PORT 20777


#pragma comment(lib,"ws2_32.lib")
// https://forums.codemasters.com/discussion/132711/f1-24-udp-specification
#pragma pack(push,1)
typedef struct {
    uint16_t m_speed;
    float    m_throttle;
    float    m_steer;
    float    m_brake;
    uint8_t  m_clutch;
    int8_t   m_gear;
    uint16_t m_engineRPM;
    uint8_t  m_drs;
    uint8_t  m_revLightsPercent;
    uint16_t m_revLightsBitValue;
    uint16_t m_brakesTemperature[4];
    uint8_t  m_tyresSurfaceTemperature[4];
    uint8_t  m_tyresInnerTemperature[4];
    uint16_t m_engineTemperature;
    float    m_tyresPressure[4];
    uint8_t  m_surfaceType[4];
} CarTelemetryData;
/*
struct CarTelemetryData { 
    uint16    m_speed;                    // Speed of car in kilometres per hour 
    float     m_throttle;                 // Amount of throttle applied (0.0 to 1.0) 
    float     m_steer;                    // Steering (-1.0 (full lock left) to 1.0 (full lock right)) 
    float     m_brake;                    // Amount of brake applied (0.0 to 1.0) 
    uint8     m_clutch;                   // Amount of clutch applied (0 to 100) 
    int8      m_gear;                     // Gear selected (1-8, N=0, R=-1) 
    uint16    m_engineRPM;                // Engine RPM 
    uint8     m_drs;                      // 0 = off, 1 = on 
    uint8     m_revLightsPercent;         // Rev lights indicator (percentage) 
    uint16    m_revLightsBitValue;        // Rev lights (bit 0 = leftmost LED, bit 14 = rightmost LED) 
    uint16    m_brakesTemperature[4];     // Brakes temperature (celsius) 
    uint8     m_tyresSurfaceTemperature[4]; // Tyres surface temperature (celsius) 
    uint8     m_tyresInnerTemperature[4]; // Tyres inner temperature (celsius) 
    uint16    m_engineTemperature;        // Engine temperature (celsius) 
    float     m_tyresPressure[4];         // Tyres pressure (PSI) 
    uint8     m_surfaceType[4];           // Driving surface, see appendices 
};*/ 
 
typedef struct {
    uint16_t m_packetFormat;
    uint8_t  m_gameYear;
    uint8_t  m_gameMajorVersion;
    uint8_t  m_gameMinorVersion;
    uint8_t  m_packetVersion;
    uint8_t  m_packetId;
    uint64_t m_sessionUID;
    float    m_sessionTime;
    uint32_t m_frameIdentifier;
    uint32_t m_overallFrameIdentifier;
    uint8_t  m_playerCarIndex;
    uint8_t  m_secondaryPlayerCarIndex;
} PacketHeader;

/* struct PacketHeader{ 
    uint16    m_packetFormat;            // 2024 
    uint8     m_gameYear;                // Game year - last two digits e.g. 24 
    uint8     m_gameMajorVersion;        // Game major version - "X.00" 
    uint8     m_gameMinorVersion;        // Game minor version - "1.XX" 
    uint8     m_packetVersion;           // Version of this packet type, all start from 1 
    uint8     m_packetId;                // Identifier for the packet type, see below 
    uint64    m_sessionUID;              // Unique identifier for the session 
    float     m_sessionTime;             // Session timestamp 
    uint32    m_frameIdentifier;         // Identifier for the frame the data was retrieved on 
    uint32    m_overallFrameIdentifier;  // Overall identifier for the frame the data was retrieved 
                                         // on, doesn't go back after flashbacks 
    uint8     m_playerCarIndex;          // Index of player's car in the array 
    uint8     m_secondaryPlayerCarIndex; // Index of secondary player's car in the array (splitscreen) 
                                         // 255 if no second player 
};  */
typedef struct {
    PacketHeader     m_header;
    CarTelemetryData m_carTelemetryData[22];
    uint8_t          m_mfdPanelIndex;
    uint8_t          m_mfdPanelIndexSecondaryPlayer;
    int8_t           m_suggestedGear;
    //uint8_t          _padding[3]; // <--- fuerza a 1352 total
} PacketCarTelemetryData;
/*

struct PacketCarTelemetryData { 
    PacketHeader    	m_header;	      // Header 
    CarTelemetryData    m_carTelemetryData[22]; 
    uint8               m_mfdPanelIndex;       // Index of MFD panel open - 255 = MFD closed 
                                               // Single player, race – 0 = Car setup, 1 = Pits 
                                               // 2 = Damage, 3 =  Engine, 4 = Temperatures 
                                               // May vary depending on game mode 
    uint8               m_mfdPanelIndexSecondaryPlayer;   // See above 
    int8                m_suggestedGear;       // Suggested gear for the player (1-8) 
                                               // 0 if no gear suggested 
}; */
#pragma pack(pop)

#pragma pack(push,1)
typedef struct {
    uint16_t speed;
    uint8_t  gear;
    uint16_t rpm;
} ShortTelemetry;
#pragma pack(pop)

#define HID_REPORT_SIZE 7
int main(void)
{
    int res;
	//unsigned char buf[65];
	wchar_t wstr[MAX_STR];
	int i;

	// Initialize the hidapi library
	res = hid_init();
    if(res != 0) {
        printf("hid_init failed\n");
        return 1;
    }

    WSADATA wsa;  WSAStartup(MAKEWORD(2,2), &wsa);

    SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (!sock)
    {
        printf("No se pudo crear el socket UDP\n");
        return 1;
    }
    
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(UDP_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;
    bind(sock, (struct sockaddr*)&addr, sizeof(addr));

    hid_device *dev = hid_open(0x0483, 0x5750, NULL); // Tu VID/PID STM32
    if(dev == NULL){
        printf("No se encontro el HUD USB\n");
        hid_exit();
        Sleep(2000);
        return 1;
    }

    if (dev){
        printf("HUD USB conectado\n");
        uint8_t report[HID_REPORT_SIZE];
        report[0]=0x00; // Report ID
        report[1]=0x00; // 
         report[2]=0xA7;
        report[3]=0xF0; report[4]=0xB8;
        report[5]=0x00; report[6]=0x00;
        int res = hid_write(dev, report, sizeof(report));
        if (res < 0) {
            printf("Error en hid_write inicial: %ls\n", hid_error(dev));
        }else if (res != sizeof(report)) {
            printf("Advertencia: Se escribieron %d bytes, se esperaban %zu.\n", res, sizeof(report));
        } else {
            printf("Escritura inicial exitosa de %d bytes.\n", res);
        }
    }
    
    printf("Escuchando UDP (port %d)\n", UDP_PORT);
    
    printf("sizeof(CarTelemetryData)=%zu  sizeof(PacketCarTelemetryData)=%zu\n",
       sizeof(CarTelemetryData), sizeof(PacketCarTelemetryData));
    char* buf;
    buf = (char*)calloc(2048, 1);
    struct sockaddr_in from; int fromlen = sizeof(from);
    
    static uint16_t prevRPM=0, prevSpeed=0;
    static uint8_t  prevGear=0;

    while (1)
    {
        int n = recvfrom(sock, buf, 2048, 0,
                        (struct sockaddr*)&from, &fromlen);

        if (n <= 0) { 
            printf("recvfrom error\n");
            Sleep(10); 
            continue; 
        }

        
        PacketHeader *hdr= (PacketHeader*)buf;
        
        
        if (hdr->m_packetId != 6)  // 6 = Car Telemetry
            continue; 

        uint8_t playerIndex = hdr->m_playerCarIndex;
        if (playerIndex >= 22) {
            printf("Indice jugador invalido: %u\n", playerIndex);
            continue;
        }
        size_t base = sizeof(PacketHeader) + playerIndex * sizeof(CarTelemetryData);
        if (base + sizeof(CarTelemetryData) > (size_t)n){
            printf("sizeof(CarTelemetryData)=%zu\n", sizeof(CarTelemetryData));
            printf("base=%zu, n=%d\n", base, n);
            printf("Paquete demasiado pequenio para datos de telemetria\n");
            continue;
        } 

        CarTelemetryData *t = (CarTelemetryData*)(buf + base);

        uint16_t rpm = t->m_engineRPM;
        uint16_t speed = t->m_speed;
        uint8_t gear = t->m_gear;

        printf("RPM:%5u  SPD:%3ukm/h  G:%u\n", rpm, speed, gear);

        if (dev)
        {
            if (rpm!=prevRPM || speed!=prevSpeed || gear!=prevGear) {
                uint8_t report[HID_REPORT_SIZE];
                report[0]=0x00;
                report[1]=0x01; // Report ID
                report[2]=rpm&0xFF; report[3]=rpm>>8;
                report[4]=speed&0xFF; report[5]=speed>>8;
                report[6]=gear;
                prevRPM=rpm; prevSpeed=speed; prevGear=gear;
                int res = hid_write(dev, report, sizeof(report));
                Sleep(40);                
                if (res < 0) {
                    // Se produjo un error

                    printf("res=%d\n", res);
                    printf("Error en hid_write: %ls\n", hid_error(dev));
                    // Puedes verificar el código de error específico del sistema operativo si es necesario
                } else if (res != sizeof(report)) {
                    // No se escribieron todos los bytes esperados (común en algunas implementaciones de Windows)
                    printf("Advertencia: Se escribieron %d bytes, se esperaban %zu.\n", res, sizeof(buf));
                } else {
                    // Escritura exitosa
                    printf("Escritura exitosa de %d bytes.\n", res);
                }
                // Después de hid_write(handle, buf, sizeof(buf)); ...
                unsigned char read_buf[64]; // Tamaño del reporte de entrada
                int read_res = hid_read_timeout(dev, read_buf, sizeof(read_buf), 100); // Timeout de 100ms

                if (read_res > 0) {
                    // Datos recibidos, el dispositivo respondió, podemos escribir de nuevo.
                    printf("Respuesta recibida del dispositivo (%d bytes): ", read_res);
                } else if (read_res == 0) {
                    // Timeout, no hubo respuesta.
                    printf("No se recibio respuesta del dispositivo (timeout).\n");
                } else {
                    // Error de lectura.
                    printf("Error en hid_read: %ls\n", hid_error(dev));
                }


            }
        }
    }

    free(buf);
    if (dev) hid_close(dev);
    hid_exit();
    closesocket(sock);
    WSACleanup();
    return 0;
}
