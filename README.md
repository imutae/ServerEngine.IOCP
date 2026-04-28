# ServerEngine.IOCP

> Windows IOCP 기반 C++ 게임 서버 엔진 포트폴리오 프로젝트

## 프로젝트 소개

`ServerEngine.IOCP`는 Windows IOCP(I/O Completion Port)를 기반으로 직접 설계·구현한 C++ 게임 서버 엔진입니다.

게임 서버를 만들 때마다 반복되는 소켓 처리, accept/recv/send 처리, 세션 관리, 패킷 파싱 같은 저수준 네트워크 코드를 엔진 영역으로 분리하고, 실제 게임 서버 프로젝트에서는 프로토콜 설계와 게임 로직 구현에 집중할 수 있도록 만드는 것을 목표로 합니다.

이 저장소는 완성형 상용 프레임워크가 아니라, IOCP 기반 서버 구조를 직접 이해하고 구현하면서 네트워크 엔진과 게임 로직을 어떻게 분리할 수 있는지 보여주기 위한 포트폴리오 프로젝트입니다.

---

## 개발 목표

- Windows IOCP 기반 비동기 서버 구조 직접 구현
- 네트워크 엔진과 게임 로직 분리
- 세션 단위 연결 관리 및 패킷 송수신 표준화
- 멀티스레드 환경에서의 세션 생명주기 안정화
- 게임 서버 프로젝트에서 재사용 가능한 기반 코드 작성

---

## 현재 구현 범위

현재 구현된 주요 기능은 다음과 같습니다.

- IOCP core 생성 및 completion dispatch
- `Listener` 기반 listen socket / `AcceptEx` 처리
- `Session` 단위 recv / send 처리
- `PacketHeader` 기반 패킷 파싱
- `RecvBuffer` 기반 수신 버퍼 관리
- `IServerLogic` 기반 게임 로직 주입 구조
- `SessionManager` 기반 세션 생성 및 ID 관리
- `SessionState` 기반 세션 상태 관리
- pending IO count 기반 세션 close finalize
- send queue 기반 순차 송신
- partial send offset 처리
- IOCP worker shutdown wake packet 처리
- `.editorconfig` 기반 UTF-8 / CRLF / C++ 스타일 설정

---

## 전체 구조

```text
Client
  ↓
Listener
  ↓
SessionManager
  ↓
Session
  ├─ RecvBuffer
  ├─ SendQueue
  └─ PacketHeader parsing
  ↓
IServerLogic
  ↓
Game Server Logic
```

엔진 내부 주요 구성요소는 다음과 같습니다.

| Component | Role |
|---|---|
| `ServerEngine` | 엔진 초기화, 실행, 종료 흐름 관리 |
| `ServerContext` | IOCP core, listener, session manager 등 내부 구성요소 보관 |
| `IocpCore` | IOCP handle 생성, object 등록, completion event dispatch |
| `IocpObject` | IOCP에 등록 가능한 객체의 공통 인터페이스 |
| `IocpEvent` | Accept / Recv / Send overlapped event 구조 |
| `Listener` | listen socket 생성 및 `AcceptEx` 기반 accept 처리 |
| `SessionManager` | 세션 생성, session id 발급, 세션 제거 관리 |
| `Session` | 클라이언트 연결, recv/send, disconnect, 생명주기 관리 |
| `RecvBuffer` | 수신 데이터 누적 및 packet 단위 파싱 지원 |
| `PacketHeader` | 공통 packet header 정의 |
| `IServerLogic` | 엔진 사용자 측 게임 로직 인터페이스 |

---

## 핵심 설계 포인트

### 1. 네트워크 엔진과 게임 로직 분리

엔진 사용자는 `IServerLogic` 인터페이스를 구현해 연결, 해제, 패킷 처리 지점에 게임 로직을 연결합니다.

```cpp
class IServerLogic
{
public:
    virtual ~IServerLogic() = default;

    virtual void OnConnected(Net::Session* session) = 0;
    virtual void OnDisconnected(Net::Session* session) = 0;

    virtual void DispatchPacket(
        Net::Session* session,
        uint16_t packetId,
        const char* data,
        int32_t len) = 0;
};
```

이 구조를 통해 엔진은 IOCP 이벤트 처리, 세션 관리, 패킷 송수신을 담당하고, 게임 서버는 패킷 ID별 도메인 로직에 집중하도록 역할을 분리했습니다.

---

### 2. 세션 중심 연결 관리

각 클라이언트 연결은 `Session` 객체로 관리합니다.

`SessionManager`는 세션 생성 시 고유 `sessionId`를 발급하고, 세션 종료가 완료되면 map에서 제거합니다.

이를 통해 이후 게임 서버에서 다음과 같은 기능으로 확장하기 쉬운 구조를 목표로 했습니다.

- 플레이어 식별
- 접속 세션 목록 관리
- 룸/월드 진입 관리
- 브로드캐스트 대상 관리
- 이동/전투/채팅 등 세션 기반 처리

---

### 3. 세션 생명주기 관리

멀티스레드 IOCP 환경에서는 `Disconnect()` 호출 시점과 실제 IO completion 회수 시점이 다를 수 있습니다.

이를 고려해 세션 상태를 단순 `bool`이 아니라 명시적인 상태로 나누었습니다.

```text
Created
  ↓
Connected
  ↓
Closing
  ↓
Closed
```

각 상태의 의미는 다음과 같습니다.

| State | Meaning |
|---|---|
| `Created` | 세션 객체가 생성되었지만 아직 socket과 logic이 연결되지 않은 상태 |
| `Connected` | 정상 송수신이 가능한 상태 |
| `Closing` | disconnect가 시작되어 새 IO는 막지만, 기존 pending IO completion을 회수 중인 상태 |
| `Closed` | pending IO가 모두 정리되어 `SessionManager`에서 제거 가능한 상태 |

`Disconnect()`는 여러 스레드에서 동시에 호출될 수 있으므로, 상태 전이를 통해 중복 disconnect와 중복 `OnDisconnected()` 호출을 방지합니다.

---

### 4. pending IO count 기반 close finalize

IOCP에서는 이미 요청한 overlapped IO가 socket close 이후에도 completion으로 돌아올 수 있습니다.

따라서 세션은 recv/send 요청을 post할 때 pending IO count를 증가시키고, completion 처리 완료 시 감소시킵니다.

```text
WSARecv / WSASend post
  → pendingIoCount++

IOCP completion 처리 완료
  → pendingIoCount--

Closing 상태 && pendingIoCount == 0
  → Closed 상태 전환
  → SessionManager::RemoveSession()
```

이 구조를 통해 pending IO가 남아 있는 상태에서 세션이 먼저 제거되는 문제를 줄이고자 했습니다.

---

### 5. send queue 기반 순차 송신

초기 구현에서는 `Send()` 호출마다 바로 `WSASend()`를 호출하는 방식이었지만, 현재는 세션 단위 send queue를 사용합니다.

```text
Session::Send()
  → packet buffer 생성
  → send queue push
  → 현재 send pending이 없으면 WSASend post

Send completion
  → partial send면 남은 offset부터 다시 WSASend
  → 완료되면 queue pop
  → 다음 packet이 있으면 다음 WSASend post
```

이 구조를 통해 다음 문제를 개선했습니다.

- 여러 스레드에서 동시에 `Send()` 호출 시 송신 순서가 불명확해지는 문제
- 하나의 세션에서 여러 `WSASend()`가 동시에 난사되는 문제
- partial send 발생 시 남은 데이터를 다시 보내지 못하는 문제
- disconnect 중 새 send가 들어오는 문제

---

## Threading Model

이 엔진은 IOCP worker thread들이 accept / recv / send completion을 처리합니다.

중요한 threading contract는 다음과 같습니다.

- `IServerLogic::OnConnected()`는 accept completion 처리 흐름에서 호출될 수 있습니다.
- `IServerLogic::OnDisconnected()`는 disconnect를 발생시킨 worker thread에서 호출될 수 있습니다.
- `IServerLogic::DispatchPacket()`은 recv completion을 처리한 IOCP worker thread에서 직접 호출됩니다.
- 여러 세션의 callback은 서로 다른 worker thread에서 동시에 호출될 수 있습니다.
- 게임 로직 내부의 공유 상태는 엔진이 자동으로 보호하지 않으므로, 사용자 로직에서 직접 동기화해야 합니다.
- `DispatchPacket()`으로 전달되는 `data` pointer는 callback 실행 중에만 유효합니다.
- callback 이후에도 packet data를 보관해야 한다면 사용자 로직에서 직접 복사해야 합니다.

예를 들어, room 목록, player 목록, world state 같은 공유 객체를 여러 callback에서 접근한다면 별도의 lock, job queue, actor/strand 구조 등을 사용해야 합니다.

---

## Packet Format

모든 패킷은 고정 크기의 `PacketHeader`를 앞에 붙이는 구조입니다.

```cpp
#pragma pack(push, 1)

struct PacketHeader
{
    uint16_t size;
    uint16_t id;
};

#pragma pack(pop)
```

필드 의미는 다음과 같습니다.

| Field | Meaning |
|---|---|
| `size` | `PacketHeader`를 포함한 전체 packet size |
| `id` | packet type id |

현재 패킷 처리 정책은 다음과 같습니다.

- `size`는 header를 포함한 전체 packet 크기입니다.
- `uint16_t size`를 사용하므로 최대 packet size는 65535 bytes입니다.
- 수신 시 `size < sizeof(PacketHeader)`인 packet은 비정상 packet으로 보고 disconnect합니다.
- 현재 `Send()`는 body 길이가 0인 packet을 허용하지 않습니다.
- multi-platform 통신까지 고려할 경우 endian 정책을 명시적으로 정리할 필요가 있습니다.

---

## 사용 예시

아래는 엔진을 사용하는 서버 프로젝트의 기본 형태입니다.

```cpp
#include <iostream>

#include "ServerEngine.h"
#include "IServerLogic.h"
#include "Session.h"

class GameLogic : public SE::IServerLogic
{
public:
    void OnConnected(SE::Net::Session* session) override
    {
        std::cout << "Client connected. sessionId="
                  << session->GetSessionId()
                  << std::endl;
    }

    void OnDisconnected(SE::Net::Session* session) override
    {
        std::cout << "Client disconnected. sessionId="
                  << session->GetSessionId()
                  << std::endl;
    }

    void DispatchPacket(
        SE::Net::Session* session,
        uint16_t packetId,
        const char* data,
        int32_t len) override
    {
        // packetId 기반으로 게임 서버 로직 분기
        // data pointer는 이 callback 안에서만 유효하므로,
        // 나중에 사용할 데이터는 복사해야 합니다.
    }
};

int main()
{
    GameLogic logic;
    SE::ServerEngine engine;

    if (!engine.Initialize(&logic, "0.0.0.0", 7777))
    {
        std::cout << "Server initialize failed." << std::endl;
        return 1;
    }

    engine.Run();

    std::cout << "Server running. Press Enter to stop." << std::endl;
    std::cin.get();

    engine.Shutdown();
    return 0;
}
```

`Run()`은 worker thread를 시작한 뒤 반환하는 구조입니다. 따라서 demo server나 실제 서버 실행 코드에서는 main thread가 바로 종료되지 않도록 별도의 대기 흐름이 필요합니다.

---

## 시연용 실행 흐름

포트폴리오 시연에서는 아래 흐름을 기준으로 검증하는 것을 권장합니다.

```text
1. 서버 실행
2. 클라이언트 접속
3. OnConnected 로그 확인
4. 클라이언트가 packet 전송
5. DispatchPacket 로그 확인
6. 서버가 response packet 전송
7. 클라이언트 정상 종료 또는 강제 종료
8. OnDisconnected 로그 확인
9. 세션 제거 흐름 확인
10. 서버 종료
```

현재 구조는 단순 시연과 제한적인 기능 검증에는 적합하지만, 대규모 부하 테스트나 장시간 운영을 목표로 하는 완성형 서버는 아닙니다.

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
├─ pch.h / pch.cpp
├─ .editorconfig
├─ ServerEngine.sln
└─ ServerEngine.vcxproj
```

---

## 개발 환경

- Language: C++
- Platform: Windows
- Network Model: IOCP
- IDE: Visual Studio 2022
- Platform Toolset: v143
- Windows SDK: 10.0
- Output: Static Library
- Recommended Platform: x64
- Recommended C++ Standard: C++20
- Encoding: UTF-8

---

## 현재 한계 및 개선 예정

현재 구현은 포트폴리오 및 학습 목적의 서버 엔진이며, 아래 항목은 추가 개선 예정입니다.

- 전체 세션 graceful shutdown 흐름 보강
- 서버 종료 시 모든 세션에 대한 명시적 disconnect / drain 처리
- Listener shutdown 중 pending `AcceptEx` 생명주기 정리
- send queue 최대 크기 제한 및 backpressure 정책 추가
- body 없는 packet 허용 여부 정책 정리
- packet endian 정책 정리
- 게임 로직 callback을 별도 logic thread / job queue로 넘기는 구조 검토
- 단위 테스트 및 부하 테스트 보강
- 실제 게임 서버 프로젝트 적용 후 반복 개선

---

## 관련 프로젝트

- [DeathRun-Server](https://github.com/imutae/DeathRun-Server)
  - 이 엔진을 사용해 작성 중인 C++ 멀티플레이 서버 프로토타입입니다.
- [DeathRun-Client](https://github.com/imutae/DeathRun-Client)
  - Unity 기반 멀티플레이 클라이언트 프로젝트입니다.

---

## 이 프로젝트에서 강조하고 싶은 점

이 프로젝트는 단순히 “서버가 실행된다”보다 다음 역량을 보여주기 위해 만들었습니다.

- Windows IOCP 기반 비동기 서버 구조를 직접 구현한 경험
- accept / recv / send completion 흐름을 직접 설계한 경험
- 네트워크 엔진과 게임 로직을 분리하려는 설계 관점
- 세션 생명주기와 pending IO 문제를 고려한 구조 개선 경험
- 멀티스레드 환경에서 발생할 수 있는 race condition을 인지하고 개선한 경험
- 이후 게임 서버 프로젝트에서 재사용 가능한 기반을 만들려는 시도

---

## 회고

이 프로젝트를 진행하면서 게임 서버 개발에서 중요한 것은 단순히 packet을 주고받는 코드가 아니라, 세션 생명주기, 비동기 IO completion, 멀티스레드 동시성, packet protocol, 게임 로직 분리 같은 요소를 함께 설계하는 것임을 체감했습니다.

아직 완성형 서버 엔진은 아니지만, low-level 네트워크 구조를 직접 구현하고 안정성을 개선해가며 게임 서버 개발자로서의 기반을 다지고 있는 프로젝트입니다.

---

## License

MIT License