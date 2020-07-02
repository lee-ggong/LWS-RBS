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

#define BDPORTNUM 9005
#define TPPORTNUM 9000
#define BROADNUM 100

int main(void) {
    char buf[256], endst[50] = "END", myIP[INET_ADDRSTRLEN];
    struct sockaddr_in brd, sin, myAddr;
    struct timeval tv_cli, current;
    int32_t offset_time, offset_utime, loc_time[BROADNUM], loc_utime[BROADNUM]; 
    int bd, sd, n, index, val, brdlen = sizeof(brd), errn, optval = 1;

    while (1) {

        if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1){ 
            perror("socket");
            exit(1);
        }

        memset((char *)&sin, '\0', sizeof(sin)); //socket 구조체에 값을 지정(&ser-메모리의 시작 주소, \0-메모리에 채우고자 하는 값, size-채우고자하는 메모리의 크기)
        sin.sin_family = AF_INET; //socket family를 AF_INET으로 지정
        sin.sin_port = htons(TPPORTNUM); 
        sin.sin_addr.s_addr = inet_addr("192.168.0.159"); //소켓 주소 구조체에 서버의 주소를 지정

        while (1) {
            errn = connect(sd, (struct sockaddr *)&sin, sizeof(sin));
            if (errn == 0) {
                break;
            }
        }

        if (recv(sd, myIP, sizeof(myIP) + 1, 0) == -1) {
            perror("recv");
            exit(1);
        }

        printf("IP: %s\n", myIP);

    // Broadcasting phase

        if ((bd = socket(AF_INET, SOCK_DGRAM, 0)) == -1){ //socket을 생성(연결 성공시 0을 리턴 실패시 -1을 리턴)
            perror("socket"); //에러메시지를 출력하는 함수
            exit(1); //1을 반환하면서 프로그램 종료
        }

        memset((char *)&brd, '\0', sizeof(brd)); //socket 구조체에 값을 지정(&ser-메모리의 시작 주소, \0-메모리에 채우고자 하는 값, size-채우고자하는 메모리의 크기)
        brd.sin_family = AF_INET; //socket family를 AF_INET으로 지정
        brd.sin_port = htons(BDPORTNUM); 
        brd.sin_addr.s_addr = inet_addr(myIP); //소켓 주소 구조체에 서버의 주소를 지정

        if ((setsockopt(bd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval))) == -1){ //사용중인 포트 재사용 옵션
            perror("setsockopt");
            exit(1);
        }

        if (bind(bd, (struct sockaddr *)&brd, sizeof(brd))) { 
            perror("bind");
            exit(1);
        }
    
    
        int32_t timelen = 0;
        memset((int32_t *)&loc_time, '\0', sizeof(loc_time));
        memset((int32_t *)&loc_utime, '\0', sizeof(loc_utime));

        while (1) {

            n = recvfrom(bd, buf, 255, 0, (struct sockaddr *)&brd, &brdlen); //서버가 보낸 msg를 recvfrom 함수로 수신(bd, buf-전송받은 msg를 저장할 메모리 주소, msg의 크기, 0-데이터를 주고받는 방법을 지정한 플래그, msg를 보내는 서버의 주소, 주소의 크기)

            index = atoi(buf);

            if (strcmp(endst, buf)==0 || timelen == BROADNUM)
                break;  
        
            if ((ioctl(bd, SIOCGSTAMP, &tv_cli)) == -1){ 
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
        if ((send(sd, &loc_time[0], sizeof(loc_time)+1, 0)) == -1) {
            perror("send");
            exit(1);
        }    
        if ((send(sd, &loc_utime[0], sizeof(loc_utime)+1, 0)) == -1) {
            perror("send");
            exit(1);
        }

    // Offset phase
        if (recv(sd, buf, sizeof(buf) + 1, 0) == -1) {
            perror("recv");
            exit(1);
        }

        if (strcmp(endst, buf) != 0){
            printf("** %s\n", buf);
            recv(sd, &offset_time, sizeof(offset_time), 0);
            recv(sd, &offset_utime, sizeof(offset_utime), 0);
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