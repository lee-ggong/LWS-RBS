#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MULPORTNUM 9005 // 브로드캐스트 UDP port
#define TPPORTNUM 9000  // TCP port
#define BROADNUM 10     // 브로드캐스트 할 패킷의 수
#define NODENUM 2       // 연결할 드론의 수

int main(void) {
    char buf[256], ipad[NODENUM][INET_ADDRSTRLEN];
    struct sockaddr_in mult, sin, cli;
    struct in_addr localaddr;
    struct timeval tv;
    float beacon_interval = 0;
    int min_offset = 0, offset = 0, optval = 1;
    int mt, sd, nsd[NODENUM], min, iter = 0, clen = sizeof(cli), multiTTL = 64;
    int32_t offset_time = 0, offset_utime = 0, loc_time[NODENUM][BROADNUM], loc_utime[NODENUM][BROADNUM], My_time[BROADNUM], My_utime[BROADNUM];

    // 멀티캐스트 UDP socket을 생성
    if ((mt = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1){ 
        perror("socket");
        exit(1);
    }
    // TCP socket을 생성
    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1){ 
        perror("socket");
        exit(1);
    }

    /* IGMP의 TTL 값 설정 
    0: 호스트 내부  1: 동일 서브넷  32: 동일 사이트
    64: 동일 지역   256: 무제한*/
    if ((setsockopt(mt , IPPROTO_IP, IP_MULTICAST_TTL, (char*)&multiTTL, sizeof(multiTTL))) == -1){
        perror("setsockopt");
        exit(1);
    }

    // TCP 소켓의 주소 재사용 옵션
    if ((setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval))) == -1){ 
        perror("setsockopt");
        exit(1);
    }

    /* 멀티캐스트 UDP socket 구조체를 생성 
    s_addr: 멀티캐스트 그룹 주소
    여러대역이 사용 가능하지만 예약된 주소가 많아 주소 선정에 주의가 필요
    IANA 주소 대역 참고 - Internet Control Block(224.0.1.0-224.0.1.255)*/
    memset((char *)&mult, '\0', sizeof(mult));
    mult.sin_family = AF_INET;
    mult.sin_port = htons(MULPORTNUM); 
    mult.sin_addr.s_addr = inet_addr("224.0.1.222");

    // 멀티캐스트 UDP socket에서 사용할 인터페이스를 선택(서버의 주소값)
    localaddr.s_addr = htonl(INADDR_ANY);;    
    if ((setsockopt(mt , IPPROTO_IP, IP_MULTICAST_IF, (char*)&localaddr, sizeof(localaddr))) == -1){
        perror("setsockopt");
        exit(1);
    }

    // TCP socket 구조체를 생성
    memset((char *)&sin, '\0', sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(TPPORTNUM); 
    sin.sin_addr.s_addr = htonl(INADDR_ANY);

    // TCP socket bind
    if (bind(sd, (struct sockaddr *)&sin, sizeof(sin))) { 
        perror("bind");
        exit(1);
    }

    // Broadcasting 단계
    for (int n = 1; n <= BROADNUM; n++) {
        sprintf(buf, "%d", n); // 브로드캐스트 패킷 내 payload = 패킷 num

        if ((sendto(mt, buf, strlen(buf)+1, 0, (struct sockaddr *)&mult, sizeof(mult))) == -1) {
            perror("sendto");
            exit(1);         
        }

        // UDP 패킷을 전송하는 서버 시간을 기록
        gettimeofday(&tv, NULL);
        My_time[n] = tv.tv_sec;
        My_utime[n] = tv.tv_usec;

        sleep(beacon_interval);
    }

    strcpy(buf, "END"); // 종료문전달  
    if ((sendto(mt, buf, strlen(buf)+1, 0, (struct sockaddr *)&mult, sizeof(mult))) == -1) {
        perror("sendto");
        exit(1);         
    }
    
    // Time stamp 수신 단계
    if (listen(sd, NODENUM)) { // cli로 부터 time값을 수신받기위해 대기
        perror("listen");
        exit(1);
    }

    // 연결을 먼저 요청한 cli 부터 time stamp 값을 수신
    for (int n = 0; n < NODENUM; n++) { 
        if ((nsd[n] = accept(sd, (struct sockaddr *)&cli, &clen)) == -1) {
            perror("accept");
            exit(1);
        }

        inet_ntop(AF_INET, &(cli.sin_addr), ipad[n], INET_ADDRSTRLEN); // ipad[n]에 현재 연결된 cli의 주소 저장

        if (recv(nsd[n], &loc_time[n][0], sizeof(loc_time[n])+1, 0) == -1) {
            perror("recv");
            exit(1);
        } // 초단위 time stamp 수신
        
        if (recv(nsd[n], &loc_utime[n][0], sizeof(loc_utime[n])+1, 0) == -1) {
            perror("recv");
            exit(1);
        } // 마이크로 초단위 time stamp 수신
    }

    // Offset 계산 단계
    for (int n = 0; n < NODENUM; n++) {

        // 기준 node 설정을 위한 기지국과 단말간 offset 계산
        for (int i = 1; i < BROADNUM; i++) { 

            if (loc_time[n][i] != 0){ // 손실된 UDP는 무시
                offset_time = offset_time + My_time[i] - loc_time[n][i];
                offset_utime = offset_utime + My_utime[i] - loc_utime[n][i];
            }
        }
        offset = offset_time*1000000 + offset_utime;

        // 서버와 시간차이가 가장 적은 cli를 기준 ndoe로 설정
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

    
    // 기준 node와 다른 node 간 offset 측정 후 각 node에 offset 전송
    for (int n = 0; n < NODENUM; n++) {
        if (n != min) {
            for (int i = 1; i < BROADNUM; i++) { 

                if (loc_time[n][i] != 0 && loc_time[min][i] != 0){ // 손실된 UDP는 무시
                    offset_time = offset_time + loc_time[min][i] - loc_time[n][i];
                    offset_utime = offset_utime + loc_utime[min][i] - loc_utime[n][i];
                    iter++;
                }

            }
            offset_time = offset_time / iter;
            offset_utime = offset_utime / iter;

            strcpy(buf, "Adjust Offset");
            if ((send(nsd[n], buf, strlen(buf)+1, 0)) == -1) {
            perror("send");
            exit(1);
            }
            if ((send(nsd[n], &offset_time, sizeof(offset_time)+1, 0)) == -1) {
            perror("send");
            exit(1);
            }
            if ((send(nsd[n], &offset_utime, sizeof(offset_utime)+1, 0)) == -1) {
            perror("send");
            exit(1);
            }
            printf("RBS synchronization %ius\n", abs(offset_time*1000000)+abs(offset_utime));

            iter = 0; 
            offset_time = 0;
            offset_utime = 0;
        }

        // 종료문전달
        else if (n == min){
            strcpy(buf, "END"); 
            send(nsd[n], buf, strlen(buf)+1, 0);
            close(nsd[n]);
        }
    }
    close(sd);
    return 0;
}