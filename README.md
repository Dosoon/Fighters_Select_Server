# ⚙ Fighters

**이동 및 3패턴 공격이 가능한 간단한 PVP 게임 서버**

**1인 프로젝트**로, 테스트용 클라이언트 프로그램은 학원 측에서 제공받았습니다.

---

## 시연 영상

[![Video Label](http://img.youtube.com/vi/7yijMzRkLTw/0.jpg)](https://youtu.be/7yijMzRkLTw)

---

# :computer: 구현 내용

- **Select 모델 기반** 싱글스레드 소켓 서버
  - 최대 동시 접속 인원 5,000명 수용 가능
  - 패킷 디스패쳐 및 프로시져 작성
- Windows Socket API 사용
- **RingBuffer**를 사용한 **I/O 최적화**
- **패킷 직렬화/역직렬화 클래스** 구현
- **Sector 기반** 시야 처리 및 **패킷량 조절**
- **HeartBeat 시스템**을 통한 **비정상 클라이언트 연결 관리**
- 에러 발생 시 Dump 파일 생성 기능

---

# :seedling: 기술 스택

- C++
