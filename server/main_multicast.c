//SERVER SOCKET
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>

#define MULPORTNUM 9005
#define TPPORTNUM 9000
#define BROADNUM 10
#define NODENUM 2

int main(void) {
    char buf[256], ipad[NODENUM][INET_ADDRSTRLEN];
    struct sockaddr_in mult, sin, cli;
    float beacon_interval = 0;
    int min_offset = 0, offset = 0, optval = 1;
    int mt, sd, nsd[NODENUM], min, iter = 0, clen = sizeof(cli), multiTTL = 255;
    int32_t offset_time = 0, offset_utime = 0, loc_time[NODENUM][BROADNUM], loc_utime[NODENUM][BROADNUM], My_time[NODENUM][BROADNUM], My_utime[NODENUM][BROADNUM];

    if ((mt = socket(AF_INET, SOCK_DGRAM, 0)) == -1){ //UDP socket을 생성(연결 성공시 0을 리턴 실패시 -1을 리턴)
        perror("socket"); //에러메시지를 출력하는 함수
        exit(1); //1을 반환하면서 프로그램 종료
    }
    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1){ //TCP socket을 생성(연결 성공시 0을 리턴 실패시 -1을 리턴)
        perror("socket"); //에러메시지를 출력하는 함수
        exit(1); //1을 반환하면서 프로그램 종료
    }

    if ((setsockopt(mt , IPPROTO_IP, IP_MULTICAST_TTL, (char*)&multiTTL, sizeof(multiTTL))) == -1){ 
        perror("setsockopt"); //에러메시지를 출력하는 함수
        exit(1); //1을 반환하면서 프로그램 종료
    }
    if ((setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval))) == -1){ 
        perror("setsockopt"); //에러메시지를 출력하는 함수
        exit(1); //1을 반환하면서 프로그램 종료
    }

    memset((char *)&mult, '\0', sizeof(mult)); //socket 구조체에 값을 지정(&ser-메모리의 시작 주소, \0-메모리에 채우고자 하는 값, size-채우고자하는 메모리의 크기)
    mult.sin_family = AF_INET; //socket family를 AF_INET으로 지정
    mult.sin_port = htons(MULPORTNUM); 
    mult.sin_addr.s_addr = inet_addr("239.0.0.222"); //소켓 주소 구조체에 서버의 주소를 지정

    memset((char *)&sin, '\0', sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(TPPORTNUM); 
    sin.sin_addr.s_addr = inet_addr("192.168.0.159"); //소켓 주소 구조체에 서버의 주소를 지정

    if (bind(sd, (struct sockaddr *)&sin, sizeof(sin))) { 
        perror("bind");
        exit(1);
    }

    // Broadcasting phase
    for (int n = 1; n <= BROADNUM; n++) {
        sprintf(buf, "%d", n);

        if ((sendto(mt, buf, strlen(buf)+1, 0, (struct sockaddr *)&mult, sizeof(mult))) == -1) { //클라이언트에게 sendto 함수로 msg를 전송(mt, buf-전송할 msg를 저장한 메모리 주소, msg의 크기, 0-데이터를 주고받는 방법을 지정한 플래그, &cli-msg를 전송할 호스트의 주소, &clientlen-&cli의 크기)
            perror("sendto");
            exit(1);         
        }

        sleep(beacon_interval);
    }
    strcpy(buf, "END"); //종료문전달 

    if ((sendto(mt, buf, strlen(buf)+1, 0, (struct sockaddr *)&mult, sizeof(mult))) == -1) { //클라이언트에게 sendto 함수로 msg를 전송(mt, buf-전송할 msg를 저장한 메모리 주소, msg의 크기, 0-데이터를 주고받는 방법을 지정한 플래그, &cli-msg를 전송할 호스트의 주소, &clientlen-&cli의 크기)
        perror("sendto");
        exit(1);         
    }


    // Time stamp phase
    if (listen(sd, NODENUM)) { //cli로 부터 time값을 수신받기위해 대기
        perror("listen");
        exit(1);
    }

    for (int n = 0; n < NODENUM; n++) {
        if ((nsd[n] = accept(sd, (struct sockaddr *)&cli, &clen)) == -1) { //accept 함수로 클라이언트의 접속 요청을 수락하고 새로운 소켓 기술자를 생성해 nsd에 저장(sd, &cli-client의 IP, &clen-cli의 크기)
            perror("accept");
            exit(1);
        }

        inet_ntop(AF_INET, &(cli.sin_addr), ipad[n], INET_ADDRSTRLEN);

        if (recv(nsd[n], &loc_time[n][0], sizeof(loc_time[n])+1, 0) == -1) {
            perror("recv");
            exit(1);
        }
        
        if (recv(nsd[n], &loc_utime[n][0], sizeof(loc_utime[n])+1, 0) == -1) {
            perror("recv");
            exit(1);
        }
    }

    /*for (int n = 0; n < NODENUM; n++) {
        for (int i = 0; i < BROADNUM; i++) {
            printf("%i-th client %i-th stamp: %d.%d\n", n+1, i+1, loc_time[n][i], loc_utime[n][i]);
        }
    }*/

    // Offset phase
    for (int n = 0; n < NODENUM; n++) {
        for (int i = 0; i < BROADNUM; i++) { // 기준 node 설정을 위한 기지국과 단말간 offset 계산

            if (loc_time[n][i] != 0){ // 손실된 UDP는 무시
                offset_time = offset_time + My_time[n][i] - loc_time[n][i];
                offset_utime = offset_utime + My_utime[n][i] - loc_utime[n][i];
            }
        }
        offset = offset_time*1000000 + offset_utime;

        if (n == 0) {
            min = n;
            min_offset = offset;
        }
        else if (offset < min_offset) {
            min = n;
            min_offset = offset;
        }

        //printf("%i-th client offset: %d\n", n+1, offset);
        offset = 0; 
        offset_time = 0;
        offset_utime = 0;
    }
    //printf("%i-th client has minimum offset: %d\n", min+1, min_offset);

    for (int n = 0; n < NODENUM; n++) {
        if (n != min) {
            for (int i = 0; i < BROADNUM; i++) { // 기준 node와 다른 node 간 offset 측정

                if (loc_time[n][i] != 0 && loc_time[min][i] != 0){ // 손실된 UDP는 무시
                    offset_time = offset_time + loc_time[min][i] - loc_time[n][i];
                    offset_utime = offset_utime + loc_utime[min][i] - loc_utime[n][i];
                    iter++;
                }

            }
            offset_time = offset_time / iter;
            offset_utime = offset_utime / iter;

            strcpy(buf, "Adjust Offset");
            send(nsd[n], buf, strlen(buf)+1, 0);
            send(nsd[n], &offset_time, sizeof(offset_time)+1, 0);
            send(nsd[n], &offset_utime, sizeof(offset_utime)+1, 0);
            printf("RBS synchronization %ius\n", abs(offset_time*1000000)+abs(offset_utime));

            iter = 0; 
            offset_time = 0;
            offset_utime = 0;
        }

        else if (n == min){
            strcpy(buf, "END"); //종료문전달
            send(nsd[n], buf, strlen(buf)+1, 0);
            close(nsd[n]);
        }
    }
    close(sd);
    return 0;
}