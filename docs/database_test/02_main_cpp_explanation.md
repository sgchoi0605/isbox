# main.cpp 상세 설명

대상 파일:

```text
drogon-db-test2/src/main.cpp
```

`main.cpp`는 서버의 진입점이다. 이 파일의 핵심 책임은 다음 4가지다.

1. Windows 로컬 환경에서 MariaDB Connector가 필요한 DLL을 찾을 수 있게 준비한다. // DLL : 동적으로 연결되는 라이브러리
2. `config.json`을 읽어 HTTP listener와 DB client를 설정한다. 
3. `/db-test` GET 라우터를 등록한다. // get 라우터 : 웹에서 GET 요청이 들어왔을 때 어떤 코드를 실행할지 정해주는 길 안내 규칙.  라우터는 URL 경로와 실행할 코드를 연결해주는 장치
4. 요청 성공/실패를 JSON 응답으로 반환한다.

## include 구간

```cpp
#include <drogon/drogon.h>
```

Drogon의 핵심 헤더다. `drogon::app()`, `HttpRequestPtr`, `HttpResponsePtr`, `HttpStatusCode`, 라우터 등록, JSON 응답 생성 기능을 사용한다.

```cpp
#include <cstdlib>
```

Windows에서 `_putenv_s()`를 사용하기 위해 포함한다. 이 예제에서는 MariaDB Connector 관련 환경 변수를 설정한다.

```cpp
#include <filesystem>
```

현재 실행 경로를 기준으로 MariaDB 인증 플러그인 DLL 경로를 찾기 위해 사용한다.

```cpp
#include <functional>
```

Drogon handler의 응답 callback 타입이 `std::function<void(const drogon::HttpResponsePtr &)>`이므로 필요하다.

```cpp
#include <string>
```

예외 메시지, 파일 경로, JSON 문자열 값을 다루기 위해 사용한다.

```cpp
#include <vector>
```

MariaDB 플러그인 디렉터리 후보 경로들을 `std::vector`에 담기 위해 사용한다.

```cpp
#include "repositories/HealthCheckRepository.h"
```

`/db-test` 요청에서 실제 DB 작업을 맡길 Repository를 가져온다. 이 include가 있기 때문에 `main.cpp`는 SQL을 직접 작성하지 않고 Repository 객체만 호출한다.
==Repository는 우리 프로젝트에서 Mapper로 쓰일거다.==

## 익명 namespace

```cpp
namespace
{
  ...
}
```

익명 namespace 안에 있는 함수들은 현재 `.cpp` 파일 안에서만 보인다. 즉, `makeJsonResponse`, `makeErrorBody`, `configureMariaDbRuntimeForWindows`는 `main.cpp` 전용 helper 함수다.



이렇게 두면 다른 파일에서 실수로 같은 이름의 함수를 가져다 쓰거나 충돌하는 일을 줄일 수 있다.

## makeJsonResponse()

```cpp
drogon::HttpResponsePtr makeJsonResponse(
    const Json::Value &body,
    drogon::HttpStatusCode status = drogon::k200OK)
```

JSON 본문과 HTTP 상태 코드를 받아 Drogon 응답 객체를 만들어주는 함수다.

```cpp
auto response = drogon::HttpResponse::newHttpJsonResponse(body);
```

`Json::Value`를 JSON HTTP response로 바꾼다. Drogon이 `Content-Type: application/json`에 맞는 응답을 만들 수 있게 해준다.

```cpp
response->setStatusCode(status);
```

HTTP 상태 코드를 설정한다. 기본값은 `200 OK`이고, 오류 응답에서는 `500 Internal Server Error`를 넘긴다.

```cpp
return response;
```

Drogon handler의 callback에 넘길 응답 포인터를 반환한다.

## makeErrorBody()

```cpp
Json::Value makeErrorBody(const std::string &message, const std::string &error)
```

예외가 발생했을 때 공통 오류 JSON을 만든다.

```cpp
Json::Value body;
body["ok"] = false;
body["message"] = message;
body["error"] = error;
return body;
```

응답 형태는 다음과 같다.

```json
{
  "ok": false,
  "message": "오류 분류 메시지",
  "error": "실제 예외 메시지"
}
```

`message`는 사용자가 이해하기 쉬운 큰 분류이고, `error`는 C++ 예외에서 나온 실제 세부 메시지다.

## configureMariaDbRuntimeForWindows()

```cpp
void configureMariaDbRuntimeForWindows()
```

Windows 로컬 실행에서 MariaDB Connector/C가 인증 플러그인 DLL을 못 찾아 DB 접속에 실패하는 문제를 줄이기 위한 helper다.

```cpp
#ifdef _WIN32
```

Windows에서만 이 블록을 컴파일한다. Linux나 macOS에서는 이 환경 변수 설정이 필요하지 않을 수 있으므로 제외된다.

```cpp
_putenv_s("MARIADB_TLS_DISABLE_PEER_VERIFICATION", "1");
```

로컬 테스트 편의를 위해 TLS peer 검증을 끈다. 운영 환경에서는 그대로 쓰면 안 되고, 실제 인증서/보안 정책에 맞게 다시 판단해야 한다.

```cpp
const std::filesystem::path cwd = std::filesystem::current_path();
```

현재 실행 경로를 구한다. Visual Studio, CMake, 터미널 실행 위치에 따라 `cwd`가 프로젝트 루트일 수도 있고 build 폴더일 수도 있다.

```cpp
const std::vector<std::filesystem::path> candidatePluginDirs = {
    cwd / "build" / "vcpkg_installed" / "x64-windows" / "plugins" / "libmariadb",
    cwd / "vcpkg_installed" / "x64-windows" / "plugins" / "libmariadb",
    cwd.parent_path() / "vcpkg_installed" / "x64-windows" / "plugins" / "libmariadb",
};
```

MariaDB 인증 플러그인이 있을 만한 후보 경로를 순서대로 만든다. 이 예제는 vcpkg 설치 구조를 기준으로 한다.

```cpp
for (const auto &dir : candidatePluginDirs)
```

후보 디렉터리를 하나씩 검사한다.

```cpp
const auto pluginFile = dir / "caching_sha2_password.dll";
```

MySQL 8 계정 인증에 필요한 `caching_sha2_password.dll` 파일 경로를 만든다.

```cpp
if (std::filesystem::exists(pluginFile))
```

실제로 DLL 파일이 있는지 확인한다.

```cpp
_putenv_s("MARIADB_PLUGIN_DIR", dir.string().c_str());
break;
```

DLL이 있는 디렉터리를 `MARIADB_PLUGIN_DIR` 환경 변수로 지정한다. 한 번 찾으면 더 볼 필요가 없으므로 반복문을 끝낸다.

## main()

```cpp
int main()
```

프로그램 시작점이다.

```cpp
configureMariaDbRuntimeForWindows();
```

Drogon DB client가 만들어지기 전에 MariaDB 런타임 환경을 먼저 준비한다.

```cpp
drogon::app().loadConfigFile("config.json");
```

`config.json`을 읽는다. 여기에는 다음 정보가 들어 있다.

- HTTP 서버가 열릴 주소: `127.0.0.1`
- HTTP 포트: `8080`
- DB client 이름: `default`
- DB 종류: `mysql`
- DB 주소: `127.0.0.1:3306`
- DB 이름: `test_db`
- DB 계정: `drogon_test`

```cpp
drogon::app().registerHandler(
    "/db-test",
    [](const drogon::HttpRequestPtr &,
       std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
       ...
    },
    {drogon::Get});
```

`GET /db-test` 요청을 처리할 handler를 등록한다.

첫 번째 lambda 인자는 요청 객체다. 현재 예제에서는 요청 본문이나 query parameter를 쓰지 않기 때문에 이름을 생략했다.

두 번째 lambda 인자는 응답 callback이다. Drogon에서는 handler가 응답 객체를 직접 return하지 않고, callback에 응답을 넘기는 방식으로 처리한다.

```cpp
try
{
    repository::HealthCheckRepository repository(
        drogon::app().getDbClient("default"));
```

`config.json`에 등록된 `"default"` DB client를 꺼내 Repository에 주입한다. 이 구조 덕분에 Repository는 직접 DB 접속 설정을 읽지 않아도 된다.

```cpp
const auto result = repository.runSimpleTest();
```

실제 DB 테스트 흐름을 실행한다. 여기서 테이블 생성, row 저장, 로그 저장, row 재조회가 일어난다.

```cpp
callback(makeJsonResponse(result.toJson()));
```

Repository 결과를 JSON으로 바꾸고 HTTP 응답으로 반환한다.

```cpp
catch (const drogon::orm::DrogonDbException &e)
```

Drogon ORM 또는 DB client에서 발생한 예외를 잡는다. DB 접속 실패, SQL 오류, 테이블/컬럼 문제 등이 여기에 들어올 수 있다.

```cpp
callback(makeJsonResponse(
    makeErrorBody("MySQL ORM 테스트 실패", e.base().what()),
    drogon::k500InternalServerError));
```

DB 오류를 `500` JSON 응답으로 내려준다.

```cpp
catch (const std::exception &e)
```

DB 예외가 아닌 일반 C++ 예외를 잡는다. 예를 들어 `std::out_of_range` 같은 예외가 여기에 들어올 수 있다.

```cpp
drogon::app().run();
```

Drogon 이벤트 루프를 시작한다. 이 함수가 호출되면 서버는 종료될 때까지 요청을 기다린다.

## 실제 서비스로 갈 때 main.cpp에서 줄여야 할 것

현재 예제에서는 `registerHandler()` 안에 lambda로 모든 라우터를 등록한다. 학습용으로는 이해하기 쉽지만, 실제 서비스에서는 다음처럼 분리하는 것이 좋다.

- `main.cpp`: 설정 로드, 서버 실행만 담당
- `controllers/*`: 라우터와 HTTP 요청/응답 담당
- `services/*`: 비즈니스 흐름 담당
- `repositories/*`: DB 조회/저장 담당
- `models/*`: DB row, DTO, domain model 담당

즉, 실제 프로젝트의 `main.cpp`는 가능한 얇게 유지하는 것이 좋다.

