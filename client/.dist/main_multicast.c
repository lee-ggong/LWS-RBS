//CLIENT SOCKET
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdint.h>

#define MULPORTNUM 9005
#define TPPORTNUM 9000
#define BROADNUM 10

int main(void) {
    char buf[256], endst[50] = "END";
    struct sockaddr_in mult, sin;
    struct ip_mreq joinAddr;
    struct timeval tv_cli, current;
    int32_t offset_time, offset_utime, loc_time[BROADNUM], loc_utime[BROADNUM], timelen = 0; 
    int mt, sd, n, index, multlen = sizeof(mult), broadcast = 1;

    if ((mt = socket(AF_INET, SOCK_DGRAM, 0)) == -1){ //socket을 생성(연결 성공시 0을 리턴 실패시 -1을 리턴)
        perror("socket"); //에러메시지를 출력하는 함수
        exit(1); //1을 반환하면서 프로그램 종료
    }

    memset((char *)&mult, '\0', sizeof(mult)); //socket 구조체에 값을 지정(&ser-메모리의 시작 주소, \0-메모리에 채우고자 하는 값, size-채우고자하는 메모리의 크기)
    mult.sin_family = AF_INET; //socket family를 AF_INET으로 지정
    mult.sin_port = htons(MULPORTNUM); 
    mult.sin_addr.s_addr = htonl(INADDR_ANY); //소켓 주소 구조체에 서버의 주소를 지정

    if (bind(mt, (struct sockaddr *)&mult, sizeof(mult))) { //17행에서 생성한 소켓을 bind 함수로 22~25행에서 설정한 IP, port 번호와 연결(실패시 -1을 리턴 성공시 0을 리턴)(mt-socket에서 선언한 구조체, &ser-AF_INET의 경우 sockaddr_in AF_UNIX의 경우 sockaddr, len-선언한 구조체의 크기)
        perror("bind");
        exit(1);
    }

    //멀티캐스트 그룹 가입을 위한 구조체
    joinAddr.imr_multiaddr.s_addr = inet_addr("239.0.0.222");    //Multicast group (=IP addr)
    joinAddr.imr_interface.s_addr = htonl(INADDR_ANY); 

    if ((setsockopt(mt, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&joinAddr, sizeof(joinAddr))) == -1){ 
        perror("setsockopt"); 
        exit(1); 
    }

    while (1) {
    
    // Broadcasting phase
        timelen = 0;
        memset((int32_t *)&loc_time, '\0', sizeof(loc_time));
        memset((int32_t *)&loc_utime, '\0', sizeof(loc_utime));
        while (1) {

            n = recvfrom(mt, buf, 255, 0, (struct sockaddr *)&mult, &multlen); //서버가 보낸 msg를 recvfrom 함수로 수신(mt, buf-전송받은 msg를 저장할 메모리 주소, msg의 크기, 0-데이터를 주고받는 방법을 지정한 플래그, msg를 보내는 서버의 주소, 주소의 크기)

            index = atoi(buf);

            if (strcmp(endst, buf)==0 || timelen == BROADNUM)
                break;  
        
            if ((ioctl(mt, SIOCGSTAMP, &tv_cli)) == -1){ 
                perror("ioctl"); 
                exit(1); 
            }
        
            if (index == timelen+1){ // UDP 정상 수신시
                loc_time[index-1] = tv_cli.tv_sec;
                loc_utime[index-1] = tv_cli.tv_usec;
            }
            else{ // UDP 손실시
                loc_time[timelen] = 0;
                loc_utime[timelen] = 0;
                loc_time[index-1] = tv_cli.tv_sec;
                loc_utime[index-1] = tv_cli.tv_usec;
                timelen = index-1;
            }

            printf("** From Server : %i packet\n", index);
            printf("** Local clock : %i.%i\n", loc_time[timelen], loc_utime[timelen]);

            timelen++;
        }
        printf("END BRD\n");

    // Time stamp phase
        if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1){ 
            perror("socket");
            exit(1);
        }

        memset((char *)&sin, '\0', sizeof(sin)); //socket 구조체에 값을 지정(&ser-메모리의 시작 주소, \0-메모리에 채우고자 하는 값, size-채우고자하는 메모리의 크기)
        sin.sin_family = AF_INET; //socket family를 AF_INET으로 지정
        sin.sin_port = htons(TPPORTNUM); 
        sin.sin_addr.s_addr = inet_addr("192.168.0.159"); //소켓 주소 구조체에 서버의 주소를 지정

        if (connect(sd, (struct sockaddr *)&sin, sizeof(sin))) {
            perror("connect");
            exit(1);
        }

        if ((send(sd, &loc_time[0], sizeof(loc_time)+1, 0)) == -1) {
            perror("send");
            exit(1);
        }
        printf("** send time\n");
    
        if ((send(sd, &loc_utime[0], sizeof(loc_utime)+1, 0)) == -1) {
            perror("send");
            exit(1);
        }
        printf("** send utime\n");


    // Offset phase
        if (recv(sd, buf, sizeof(buf) + 1, 0) == -1) {
            perror("recv");
            exit(1);
        }

        if (strcmp(endst, buf) != 0){
            printf("** %s\n", buf);
            recv(sd, &offset_time, sizeof(offset_time)+1, 0);
            recv(sd, &offset_utime, sizeof(offset_utime)+1, 0);

            printf("** offset : %i.%i\n", offset_time, offset_utime);

            gettimeofday(&current, 0);
            printf("** current time : %i.%i\n", current.tv_sec, current.tv_usec);

            current.tv_sec = current.tv_sec + offset_time;
            current.tv_usec = current.tv_usec + offset_utime;
            printf("** change time : %i.%i\n", current.tv_sec, current.tv_usec);

            settimeofday(&current, 0); // 관리자 권한 필요

            gettimeofday(&current, 0);
            printf("** current time : %i.%i\n", current.tv_sec, current.tv_usec);
        }
        close(sd);
    }
    return 0;
}