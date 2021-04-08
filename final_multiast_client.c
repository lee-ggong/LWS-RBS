#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

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

    // 멀티캐스트 UDP socket을 생성
    if ((mt = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1){
        perror("socket");
        exit(1);
    }

    // 멀티캐스트 UDP sokcet 구조체 생성
    memset((char *)&mult, '\0', sizeof(mult));
    mult.sin_family = AF_INET;
    mult.sin_port = htons(MULPORTNUM); 
    mult.sin_addr.s_addr = htonl(INADDR_ANY);

    // UDP socket bind
    if (bind(mt, (struct sockaddr *)&mult, sizeof(mult))) {
        perror("bind");
        exit(1);
    }

    /* 멀티캐스트 그룹 가입을 위한 구조체
    여러대역이 사용 가능하지만 예약된 주소가 많아 주소 선정에 주의가 필요
    IANA 주소 대역 참고 - Internet Control Block(224.0.1.0-224.0.1.255)*/
    joinAddr.imr_multiaddr.s_addr = inet_addr("224.0.1.222");
    joinAddr.imr_interface.s_addr = htonl(INADDR_ANY);  

    // 멀티캐스트 그룹 주소를 구독
    if ((setsockopt(mt, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&joinAddr, sizeof(joinAddr))) == -1){ 
        perror("setsockopt"); 
        exit(1); 
    }

    while (1) {

        // Time stamp 변수 초기화
        timelen = 0;
        memset((int32_t *)&loc_time, '\0', sizeof(loc_time));
        memset((int32_t *)&loc_utime, '\0', sizeof(loc_utime));

        // Broadcasting 단계
        while (1) {
            // 멀티캐스트 UDP 패킷 수신
            n = recvfrom(mt, buf, 255, 0, (struct sockaddr *)&mult, &multlen);
            index = atoi(buf); // UDP 내 payload = 패킷 num

            /* Broadcasting 종료 조건문
            1. 약속된 패킷 수 달성 시 
            or 
            2. 종료문 수신 시*/
            if (strcmp(endst, buf)==0 || timelen == BROADNUM)
                break;  
        
            // UDP 패킷을 받는 순간 time stamping
            if ((ioctl(mt, SIOCGSTAMP, &tv_cli)) == -1){ 
                perror("ioctl"); 
                exit(1); 
            }
        
            // UDP 정상 수신시 time stamping
            if (index == timelen+1){ 
                loc_time[index-1] = tv_cli.tv_sec;
                loc_utime[index-1] = tv_cli.tv_usec;
            }
            // UDP 손실시 0으로 초기화
            else{ 
                loc_time[timelen] = 0;
                loc_utime[timelen] = 0;
                loc_time[index-1] = tv_cli.tv_sec;
                loc_utime[index-1] = tv_cli.tv_usec;
                timelen = index-1;
            }

            // 서버로 부터 받은 패킷의 num과 수신 받을 때 시간 출력
            printf("** From Server : %i packet\n", index);
            printf("** Local clock : %i.%i\n", loc_time[timelen], loc_utime[timelen]);

            timelen++;
        }

        // TCP socket을 생성 
        if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1){ 
            perror("socket");
            exit(1);
        }

        // TCP socket 구조체를 생성
        memset((char *)&sin, '\0', sizeof(sin));
        sin.sin_family = AF_INET;
        sin.sin_port = htons(TPPORTNUM); 
        sin.sin_addr.s_addr = inet_addr("192.168.0.159"); // 서버 주소 변경 시 수정!!!!!

        // 서버에 TCP 연결 요청
        if (connect(sd, (struct sockaddr *)&sin, sizeof(sin))) {
            perror("connect");
            exit(1);
        }

        // 측정한 time stamp 값 전송
        if ((send(sd, &loc_time[0], sizeof(loc_time)+1, 0)) == -1) {
            perror("send");
            exit(1);
        }
        if ((send(sd, &loc_utime[0], sizeof(loc_utime)+1, 0)) == -1) {
            perror("send");
            exit(1);
        }

        if (recv(sd, buf, sizeof(buf) + 1, 0) == -1) {
            perror("recv");
            exit(1);
        }

        if (strcmp(endst, buf) != 0){
            printf("** %s\n", buf); // 서버로 부터 받은 'Adjust Offset' 출력
            // 서버로 부터 기준 node와 시간 offset 수신
            if (recv(sd, &offset_time, sizeof(offset_time)+1, 0) == -1) {
            perror("recv");
            exit(1);
            }
            if (recv(sd, &offset_utime, sizeof(offset_utime)+1, 0) == -1) {
            perror("recv");
            exit(1);
            }
            printf("** offset : %i.%i\n", offset_time, offset_utime); // 서버로 부터 받은 offset 출력

            // 현재 시간
            gettimeofday(&current, 0);
            printf("** current time : %i.%i\n", current.tv_sec, current.tv_usec);

            // 수정한 시간 = 현재 시간 + offset
            current.tv_sec = current.tv_sec + offset_time;
            current.tv_usec = current.tv_usec + offset_utime;
            printf("** change time : %i.%i\n", current.tv_sec, current.tv_usec);

            // 수정한 시간을 시스템 시간으로 등록 (반드시 관리자 권한 필요!!)
            settimeofday(&current, 0);

            // 잘 수정되었는지 확인
            gettimeofday(&current, 0);
            printf("** current time : %i.%i\n", current.tv_sec, current.tv_usec);
        }
        close(sd);
    }
    return 0;
}