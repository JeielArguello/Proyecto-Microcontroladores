#include <winsock2.h>
#include <stdio.h>
#include <stdint.h>

#include "hidapi.h"
#include <wchar.h>

#pragma pack(push,1)
typedef struct {
    unsigned    Time;
    char        Car[4];
    unsigned short Flags;
    unsigned char Gear;     // <- La marcha
    unsigned char PLID;
    float       Speed;      // m/s
    float       RPM;
    float       Turbo;
    float       EngTemp;
    float       Fuel;
    float       OilPressure;
    float       OilTemp;
    unsigned    DashLights;
    unsigned    ShowLights;
    float       Throttle;
    float       Brake;
    float       Clutch;
    char        Display1[16];
    char        Display2[16];
    int         ID;
} OutGaugePack;
#pragma pack(pop)


#define HID_REPORT_SIZE 7

int main() {
    WSADATA wsa;
    SOCKET sock;
    struct sockaddr_in addr;
    OutGaugePack* og;

    uint8_t buf[2048]; 

    WSAStartup(MAKEWORD(2,2), &wsa);

    sock = socket(AF_INET, SOCK_DGRAM, 0);

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;  // escuchar en cualquier IP local
    addr.sin_port = htons(30000);        // mismo puerto que OutGauge Port

    if(bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        printf("Error haciendo bind\n");
        return 1;
    }

    
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

    printf("Esperando paquetes OutGauge...\n");

    while(1) {
        int len = recv(sock, (char*)&buf, sizeof(buf), 0);
        if(len < 0) {
            printf("Error recibiendo paquete\n");
            break;
        }
        
        og = (OutGaugePack*)buf;

        // Imprimir marcha, velocidad y RPM
        char gear_name;
        switch(og->Gear) {
            case 0: gear_name = 'R'; break;
            case 1: gear_name = 'N'; break;
            default: gear_name = og->Gear-1+'0'; break;
        }

        printf("Gear: %c | Speed: %.2f km/h | RPM: %.0f\n",
               gear_name,
               og->Speed * 3.6,  // convertir m/s a km/h
               og->RPM);

        static float prevRPM=0, prevSpeed=0;
        static float  prevGear=0;

        uint16_t RPM=(uint16_t)og->RPM, Speed=(uint16_t)(og->Speed * 3.6f);
        
        uint8_t gear_num;
        switch(og->Gear) {
            case 0: gear_num = 255; break;
            case 1: gear_num = 0; break;
            default: gear_num = og->Gear-1; break;
        }

        if (dev)
        {
            if (og->RPM!=prevRPM || og->Speed!=prevSpeed || og->Gear!=prevGear) {
                uint8_t report[HID_REPORT_SIZE];
                report[0]=0x00;
                report[1]=0x01; // Report ID
                report[2]=RPM&0xFF; report[3]=RPM>>8;
                report[4]=Speed&0xFF; report[5]=Speed>>8;
                report[6]=gear_num;
                prevRPM=og->RPM; prevSpeed=og->Speed; prevGear=og->Gear;
                int res = hid_write(dev, report, sizeof(report));
                Sleep(5);
                if (res < 0) {
                    // Se produjo un error
                    printf("res=%d\n", res);
                    printf("Error en hid_write: %ls\n", hid_error(dev));
                    // Puedes verificar el código de error específico del sistema operativo si es necesario
                } else if (res != sizeof(report)) {
                    // No se escribieron todos los bytes esperados (común en algunas implementaciones de Windows)
                    printf("Advertencia: Se escribieron %d bytes, se esperaban %zu.\n", res, sizeof(buf));
                }


            }
        }
    }

    if (dev) hid_close(dev);
    hid_exit();
    closesocket(sock);
    WSACleanup();
    return 0;
}
