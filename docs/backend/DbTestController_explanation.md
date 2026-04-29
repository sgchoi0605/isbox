# DbTestController.cpp 코드 설명

이 문서는 `backend/controllers/DbTestController.cpp`가 어떤 역할을 하는지 코드 순서대로 설명합니다.

이 컨트롤러의 목적은 하나입니다.

```text
GET /api/db/test 요청이 들어오면 MySQL에 연결해서 SELECT 1 AS ok를 실행하고,
성공하면 {"ok": true, "stage": "ok", "db": 1}을 반환한다.
```

## 전체 흐름

```text
서버 시작
-> registerDbTestRoutes() 호출
-> /api/db/test 라우트 등록
-> 사용자가 GET /api/db/test 요청
-> testDatabase() 실행
-> config.json에서 DB 접속 정보 읽기
-> mysql_init()
-> mysql_options()로 timeout, plugin, SSL 설정
-> mysql_real_connect()로 DB 접속
-> SELECT 1 AS ok 실행
-> JSON 응답 반환
```

## include 구문

```cpp
#include "DbTestController.hpp"
```

`registerDbTestRoutes()` 함수 선언이 들어 있는 헤더입니다.

`main.cpp`에서 이 헤더를 include하고 `registerDbTestRoutes()`를 호출합니다.

```cpp
#include <drogon/drogon.h>
```

Drogon 웹 서버 기능을 사용하기 위한 헤더입니다.

이 파일에서는 다음 기능을 사용합니다.

- `drogon::app().registerHandler(...)`
- `drogon::HttpResponse`
- `drogon::HttpStatusCode`
- `drogon::k200OK`
- `drogon::k500InternalServerError`

```cpp
#include <json/json.h>
```

JsonCpp 라이브러리 헤더입니다.

Drogon이 내부적으로 JsonCpp를 사용하기 때문에 `Json::Value`, `Json::CharReaderBuilder`를 사용할 수 있습니다.

이 파일에서는 두 가지 목적으로 씁니다.

- `config.json` 파싱
- HTTP 응답 JSON 생성

```cpp
#include <mysql/mysql.h>
```

MariaDB/MySQL C API 헤더입니다.

기존에는 Drogon ORM의 `db_clients`를 사용했지만, 현재 코드는 MySQL 연결을 직접 제어합니다.

이 파일에서 사용하는 대표 함수는 다음과 같습니다.

- `mysql_init`
- `mysql_options`
- `mysql_real_connect`
- `mysql_query`
- `mysql_store_result`
- `mysql_fetch_row`
- `mysql_free_result`
- `mysql_close`
- `mysql_errno`
- `mysql_error`

```cpp
#include <fstream>
#include <functional>
#include <memory>
#include <string>
```

C++ 표준 라이브러리 헤더입니다.

- `<fstream>`: `config.json` 파일 읽기
- `<functional>`: 응답 콜백 타입 정의
- `<memory>`: 현재 코드에서는 직접 쓰지 않지만, Drogon 콜백 타입과 함께 쓰기 위해 남아 있음
- `<string>`: DB 설정값과 에러 메시지 저장

## 익명 namespace

```cpp
namespace
{
...
}
```

익명 namespace 안에 있는 함수와 타입은 이 `.cpp` 파일 안에서만 사용할 수 있습니다.

외부 파일에서 직접 호출할 필요가 없는 내부 구현을 숨기는 용도입니다.

현재 외부에 공개되는 함수는 마지막의 `registerDbTestRoutes()` 하나뿐입니다.

## ResponseCallback 타입

```cpp
using ResponseCallback = std::function<void(const drogon::HttpResponsePtr &)>;
```

Drogon 핸들러에서 HTTP 응답을 돌려줄 때 사용하는 콜백 타입을 짧은 이름으로 만든 것입니다.

원래 Drogon 핸들러는 대략 이런 모양입니다.

```cpp
[](const drogon::HttpRequestPtr &, ResponseCallback &&callback) {
    ...
    callback(response);
}
```

요청 처리가 끝나면 `callback(response)`를 호출해서 클라이언트에게 응답을 보냅니다.

## DbSettings 구조체

```cpp
struct DbSettings
{
    std::string host = "127.0.0.1";
    unsigned int port = 3306;
    std::string dbname = "isbox";
    std::string user = "root";
    std::string passwd;
    std::string pluginDir =
        "build-vcpkg/vcpkg_installed/x64-windows/plugins/libmariadb";
    bool ssl = false;
    unsigned int connectTimeoutSeconds = 3;
};
```

DB 접속 정보를 담는 구조체입니다.

각 필드의 의미는 다음과 같습니다.

- `host`: DB 서버 주소입니다. 기본값은 로컬 MySQL인 `127.0.0.1`입니다.
- `port`: MySQL 포트입니다. 기본값은 `3306`입니다.
- `dbname`: 접속할 DB 이름입니다. 기본값은 `isbox`입니다.
- `user`: DB 사용자 이름입니다. 기본값은 `root`입니다.
- `passwd`: DB 비밀번호입니다. 기본값은 비어 있습니다.
- `pluginDir`: MariaDB 클라이언트 인증 플러그인 DLL이 있는 폴더입니다.
- `ssl`: SSL 연결을 강제할지 여부입니다. 현재 기본값은 `false`입니다.
- `connectTimeoutSeconds`: 연결, 읽기, 쓰기 timeout 초 단위입니다.

여기 기본값은 `config.json`을 못 읽었을 때 사용하는 fallback입니다.

실제 값은 `backend/config/config.json`의 `database` 섹션에서 읽어옵니다.

## readDbSettings()

```cpp
DbSettings readDbSettings()
```

`config.json`에서 DB 설정을 읽어서 `DbSettings`로 반환하는 함수입니다.

### 기본 설정 생성

```cpp
DbSettings settings;
```

먼저 기본값이 들어 있는 `DbSettings` 객체를 만듭니다.

설정 파일을 못 찾거나 JSON 파싱에 실패하면 이 기본값이 반환됩니다.

### config.json 경로 후보

```cpp
for (const auto &path : {"backend/config/config.json",
                         "../backend/config/config.json",
                         "../../backend/config/config.json"})
```

실행 위치에 따라 상대경로가 달라질 수 있어서 여러 경로를 시도합니다.

예를 들면 다음 실행 위치들을 고려한 것입니다.

- 프로젝트 루트: `backend/config/config.json`
- 빌드 폴더 근처: `../backend/config/config.json`
- 더 깊은 빌드 폴더: `../../backend/config/config.json`

### 파일 열기

```cpp
std::ifstream configFile(path);
if (!configFile)
{
    continue;
}
```

현재 후보 경로에서 파일을 열어봅니다.

파일이 없거나 열 수 없으면 다음 경로로 넘어갑니다.

### JSON 파싱

```cpp
Json::Value root;
Json::CharReaderBuilder builder;
std::string errors;
if (!Json::parseFromStream(builder, configFile, &root, &errors))
{
    continue;
}
```

파일을 JSON으로 파싱합니다.

파싱에 실패하면 다음 경로를 시도합니다.

`errors`에는 파싱 실패 이유가 들어갈 수 있지만, 현재 코드는 간단한 테스트용이라 응답에 노출하지 않습니다.

### database 섹션 읽기

```cpp
const auto db = root["database"];
if (!db.isObject())
{
    return settings;
}
```

`config.json` 안의 `database` 객체를 읽습니다.

예상 구조는 다음과 같습니다.

```json
{
  "database": {
    "host": "127.0.0.1",
    "port": 3306,
    "dbname": "isbox",
    "user": "root",
    "passwd": "password",
    "plugin_dir": "",
    "ssl": false,
    "connect_timeout_seconds": 3
  }
}
```

`database`가 객체가 아니면 기본값을 그대로 반환합니다.

### 설정값 덮어쓰기

```cpp
settings.host = db.get("host", settings.host).asString();
settings.port = db.get("port", settings.port).asUInt();
settings.dbname = db.get("dbname", settings.dbname).asString();
settings.user = db.get("user", settings.user).asString();
settings.passwd = db.get("passwd", settings.passwd).asString();
settings.pluginDir = db.get("plugin_dir", settings.pluginDir).asString();
settings.ssl = db.get("ssl", settings.ssl).asBool();
```

`db.get(key, defaultValue)`는 JSON에 값이 있으면 그 값을 쓰고, 없으면 기존 기본값을 씁니다.

그래서 `config.json`에 일부 값이 빠져도 프로그램이 바로 죽지는 않습니다.

### timeout 설정 읽기

```cpp
settings.connectTimeoutSeconds =
    db.get("connect_timeout_seconds", settings.connectTimeoutSeconds)
        .asUInt();
```

MySQL 연결 timeout 값을 읽습니다.

이 값은 나중에 다음 세 옵션에 같이 들어갑니다.

- `MYSQL_OPT_CONNECT_TIMEOUT`
- `MYSQL_OPT_READ_TIMEOUT`
- `MYSQL_OPT_WRITE_TIMEOUT`

### 설정 반환

```cpp
return settings;
```

설정 파일을 성공적으로 읽으면 즉시 반환합니다.

모든 경로에서 파일을 못 읽으면 마지막에 기본 설정을 반환합니다.

## mysqlError()

```cpp
std::string mysqlError(MYSQL *connection)
{
    return std::to_string(mysql_errno(connection)) + ": " +
           mysql_error(connection);
}
```

MySQL C API에서 발생한 에러를 문자열로 만들어주는 함수입니다.

반환 예시는 다음과 같습니다.

```text
1045: Access denied for user 'root'@'localhost'
```

응답 JSON의 `detail` 필드에 이 값이 들어갑니다.

## testDatabase()

```cpp
Json::Value testDatabase(drogon::HttpStatusCode &statusCode)
```

실제 DB 연결 테스트를 수행하는 핵심 함수입니다.

반환값은 HTTP 응답 body에 들어갈 JSON입니다.

`statusCode`는 참조로 받아서 함수 안에서 `200` 또는 `500`으로 설정합니다.

### 설정 읽기

```cpp
const auto settings = readDbSettings();
Json::Value body;
```

먼저 DB 설정을 읽고, 응답 JSON을 담을 `body`를 만듭니다.

### MySQL 연결 객체 생성

```cpp
MYSQL *connection = mysql_init(nullptr);
```

MySQL 연결 핸들을 생성합니다.

이 단계는 아직 실제 DB에 접속한 것이 아닙니다.

### mysql_init 실패 처리

```cpp
if (connection == nullptr)
{
    statusCode = drogon::k500InternalServerError;
    body["ok"] = false;
    body["stage"] = "init";
    body["detail"] = "mysql_init failed";
    return body;
}
```

연결 객체 생성 자체가 실패하면 `500` 응답을 반환합니다.

`stage`가 `init`이면 MySQL 클라이언트 초기화 단계에서 실패했다는 뜻입니다.

## mysql_options() 설정

### 연결 timeout

```cpp
mysql_options(connection,
              MYSQL_OPT_CONNECT_TIMEOUT,
              &settings.connectTimeoutSeconds);
```

DB 연결을 시도할 때 최대 몇 초 기다릴지 설정합니다.

### 읽기 timeout

```cpp
mysql_options(connection,
              MYSQL_OPT_READ_TIMEOUT,
              &settings.connectTimeoutSeconds);
```

DB에서 응답을 읽을 때 최대 몇 초 기다릴지 설정합니다.

### 쓰기 timeout

```cpp
mysql_options(connection,
              MYSQL_OPT_WRITE_TIMEOUT,
              &settings.connectTimeoutSeconds);
```

DB로 요청을 보낼 때 최대 몇 초 기다릴지 설정합니다.

### 플러그인 경로 설정

```cpp
if (!settings.pluginDir.empty())
{
    mysql_options(connection, MYSQL_PLUGIN_DIR, settings.pluginDir.c_str());
}
```

MariaDB 클라이언트가 인증 플러그인 DLL을 찾을 수 있게 경로를 알려줍니다.

이 프로젝트에서 이 설정이 필요한 이유는 MySQL 8 계정이 `caching_sha2_password` 인증 방식을 사용할 수 있기 때문입니다.

이 경로가 없으면 다음과 같은 에러가 날 수 있습니다.

```text
Plugin caching_sha2_password could not be loaded
```

## SSL 관련 설정

```cpp
my_bool sslEnforce = settings.ssl ? 1 : 0;
my_bool verifyServerCert = 0;
mysql_options(connection, MYSQL_OPT_SSL_ENFORCE, &sslEnforce);
mysql_options(connection,
              MYSQL_OPT_SSL_VERIFY_SERVER_CERT,
              &verifyServerCert);
```

SSL 연결 정책을 설정합니다.

현재 기본값은 `ssl = false`입니다.

그래서 기본 동작은 다음과 같습니다.

- SSL 연결을 강제하지 않음
- 서버 인증서 검증도 하지 않음

이 설정을 둔 이유는 로컬 개발 환경에서 SSL 인증서가 없어서 다음 에러가 났기 때문입니다.

```text
TLS/SSL error: no credentials
```

운영 서버에서 SSL을 써야 한다면 `config.json`에서 `ssl`을 `true`로 바꾸고, 인증서 설정을 별도로 정리해야 합니다.

## mysql_real_connect()

```cpp
const unsigned long flags = settings.ssl ? CLIENT_SSL : 0;
```

SSL을 사용할 경우 `CLIENT_SSL` 플래그를 켭니다.

SSL을 사용하지 않으면 플래그는 `0`입니다.

```cpp
if (mysql_real_connect(connection,
                       settings.host.c_str(),
                       settings.user.c_str(),
                       settings.passwd.c_str(),
                       settings.dbname.c_str(),
                       settings.port,
                       nullptr,
                       flags) == nullptr)
```

실제 MySQL 서버에 접속합니다.

인자로 들어가는 값은 다음과 같습니다.

- `connection`: `mysql_init()`으로 만든 연결 핸들
- `settings.host`: DB 주소
- `settings.user`: DB 사용자
- `settings.passwd`: DB 비밀번호
- `settings.dbname`: DB 이름
- `settings.port`: DB 포트
- `nullptr`: Unix socket 경로입니다. Windows/TCP 접속이므로 쓰지 않습니다.
- `flags`: SSL 사용 여부 같은 연결 플래그입니다.

### 연결 실패 처리

```cpp
statusCode = drogon::k500InternalServerError;
body["ok"] = false;
body["stage"] = "connect";
body["detail"] = mysqlError(connection);
mysql_close(connection);
return body;
```

DB 접속에 실패하면 `500` 응답을 반환합니다.

`stage`가 `connect`이면 다음 문제일 가능성이 큽니다.

- MySQL 서버가 꺼져 있음
- host 또는 port가 틀림
- DB 계정 또는 비밀번호가 틀림
- DB 이름이 없음
- 인증 플러그인 문제
- SSL 설정 문제

실패하더라도 `mysql_close(connection)`으로 연결 핸들을 정리합니다.

## SELECT 1 실행

```cpp
if (mysql_query(connection, "SELECT 1 AS ok") != 0)
```

DB 연결이 된 후 간단한 쿼리를 실행합니다.

`SELECT 1 AS ok`는 테이블이 없어도 실행됩니다.

그래서 이 테스트는 schema나 table 존재 여부가 아니라 다음만 확인합니다.

- DB 서버에 접속 가능
- 인증 성공
- SQL 실행 가능
- 결과를 읽을 수 있음

### 쿼리 실패 처리

```cpp
statusCode = drogon::k500InternalServerError;
body["ok"] = false;
body["stage"] = "query";
body["detail"] = mysqlError(connection);
mysql_close(connection);
return body;
```

쿼리 실행이 실패하면 `stage`는 `query`가 됩니다.

## 결과 가져오기

```cpp
MYSQL_RES *result = mysql_store_result(connection);
```

쿼리 결과를 클라이언트 메모리로 가져옵니다.

`SELECT 1 AS ok`는 결과 row를 반환하는 쿼리이기 때문에 `mysql_store_result()`가 필요합니다.

### 결과 가져오기 실패 처리

```cpp
if (result == nullptr)
{
    statusCode = drogon::k500InternalServerError;
    body["ok"] = false;
    body["stage"] = "result";
    body["detail"] = mysqlError(connection);
    mysql_close(connection);
    return body;
}
```

쿼리는 실행됐지만 결과를 가져오지 못하면 `stage`는 `result`가 됩니다.

## 성공 응답 만들기

```cpp
MYSQL_ROW row = mysql_fetch_row(result);
```

결과에서 첫 번째 row를 가져옵니다.

`SELECT 1 AS ok`의 결과는 보통 한 row, 한 column입니다.

```cpp
body["ok"] = true;
body["stage"] = "ok";
body["db"] = row && row[0] ? std::stoi(row[0]) : 0;
statusCode = drogon::k200OK;
```

성공하면 JSON body를 이렇게 만듭니다.

```json
{
  "ok": true,
  "stage": "ok",
  "db": 1
}
```

`row[0]` 값은 문자열이므로 `std::stoi(row[0])`로 숫자 변환합니다.

row가 비어 있으면 안전하게 `0`을 넣습니다.

## 리소스 정리

```cpp
mysql_free_result(result);
mysql_close(connection);
return body;
```

사용한 MySQL 결과 객체와 연결 객체를 정리합니다.

- `mysql_free_result(result)`: 쿼리 결과 메모리 해제
- `mysql_close(connection)`: DB 연결 종료

이 정리를 하지 않으면 요청이 반복될 때 메모리나 연결이 쌓일 수 있습니다.

## registerDbTestRoutes()

```cpp
void registerDbTestRoutes()
```

이 파일에서 외부로 공개되는 라우트 등록 함수입니다.

`main.cpp`에서 이 함수를 호출해야 `/api/db/test`가 등록됩니다.

## registerHandler()

```cpp
drogon::app().registerHandler(
    "/api/db/test",
    ...
    {drogon::Get});
```

Drogon 앱에 HTTP 핸들러를 등록합니다.

등록되는 API는 다음과 같습니다.

```text
GET /api/db/test
```

마지막의 `{drogon::Get}` 때문에 GET 요청만 처리합니다.

POST, PUT, DELETE 요청은 이 핸들러로 들어오지 않습니다.

## 요청 핸들러 람다

```cpp
[](const drogon::HttpRequestPtr &,
   ResponseCallback &&callback) {
```

Drogon이 요청을 받으면 이 람다 함수를 실행합니다.

첫 번째 인자는 요청 객체입니다.

현재 DB 테스트는 request body나 query string을 쓰지 않기 때문에 변수명을 생략했습니다.

두 번째 인자인 `callback`은 HTTP 응답을 돌려주는 함수입니다.

## 응답 생성

```cpp
drogon::HttpStatusCode statusCode;
auto response =
    drogon::HttpResponse::newHttpJsonResponse(
        testDatabase(statusCode));
```

`testDatabase(statusCode)`를 실행해서 DB 테스트 결과 JSON을 가져옵니다.

그 JSON을 Drogon HTTP JSON 응답 객체로 감쌉니다.

`statusCode`는 `testDatabase()` 안에서 성공이면 `200`, 실패면 `500`으로 설정됩니다.

## HTTP status 설정

```cpp
response->setStatusCode(statusCode);
```

응답의 HTTP status code를 설정합니다.

성공하면:

```text
HTTP/1.1 200 OK
```

실패하면:

```text
HTTP/1.1 500 Internal Server Error
```

## 응답 전송

```cpp
callback(response);
```

클라이언트에게 응답을 보냅니다.

이 줄이 실행되어야 브라우저나 curl에서 결과를 받을 수 있습니다.

## 성공 응답 예시

```http
HTTP/1.1 200 OK
Content-Type: application/json

{"db":1,"ok":true,"stage":"ok"}
```

## 실패 응답 예시

DB 접속 실패 예시는 다음과 같습니다.

```http
HTTP/1.1 500 Internal Server Error
Content-Type: application/json

{
  "ok": false,
  "stage": "connect",
  "detail": "1045: Access denied for user ..."
}
```

`stage` 값으로 어디서 실패했는지 빠르게 구분할 수 있습니다.

## stage 값 정리

| stage | 의미 | 주로 확인할 것 |
| --- | --- | --- |
| `init` | MySQL 클라이언트 초기화 실패 | MariaDB/MySQL 클라이언트 라이브러리 |
| `connect` | DB 접속 실패 | host, port, user, passwd, dbname, plugin, SSL |
| `query` | SQL 실행 실패 | SQL 문법, DB 권한 |
| `result` | SQL 결과 읽기 실패 | MySQL 결과 처리 |
| `ok` | DB 테스트 성공 | 정상 |

## 왜 Drogon db_clients를 안 쓰는가

처음 구현은 Drogon ORM의 `db_clients` 설정을 사용했습니다.

하지만 로컬 Windows 환경에서 다음 문제가 발생했습니다.

```text
TLS/SSL error: no credentials
Plugin caching_sha2_password could not be loaded
```

그래서 현재 코드는 DB 테스트 API 안에서 MariaDB C API를 직접 사용합니다.

직접 사용하는 이유는 다음 옵션을 코드에서 명확히 제어하기 위해서입니다.

- SSL 강제 여부
- 서버 인증서 검증 여부
- 인증 플러그인 DLL 경로
- 연결 timeout

## config.json을 쓰는 이유

DB 접속 정보는 환경마다 달라집니다.

예를 들면 개발자 PC, 학교 PC, 배포 서버가 모두 다른 값을 쓸 수 있습니다.

그래서 다음 값은 C++ 코드에 고정하지 않고 `backend/config/config.json`에서 읽습니다.

- `host`
- `port`
- `dbname`
- `user`
- `passwd`
- `plugin_dir`
- `ssl`
- `connect_timeout_seconds`

코드에 비밀번호를 박아두면 DB 비밀번호가 바뀔 때마다 다시 빌드해야 합니다.

반대로 config 파일에 두면 설정 파일만 바꿔서 실행할 수 있습니다.

## 현재 코드의 한계

이 코드는 DB 연결 테스트용입니다.

회원 테이블, 로그인 테이블, schema까지 검증하지는 않습니다.

즉 성공 응답은 다음을 의미합니다.

```text
MySQL 서버 연결과 SELECT 1 실행은 된다.
```

다음을 의미하지는 않습니다.

```text
member 테이블이 있다.
login 기능이 완성됐다.
mapper/service/controller 전체 흐름이 검증됐다.
```

회원 기능까지 테스트하려면 별도의 API 또는 테스트 코드를 추가해야 합니다.

## 관련 파일

- `backend/controllers/DbTestController.cpp`: DB 테스트 API 구현
- `backend/controllers/DbTestController.hpp`: 라우트 등록 함수 선언
- `backend/main.cpp`: `registerDbTestRoutes()` 호출
- `backend/config/config.json`: 실제 DB 접속 설정
- `backend/config/config.json.example`: 공유용 설정 예시
- `CMakeLists.txt`: Drogon과 MariaDB 클라이언트 링크 설정
