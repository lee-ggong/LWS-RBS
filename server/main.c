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

#define BDPORTNUM 9005
#define TPPORTNUM 9000
#define BROADNUM 10
#define NODENUM 2

int main(void) {
    char buf[256], ipad[NODENUM][INET_ADDRSTRLEN];
    struct sockaddr_in brd, sin, cli;
    float beacon_interval = 0;
    struct timeval tv;
    int min_offset = 0, offset = 0, optval = 1;
    int bd, sd, nsd[NODENUM], min, iter = 0, clen = sizeof(cli);
    int32_t offset_time = 0, offset_utime = 0, loc_time[NODENUM][BROADNUM], loc_utime[NODENUM][BROADNUM], timelen[NODENUM], My_time[NODENUM][BROADNUM], My_utime[NODENUM][BROADNUM];

    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1){ //TCP socket을 생성(연결 성공시 0을 리턴 실패시 -1을 리턴)
        perror("TCP socket");
        exit(1); 
    }

    if ((setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval))) == -1){ //사용중인 포트 재사용 옵션
        perror("setsockopt");
        exit(1);
    }

    memset((char *)&sin, '\0', sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(TPPORTNUM); 
    sin.sin_addr.s_addr = inet_addr("192.168.0.159"); //소켓 주소 구조체에 서버의 주소를 지정

    if (bind(sd, (struct sockaddr *)&sin, sizeof(sin))) { 
        perror("bind");
        exit(1);
    }

    if (listen(sd, NODENUM)) {
        perror("listen");
        exit(1);
    }

    // IP 수신 phase
    for (int n = 0; n < NODENUM; n++) {

        if ((nsd[n] = accept(sd, (struct sockaddr *)&cli, &clen)) == -1) { //accept 함수로 클라이언트의 접속 요청을 수락하고 새로운 소켓 기술자를 생성해 nsd에 저장(sd, &cli-client의 IP, &clen-cli의 크기)
            perror("accept");
            exit(1);
        }

        inet_ntop(AF_INET, &(cli.sin_addr), ipad[n], INET_ADDRSTRLEN);

        switch (fork()) {            
            case 0:
                close(sd);

                if ((send(nsd[n], ipad[n], strlen(ipad[n]) + 1, 0)) == -1) {
                    perror("send");
                    exit(1);
                }

                exit(0);
        }
    }

    // Broadcasting phase
    for (int n = 0; n < NODENUM; n++) {

        if ((bd = socket(AF_INET, SOCK_DGRAM, 0)) == -1){ //UDP socket을 생성(연결 성공시 0을 리턴 실패시 -1을 리턴)
            perror("UDP socket"); //에러메시지를 출력하는 함수
            exit(1); //1을 반환하면서 프로그램 종료
        }

        memset((char *)&brd, '\0', sizeof(brd)); //socket 구조체에 값을 지정(&ser-메모리의 시작 주소, \0-메모리에 채우고자 하는 값, size-채우고자하는 메모리의 크기)
        brd.sin_family = AF_INET; //socket family를 AF_INET으로 지정
        brd.sin_port = htons(BDPORTNUM); 
        brd.sin_addr.s_addr = inet_addr(ipad[n]); //소켓 주소 구조체에 서버의 주소를 지정

        switch (vfork()) {            
            case 0:
                for (int k = 1; k <= BROADNUM; k++) {
                    sprintf(buf, "%d", k);

                    if ((sendto(bd, buf, strlen(buf)+1, 0, (struct sockaddr *)&brd, sizeof(brd))) == -1) { //클라이언트에게 sendto 함수로 msg를 전송(bd, buf-전송할 msg를 저장한 메모리 주소, msg의 크기, 0-데이터를 주고받는 방법을 지정한 플래그, &cli-msg를 전송할 호스트의 주소, &clientlen-&cli의 크기)
                        perror("sendto");
                        exit(1);         
                    }

                    gettimeofday(&tv, NULL);
        
                    My_time[n][k] = tv.tv_sec;
                    My_utime[n][k] = tv.tv_usec;

                    //printf("%i-th stamp: %d.%d\n", k, My_time[n][k], My_utime[n][k]);

                    sleep(beacon_interval);
                }

                strcpy(buf, "END"); //종료문전달 
                for (int k = 1; k <= 1; k++) {
                    if ((sendto(bd, buf, strlen(buf)+1, 0, (struct sockaddr *)&brd, sizeof(brd))) == -1) { //클라이언트에게 sendto 함수로 msg를 전송(bd, buf-전송할 msg를 저장한 메모리 주소, msg의 크기, 0-데이터를 주고받는 방법을 지정한 플래그, &cli-msg를 전송할 호스트의 주소, &clientlen-&cli의 크기)
                        perror("sendto");
                        exit(1);         
                    }
                    sleep(0);
                }
                exit(0);
        }
    }

    /*for (int n = 0; n < NODENUM; n++) {
        for (int i = 1; i <= BROADNUM; i++) {
            printf("%i-th stamp: %d.%d\n", i, My_time[n][i], My_utime[n][i]);
        }
    }*/

    // Time stamp phase
    for (int n = 0; n < NODENUM; n++) {
        close(sd);
                
        if (recv(nsd[n], &timelen[n], sizeof(timelen), 0) == -1) {
            perror("recv");
            exit(1);
        }
        if (recv(nsd[n], loc_time[n], sizeof(loc_time[n])+1, 0) == -1) {
            perror("recv");
            exit(1);
        }
        if (recv(nsd[n], loc_utime[n], sizeof(loc_utime[n])+1, 0) == -1) {
            perror("recv");
            exit(1);
        }
        for (int i = 0; i < timelen[n]; i++) {
            printf("%i-th client %i-th stamp: %d.%d\n", n+1, i+1, loc_time[n][i], loc_utime[n][i]);
        }
    }

    // Offset phase
    for (int n = 0; n < NODENUM; n++) {
        for (int i = 0; i < min; i++) { // 기준 node 설정을 위한 기지국과 단말간 offset 계산

            if (loc_time[n][i] != 0){ // 손실된 UDP는 무시
                offset_time = offset_time + My_time[n][i] - loc_time[n][i];
                offset_utime = offset_utime + My_utime[n][i] - loc_utime[n][i];

                offset = offset_time*1000000 + offset_utime;
            }

        }

        if (n == 0) {
            min = n;
            min_offset = offset;
        }
        else if (offset < min_offset) {
            min = n;
            min_offset = offset;
        }

        offset = 0; 
        offset_time = 0;
        offset_utime = 0;
    }

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
            send(nsd[n], &offset_time, sizeof(offset_time), 0);
            send(nsd[n], &offset_utime, sizeof(offset_utime), 0);
            printf("RBS synchronization %i%ius\n", abs(offset_time*1000000), abs(offset_utime));

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
