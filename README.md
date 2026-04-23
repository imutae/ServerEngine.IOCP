# ServerEngine.IOCP

> Windows IOCP 기반 C++ 게임 서버 엔진 포트폴리오 프로젝트 (개발 중)

## 프로젝트 소개

`ServerEngine.IOCP`는 **게임 서버 로직과 네트워크 엔진을 분리**하기 위해 직접 설계·구현 중인 C++ IOCP 서버 엔진입니다.

목표는 매 프로젝트마다 소켓 처리, 세션 관리, 패킷 파싱 같은 저수준 네트워크 코드를 반복 작성하는 대신,
**프로토콜 설계와 게임 로직 구현에 집중할 수 있는 서버 엔진 구조**를 만드는 것입니다.

이 저장소는 완성형 프레임워크를 보여주기보다,
제가 **IOCP 기반 서버 구조를 어떻게 이해하고, 어떤 방식으로 추상화하려고 했는지**를 보여주기 위한 포트폴리오입니다.

---

## 개발 목표

- Windows IOCP 기반 비동기 서버 구조 직접 구현
- 네트워크 엔진과 게임 로직 분리
- 세션 단위 연결 관리 및 패킷 송수신 표준화
- 게임 서버 프로젝트에서 재사용 가능한 기반 코드 작성

---

## 핵심 설계 포인트

### 1. 게임 로직 분리
엔진 사용자는 `IServerLogic` 인터페이스만 구현하면,
세션 연결/해제와 패킷 디스패치 지점에서 게임 로직을 연결할 수 있도록 설계했습니다.

```cpp
class IServerLogic
{
public:
    virtual ~IServerLogic() = default;
    virtual void OnConnected(Net::Session* session) = 0;
    virtual void OnDisconnected(Net::Session* session) = 0;
    virtual void DispatchPacket(Net::Session* session, uint16_t packetId, const char* data, int32_t len) = 0;
};
```

이 구조를 통해 엔진은 **연결 처리, IOCP 이벤트 처리, 세션 관리**를 담당하고,
게임 서버는 **패킷 규약과 도메인 로직**에 집중하도록 역할을 분리했습니다.

### 2. 세션 중심 구조
각 클라이언트 연결은 `Session`으로 관리하며,
세션마다 고유 `sessionId`를 부여하는 구조를 사용했습니다.

이를 통해 이후 게임 서버에서 다음과 같은 기능으로 확장하기 쉽도록 방향을 잡았습니다.

- 플레이어 식별
- 룸/월드 진입 관리
- 브로드캐스트 대상 관리
- 이동/전투/채팅 등의 세션 기반 처리

### 3. 고정 패킷 헤더
모든 패킷은 아래와 같은 고정 헤더를 기준으로 처리합니다.

```cpp
struct PacketHeader
{
    uint16_t size;
    uint16_t id;
};
```

패킷 헤더를 공통화해
서버 로직에서는 `packetId` 기준으로 명확하게 분기하고,
클라이언트와의 프로토콜 규약도 일관되게 유지할 수 있도록 했습니다.

### 4. 엔진 컴포넌트 분리
네트워크 엔진을 아래와 같이 역할별로 나누어 구성했습니다.

- `IocpCore` : IOCP 핸들 생성 및 completion 이벤트 디스패치
- `Listener` : listen socket 생성 및 accept 처리
- `Session` : 개별 클라이언트 연결, 송수신 처리
- `SessionManager` : 세션 생성 및 ID 관리
- `RecvBuffer` : 수신 버퍼 관리
- `ServerEngine` : 엔진 초기화 및 실행 흐름 관리

---

## 현재 구현 범위

### 구현된 내용
- IOCP 코어 등록 및 dispatch 구조 작성
- listener / session / session manager 계층 분리
- 공통 패킷 헤더 구조 정의
- recv buffer 추상화
- `IServerLogic` 기반 게임 로직 주입 구조 작성
- `ServerContext`를 통한 엔진 내부 구성요소 관리

### 진행 중인 내용
- 워커 스레드 실행 루프 보완
- 종료 흐름 정리
- disconnect 이후 세션 정리 강화
- send queue / thread-safety 보강
- 예외 상황 및 안정성 검증

---

## 디렉터리 구성

```text
ServerEngine.IOCP/
├─ IServerLogic.h
├─ IocpCore.h / IocpCore.cpp
├─ IocpEvent.h
├─ IocpObject.h
├─ Listener.h / Listener.cpp
├─ Session.h / Session.cpp
├─ SessionManager.h / SessionManager.cpp
├─ RecvBuffer.h / RecvBuffer.cpp
├─ PacketHeader.h
├─ ServerContext.h
├─ ServerEngine.h / ServerEngine.cpp
├─ ServerEngine.sln
└─ ServerEngine.vcxproj
```

---

## 사용 예시

이 엔진은 아래와 같은 흐름으로 사용하는 것을 목표로 설계했습니다.

1. 게임 서버 프로젝트에서 `IServerLogic` 상속
2. 패킷 ID / 패킷 구조체 정의
3. `DispatchPacket()`에서 패킷 분기 처리
4. `OnConnected()`, `OnDisconnected()`에서 세션 상태 관리
5. `ServerEngine`에 로직 객체를 주입하여 실행

즉,
**엔진은 연결/패킷 처리 기반을 제공하고,
게임 서버는 도메인 로직만 구현하는 구조**를 지향합니다.

---

## 개발 환경

- Language: C++
- Platform: Windows
- Network Model: IOCP (I/O Completion Port)
- Build: Visual Studio Solution (`ServerEngine.sln`)

---

## 이 프로젝트에서 강조하고 싶은 점

이 프로젝트는 단순히 “서버가 돌아간다”보다,
다음 역량을 보여주기 위해 만들었습니다.

- IOCP 기반 비동기 서버 구조를 직접 이해하고 설계한 경험
- low-level 네트워크 처리와 game logic을 분리하려는 설계 관점
- 세션, 패킷, 이벤트 흐름을 직접 구성한 경험
- 이후 게임 서버 프로젝트에서 재사용 가능한 기반을 만들려는 시도

---

## 한계 및 개선 예정

현재는 엔진 전체가 완성된 상태는 아니며,
포트폴리오 관점에서 **구조 설계와 구현 방향성**을 먼저 정리한 단계입니다.

앞으로는 아래 항목을 보완할 예정입니다.

- IOCP worker thread loop 완성
- 패킷 송수신 안정성 강화
- 세션 생명주기 정리
- 멀티스레드 환경에서의 안전성 검증
- 실제 게임 서버 프로젝트 적용을 통한 반복 개선

---

## 관련 프로젝트

- [DeathRun-Server](https://github.com/imutae/DeathRun-Server)
  - 이 엔진을 사용해 작성 중인 게임 서버 프로젝트
- [DeathRun-Client](https://github.com/imutae/DeathRun-Client)
  - Unity 기반 멀티플레이 클라이언트 프로젝트

---

## 회고

이 프로젝트를 통해
“게임 서버 개발자는 단순히 패킷을 처리하는 사람이 아니라,
**동시성·세션·프로토콜·상태 흐름을 구조적으로 설계하는 개발자**”라는 점을 더 명확하게 이해하게 되었습니다.

완성도보다도,
직접 low-level 네트워크 구조를 구현해 보면서
게임 서버 개발자로서의 기반을 다지고 있는 프로젝트입니다.
