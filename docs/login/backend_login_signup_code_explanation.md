# Backend 로그인/회원가입 코드 상세 설명

이 문서는 `backend`의 로그인과 회원가입 기능이 어떤 파일을 거쳐 동작하는지 계층별로 설명한다.

대상 파일은 다음과 같다.

- `backend/app/main.cpp`
- `backend/controllers/MemberController.h`
- `backend/controllers/MemberController.cpp`
- `backend/services/MemberService.h`
- `backend/services/MemberService.cpp`
- `backend/mappers/MemberMapper.h`
- `backend/mappers/MemberMapper.cpp`
- `backend/models/MemberTypes.h`

## 전체 구조

로그인/회원가입은 다음 흐름으로 처리된다.

```text
브라우저
  -> POST /api/auth/signup 또는 POST /api/auth/login
  -> MemberController
  -> MemberService
  -> MemberMapper
  -> members 테이블
  -> MemberMapper
  -> MemberService
  -> MemberController
  -> JSON 응답 + isbox_session 쿠키
```

각 계층의 책임은 분리되어 있다.

- `app`: Drogon 서버를 시작하고 컨트롤러 라우트를 등록한다.
- `controllers`: HTTP 요청/응답을 처리한다. JSON 파싱, 쿠키 설정, 상태 코드 설정을 담당한다.
- `services`: 로그인/회원가입의 실제 비즈니스 규칙을 처리한다. 입력 검증, 비밀번호 확인, 세션 생성이 여기에 있다.
- `mappers`: DB와 직접 통신한다. `members` 테이블 조회, 삽입, 업데이트를 담당한다.
- `models`: 계층 사이에서 주고받는 데이터 구조를 정의한다.

## app/main.cpp

`main.cpp`는 인증 로직을 직접 처리하지 않는다. 대신 서버 환경을 준비하고 `MemberController`를 등록해서 로그인/회원가입 API가 동작하게 만든다.

### include

```cpp
#include <drogon/drogon.h>
```

Drogon 서버, 라우팅, 요청/응답, DB 클라이언트를 쓰기 위한 핵심 헤더다.

```cpp
#include <cstdlib>
#include <filesystem>
#include <vector>
```

Windows MariaDB 런타임 설정에 필요한 표준 라이브러리다.

```cpp
#include "../controllers/MemberController.h"
#include "../controllers/BoardController.h"
```

서버 시작 시 컨트롤러 객체를 만들고 라우트를 등록하기 위해 포함한다. 로그인/회원가입은 `MemberController`가 담당한다.

### 익명 namespace

```cpp
namespace
{
...
}
```

이 안의 함수는 현재 `.cpp` 파일에서만 보인다. `configureMariaDbRuntimeForWindows`, `registerApiCorsPreflight` 같은 서버 내부 보조 함수가 외부로 노출되지 않게 한다.

### configureMariaDbRuntimeForWindows

```cpp
void configureMariaDbRuntimeForWindows()
{
#ifdef _WIN32
```

Windows에서만 실행되는 MariaDB 클라이언트 런타임 보정 함수다.

```cpp
_putenv_s("MARIADB_TLS_DISABLE_PEER_VERIFICATION", "1");
```

로컬 개발 환경에서 MariaDB TLS 인증서 검증 문제를 피하기 위해 환경 변수를 설정한다.

```cpp
const std::filesystem::path cwd = std::filesystem::current_path();
```

현재 실행 디렉터리를 가져온다. 이후 MariaDB 플러그인 DLL 위치 후보를 만들 때 기준 경로로 쓴다.

```cpp
const std::vector<std::filesystem::path> candidatePluginDirs = { ... };
```

`caching_sha2_password.dll`이 있을 수 있는 후보 폴더 목록이다. vcpkg 설치 위치가 빌드 폴더 안쪽인지, backend 폴더 안쪽인지, 부모 폴더 쪽인지 모두 확인한다.

```cpp
for (const auto &dir : candidatePluginDirs)
```

후보 디렉터리를 하나씩 검사한다.

```cpp
const auto pluginFile = dir / "caching_sha2_password.dll";
```

MySQL/MariaDB 인증 플러그인 DLL 경로를 만든다.

```cpp
if (std::filesystem::exists(pluginFile))
{
    _putenv_s("MARIADB_PLUGIN_DIR", dir.string().c_str());
    break;
}
```

DLL이 존재하는 첫 번째 폴더를 `MARIADB_PLUGIN_DIR`로 등록한다. DB 접속 시 인증 플러그인을 찾을 수 있게 하기 위한 설정이다.

### registerApiCorsPreflight

```cpp
void registerApiCorsPreflight()
{
    drogon::app().registerPreRoutingAdvice(...)
}
```

라우터가 요청을 컨트롤러로 보내기 전에 먼저 실행되는 전처리 로직을 등록한다.

```cpp
if (request->getMethod() == drogon::Options &&
    request->path().rfind("/api/", 0) == 0)
```

브라우저의 CORS preflight 요청인지 검사한다. `OPTIONS` 요청이고 경로가 `/api/`로 시작하면 API용 preflight로 본다.

```cpp
auto response = drogon::HttpResponse::newHttpResponse();
response->setStatusCode(drogon::k204NoContent);
```

본문 없는 성공 응답을 만든다. preflight는 실제 데이터를 내려주는 요청이 아니기 때문에 `204 No Content`가 적절하다.

```cpp
auto origin = request->getHeader("Origin");
if (origin.empty())
{
    origin = request->getHeader("origin");
}
```

요청의 Origin 헤더를 읽는다. 대소문자 차이를 고려해 두 번 확인한다.

```cpp
response->addHeader("Access-Control-Allow-Origin", origin);
response->addHeader("Vary", "Origin");
```

브라우저가 해당 Origin에서 온 요청을 허용하도록 응답 헤더를 추가한다.

```cpp
response->addHeader("Access-Control-Allow-Credentials", "true");
```

쿠키를 포함한 요청을 허용한다. 로그인/회원가입 후 `isbox_session` 쿠키를 쓰기 때문에 필요하다.

```cpp
response->addHeader("Access-Control-Allow-Headers", "Content-Type, X-Member-Id");
```

프론트엔드가 보낼 수 있는 요청 헤더를 허용한다. 로그인/회원가입에는 `Content-Type`이 중요하고, 게시판 작성에는 `X-Member-Id`가 쓰인다.

```cpp
response->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
```

API에서 허용할 HTTP 메서드를 알려준다.

```cpp
adviceCallback(response);
return;
```

preflight 요청은 컨트롤러까지 보내지 않고 여기서 바로 응답한다.

```cpp
adviceChainCallback();
```

preflight가 아닌 요청은 다음 단계로 넘긴다.

### main

```cpp
int main()
{
    configureMariaDbRuntimeForWindows();
```

프로그램 시작 시 Windows DB 런타임 설정을 먼저 수행한다.

```cpp
drogon::app().loadConfigFile("config/config.json");
```

Drogon 설정 파일을 읽는다. DB 연결 정보, 포트 같은 서버 설정이 여기서 로드된다.

```cpp
drogon::app().setDocumentRoot("../frontend");
```

정적 파일 루트를 `frontend`로 설정한다. 로그인 화면 HTML, CSS, JS 같은 파일을 Drogon이 직접 제공할 수 있게 한다.

```cpp
registerApiCorsPreflight();
```

API CORS preflight 처리를 등록한다.

```cpp
auth::MemberController memberController;
memberController.registerHandlers();
```

로그인, 회원가입, 현재 사용자 조회, 로그아웃 API 라우트를 등록한다. 인증 기능의 HTTP 진입점은 여기서 서버에 연결된다.

```cpp
board::BoardController boardController;
boardController.registerHandlers();
```

게시판 API 라우트도 등록한다. 로그인/회원가입의 핵심은 아니지만, 같은 서버 프로세스에서 함께 실행된다.

```cpp
drogon::app().registerHandler("/", ...);
drogon::app().registerHandler("/index.html", ...);
```

브라우저가 `/` 또는 `/index.html`로 접속하면 `/html-version/index.html`로 이동시킨다. 즉 기본 진입 화면은 `frontend/html-version/index.html`이다.

```cpp
drogon::app().run();
```

Drogon 이벤트 루프를 시작한다. 이 줄 이후 서버가 요청을 받기 시작한다.

## models/MemberTypes.h

이 파일은 로그인/회원가입에서 쓰는 데이터 모양을 정의한다. DB 조회 결과, 서비스 결과, 컨트롤러 요청 DTO가 모두 여기에 있다.

### include

```cpp
#include <cstdint>
#include <optional>
#include <string>
```

`std::uint64_t`, `std::optional`, `std::string`을 쓰기 위한 헤더다.

### namespace auth

```cpp
namespace auth
{
...
}
```

인증 관련 타입을 `auth` 네임스페이스 안에 묶는다. 게시판의 `board` 네임스페이스와 충돌하지 않게 한다.

### LoginRequestDTO

```cpp
class LoginRequestDTO
{
public:
    std::string email;
    std::string password;
};
```

로그인 요청 본문을 담는 DTO다.

- `email`: 사용자가 입력한 이메일이다.
- `password`: 사용자가 입력한 비밀번호 원문이다.

이 값은 `MemberController::handleLogin`에서 JSON으로부터 채워지고, `MemberService::login`으로 넘어간다.

### SignupRequestDTO

```cpp
class SignupRequestDTO
{
public:
    std::string name;
    std::string email;
    std::string password;
    std::string confirmPassword;
    bool agreeTerms{false};
};
```

회원가입 요청 본문을 담는 DTO다.

- `name`: 사용자 이름이다.
- `email`: 가입 이메일이다.
- `password`: 비밀번호 원문이다.
- `confirmPassword`: 비밀번호 확인 입력값이다.
- `agreeTerms`: 약관 동의 여부다. 기본값은 `false`다.

컨트롤러는 이 구조체에 프론트엔드 요청 값을 넣고 서비스로 넘긴다.

### MemberModel

```cpp
class MemberModel
{
public:
    std::uint64_t memberId{0};
    std::string email;
    std::string passwordHash;
    std::string name;
    std::string role;
    std::string status;
    unsigned int level{1};
    unsigned int exp{0};
    std::optional<std::string> lastLoginAt;
};
```

DB의 `members` 테이블 데이터를 담는 내부 모델이다.

- `memberId`: DB 기본키 `member_id`다.
- `email`: 회원 이메일이다.
- `passwordHash`: DB에 저장된 비밀번호 해시다. 응답으로 내려주면 안 된다.
- `name`: 회원 이름이다.
- `role`: 회원 권한이다. 기본값은 DB에서 `USER`로 잡힌다.
- `status`: 계정 상태다. 로그인 시 `ACTIVE`인지 확인한다.
- `level`: 사용자 레벨이다.
- `exp`: 경험치다.
- `lastLoginAt`: 마지막 로그인 시간이다. DB 값이 없을 수 있으므로 `optional`이다.

중요한 점은 `MemberModel`은 내부용이라는 것이다. 비밀번호 해시를 포함하기 때문에 컨트롤러 응답에 직접 쓰지 않는다.

### MemberDTO

```cpp
class MemberDTO
{
public:
    std::uint64_t memberId{0};
    std::string email;
    std::string name;
    std::string role;
    std::string status;
    unsigned int level{1};
    unsigned int exp{0};
    std::optional<std::string> lastLoginAt;
};
```

클라이언트에게 내려줄 수 있는 회원 데이터다.

`MemberModel`과 비슷하지만 `passwordHash`가 없다. 이 차이가 중요하다. 서비스와 컨트롤러는 최종 응답을 만들 때 `MemberModel`을 그대로 쓰지 않고 `MemberDTO`로 변환한다.

### AuthResultDTO

```cpp
class AuthResultDTO
{
public:
    bool ok{false};
    int statusCode{400};
    std::string message;
    std::optional<MemberDTO> member;
    std::optional<std::string> sessionToken;
};
```

로그인/회원가입 서비스 결과를 담는 DTO다.

- `ok`: 성공 여부다.
- `statusCode`: 컨트롤러가 HTTP 상태 코드로 사용할 값이다.
- `message`: 사용자 또는 프론트엔드가 볼 메시지다.
- `member`: 성공 시 응답에 포함할 회원 정보다.
- `sessionToken`: 성공 시 쿠키로 내려줄 세션 토큰이다.

서비스가 이 값을 만들어 반환하고, 컨트롤러가 이 값을 HTTP 응답으로 바꾼다.

## controllers/MemberController.h

헤더 파일은 컨트롤러의 공개 기능과 내부 보조 함수를 선언한다.

```cpp
#include <drogon/HttpTypes.h>
#include <drogon/drogon.h>
```

Drogon의 요청/응답 타입과 앱 라우팅 기능을 쓰기 위해 포함한다.

```cpp
#include <functional>
#include <string>
```

콜백 타입과 문자열 처리를 위해 포함한다.

```cpp
#include "../services/MemberService.h"
```

컨트롤러가 실제 로그인/회원가입 처리를 서비스에 맡기기 위해 포함한다.

### MemberController class

```cpp
class MemberController
{
  public:
    MemberController() = default;

    void registerHandlers();
```

`MemberController`는 HTTP 라우트를 등록하고 요청을 처리하는 클래스다.

- 생성자는 기본 생성자를 쓴다.
- `registerHandlers`는 `/api/auth/signup`, `/api/auth/login` 같은 URL을 Drogon에 연결한다.

```cpp
using Callback = std::function<void(const drogon::HttpResponsePtr &)>;
```

Drogon 핸들러가 응답을 돌려줄 때 쓰는 콜백 타입을 짧게 이름 붙인 것이다.

```cpp
static Json::Value buildMemberJson(const MemberDTO &member);
```

`MemberDTO`를 JSON 응답 모양으로 바꾼다.

```cpp
static void applyCors(...);
```

응답에 CORS 헤더를 붙인다.

```cpp
static std::string extractSessionToken(...);
```

요청 쿠키에서 `isbox_session` 값을 꺼낸다.

```cpp
static std::string buildSessionCookie(...);
```

세션 토큰을 `Set-Cookie` 헤더 문자열로 만든다.

```cpp
void handleSignup(...);
void handleLogin(...);
void handleMe(...);
void handleLogout(...);
```

각 API 요청을 실제로 처리하는 함수다.

```cpp
MemberService service_;
```

컨트롤러가 소유하는 서비스 객체다. 컨트롤러는 비즈니스 로직을 직접 처리하지 않고 이 객체에 위임한다.

## controllers/MemberController.cpp

이 파일은 HTTP 요청과 서비스 계층 사이를 연결한다.

### 파일 상단

```cpp
#include "MemberController.h"
#include <sstream>
#include <utility>
```

자기 헤더를 포함하고, 쿠키 문자열 조립용 `sstream`, 콜백 이동용 `utility`를 포함한다.

```cpp
constexpr const char *kSessionCookieName = "isbox_session";
```

로그인 세션 쿠키 이름이다. 로그인/회원가입 성공 시 이 이름으로 쿠키가 내려가고, `/api/members/me`는 이 이름의 쿠키를 읽는다.

```cpp
Json::Value makeErrorBody(const std::string &message)
```

에러 응답 JSON을 만드는 내부 함수다.

```cpp
body["ok"] = false;
body["message"] = message;
```

모든 컨트롤러 에러 응답은 최소한 `ok: false`, `message: ...` 형태를 가진다.

### registerHandlers

```cpp
auto &app = drogon::app();
```

전역 Drogon 앱 객체를 가져온다.

```cpp
app.registerHandler("/api/auth/signup", ..., {drogon::Post});
```

회원가입 API를 등록한다. `POST /api/auth/signup` 요청이 오면 `handleSignup`이 실행된다.

```cpp
[this](const drogon::HttpRequestPtr &request, Callback &&callback) {
    handleSignup(request, std::move(callback));
}
```

람다 함수가 Drogon 요청을 받아 클래스 멤버 함수로 넘긴다. `std::move(callback)`은 응답 콜백 소유권을 넘기는 의미다.

```cpp
app.registerHandler("/api/auth/login", ..., {drogon::Post});
```

로그인 API를 등록한다. `POST /api/auth/login` 요청이 오면 `handleLogin`이 실행된다.

```cpp
app.registerHandler("/api/members/me", ..., {drogon::Get});
```

현재 로그인한 사용자 조회 API다. 세션 쿠키가 유효하면 회원 정보를 돌려준다.

```cpp
app.registerHandler("/api/auth/logout", ..., {drogon::Post});
```

로그아웃 API다. 서버 메모리 세션을 지우고 쿠키도 만료시킨다.

### buildMemberJson

```cpp
Json::Value memberJson;
```

응답으로 내려줄 JSON 객체를 만든다.

```cpp
memberJson["memberId"] = static_cast<Json::UInt64>(member.memberId);
```

C++의 `uint64_t`를 JSON 라이브러리가 이해하는 정수 타입으로 바꿔 넣는다.

```cpp
memberJson["email"] = member.email;
memberJson["name"] = member.name;
memberJson["role"] = member.role;
memberJson["status"] = member.status;
memberJson["level"] = member.level;
memberJson["exp"] = member.exp;
```

클라이언트가 사용할 회원 정보를 JSON에 넣는다.

```cpp
memberJson["loggedIn"] = true;
```

프론트엔드에서 로그인 상태 판단에 쓸 수 있도록 항상 `true`를 넣는다. 이 함수는 로그인된 회원을 응답할 때만 호출된다.

```cpp
if (member.lastLoginAt.has_value())
{
    memberJson["lastLoginAt"] = *member.lastLoginAt;
}
```

마지막 로그인 시간이 있으면 응답에 포함한다. 값이 없으면 필드 자체를 생략한다.

### applyCors

```cpp
std::string origin = request->getHeader("Origin");
```

요청 Origin을 읽는다.

```cpp
if (origin.empty())
{
    origin = request->getHeader("origin");
}
```

헤더 대소문자 차이를 보정한다.

```cpp
if (!origin.empty())
{
    response->addHeader("Access-Control-Allow-Origin", origin);
    response->addHeader("Vary", "Origin");
}
```

Origin이 있는 브라우저 요청이면 해당 Origin을 허용한다.

```cpp
response->addHeader("Access-Control-Allow-Credentials", "true");
```

쿠키 기반 로그인이라서 credentials 허용이 필요하다.

```cpp
response->addHeader("Access-Control-Allow-Headers", "Content-Type");
response->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
```

컨트롤러 응답에도 CORS 관련 허용 헤더를 붙인다.

### extractSessionToken

```cpp
return request->getCookie(kSessionCookieName);
```

요청 쿠키에서 `isbox_session` 값을 꺼낸다. 없으면 빈 문자열이 반환된다.

### buildSessionCookie

```cpp
std::ostringstream cookie;
```

쿠키 문자열을 조립하기 위한 스트림이다.

```cpp
cookie << kSessionCookieName << '=' << token
       << "; Path=/; HttpOnly; SameSite=Lax; Max-Age=" << maxAgeSeconds;
```

최종 쿠키 문자열을 만든다.

- `Path=/`: 사이트 전체 경로에서 쿠키를 보낸다.
- `HttpOnly`: 자바스크립트에서 쿠키를 직접 읽지 못하게 한다.
- `SameSite=Lax`: 기본적인 CSRF 위험을 줄인다.
- `Max-Age`: 쿠키 유지 시간이다.

로그인/회원가입 성공 시 `maxAgeSeconds`는 `86400`이고, 로그아웃 시에는 `0`이다.

### handleSignup

```cpp
const auto json = request->getJsonObject();
```

요청 본문을 JSON으로 파싱한다.

```cpp
if (!json)
```

JSON 파싱에 실패했거나 본문이 없으면 실패 처리한다.

```cpp
newHttpJsonResponse(makeErrorBody("Invalid JSON body."));
response->setStatusCode(drogon::k400BadRequest);
```

잘못된 요청이므로 `400 Bad Request`를 반환한다.

```cpp
SignupRequestDTO dto;
dto.name = json->get("name", "").asString();
dto.email = json->get("email", "").asString();
dto.password = json->get("password", "").asString();
dto.confirmPassword = json->get("confirmPassword", "").asString();
dto.agreeTerms = json->get("agreeTerms", false).asBool();
```

프론트엔드 JSON 값을 `SignupRequestDTO`에 담는다. 값이 없으면 문자열은 `""`, 약관 동의는 `false`가 된다.

```cpp
const auto result = service_.signup(dto);
```

회원가입의 실제 검증과 DB 처리는 서비스에 맡긴다.

```cpp
Json::Value body;
body["ok"] = result.ok;
body["message"] = result.message;
```

서비스 결과를 HTTP 응답 본문으로 변환한다.

```cpp
if (result.member.has_value())
{
    body["member"] = buildMemberJson(*result.member);
}
```

회원가입 성공 시 회원 정보를 응답에 포함한다.

```cpp
auto response = drogon::HttpResponse::newHttpJsonResponse(body);
response->setStatusCode(static_cast<drogon::HttpStatusCode>(result.statusCode));
```

JSON 응답 객체를 만들고, 서비스가 정한 상태 코드를 HTTP 상태 코드로 적용한다.

```cpp
if (result.ok && result.sessionToken.has_value())
{
    response->addHeader("Set-Cookie", buildSessionCookie(*result.sessionToken, 86400));
}
```

회원가입 성공 시 자동 로그인되도록 세션 쿠키를 내려준다.

```cpp
applyCors(request, response);
callback(response);
```

CORS 헤더를 붙이고 응답을 반환한다.

### handleLogin

로그인은 `handleSignup`과 구조가 거의 같다. 차이는 요청 DTO와 호출하는 서비스 함수다.

```cpp
const auto json = request->getJsonObject();
```

요청 본문을 JSON으로 읽는다.

```cpp
LoginRequestDTO dto;
dto.email = json->get("email", "").asString();
dto.password = json->get("password", "").asString();
```

로그인에는 이메일과 비밀번호만 필요하다.

```cpp
const auto result = service_.login(dto);
```

실제 로그인 검증은 서비스가 처리한다.

```cpp
body["ok"] = result.ok;
body["message"] = result.message;
```

서비스 결과를 응답 본문에 넣는다.

```cpp
if (result.member.has_value())
{
    body["member"] = buildMemberJson(*result.member);
}
```

로그인 성공 시 회원 정보를 내려준다.

```cpp
if (result.ok && result.sessionToken.has_value())
{
    response->addHeader("Set-Cookie", buildSessionCookie(*result.sessionToken, 86400));
}
```

로그인 성공 시 `isbox_session` 쿠키를 내려준다.

### handleMe

```cpp
const auto sessionToken = extractSessionToken(request);
```

요청 쿠키에서 세션 토큰을 꺼낸다.

```cpp
const auto member = service_.getCurrentMember(sessionToken);
```

세션 토큰이 유효한지 확인하고, 유효하면 현재 회원 정보를 가져온다.

```cpp
if (!member.has_value())
```

토큰이 없거나 만료됐거나 회원이 비활성 상태면 인증 실패다.

```cpp
response->setStatusCode(drogon::k401Unauthorized);
```

현재 로그인 사용자를 확인할 수 없으므로 `401 Unauthorized`를 반환한다.

```cpp
body["ok"] = true;
body["member"] = buildMemberJson(*member);
```

세션이 유효하면 회원 정보를 JSON으로 내려준다.

### handleLogout

```cpp
const auto sessionToken = extractSessionToken(request);
service_.logout(sessionToken);
```

쿠키에서 토큰을 꺼내 서버 메모리 세션 저장소에서 삭제한다.

```cpp
body["ok"] = true;
body["message"] = "Logout success.";
```

로그아웃은 세션이 없더라도 성공 응답을 돌려준다.

```cpp
response->addHeader("Set-Cookie", buildSessionCookie("", 0));
```

빈 토큰과 `Max-Age=0`으로 쿠키를 만료시킨다.

## services/MemberService.h

서비스 헤더는 인증 비즈니스 로직의 공개 함수와 내부 도우미를 선언한다.

```cpp
#include <chrono>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
```

세션 만료 시간, 동시성 제어, optional 반환값, 문자열, 메모리 세션 저장소에 필요한 헤더다.

```cpp
#include "../mappers/MemberMapper.h"
#include "../models/MemberTypes.h"
```

서비스는 DB 접근을 `MemberMapper`에 맡기고, DTO 타입은 `MemberTypes.h`에서 가져온다.

### public 함수

```cpp
AuthResultDTO signup(const SignupRequestDTO &request);
```

회원가입 요청을 검증하고 회원을 생성한다.

```cpp
AuthResultDTO login(const LoginRequestDTO &request);
```

로그인 요청을 검증하고 세션을 만든다.

```cpp
std::optional<MemberDTO> getCurrentMember(const std::string &sessionToken);
```

세션 토큰으로 현재 로그인한 회원을 찾는다.

```cpp
bool logout(const std::string &sessionToken);
```

세션 토큰을 삭제한다.

### SessionData

```cpp
struct SessionData
{
    std::uint64_t memberId{0};
    std::chrono::system_clock::time_point expiresAt;
};
```

서버 메모리에 저장되는 세션 정보다.

- `memberId`: 이 세션이 가리키는 회원 ID다.
- `expiresAt`: 세션 만료 시각이다.

현재 구조는 DB 세션 테이블이 아니라 `MemberService` 객체 안의 메모리 맵에 세션을 저장한다. 서버를 재시작하면 기존 로그인 세션은 사라진다.

### private helper

```cpp
static std::string trim(std::string value);
static std::string toLower(std::string value);
static bool isValidEmail(const std::string &email);
```

입력값 정규화와 이메일 형식 검증에 쓰인다.

```cpp
static std::string randomHex(std::size_t length);
static std::string hashPasswordWithSalt(const std::string &password);
static bool verifyPassword(...);
```

세션 토큰 생성, 비밀번호 저장용 해시 생성, 비밀번호 검증에 쓰인다.

```cpp
std::string createSession(std::uint64_t memberId);
std::optional<std::uint64_t> resolveSessionMemberId(...);
void cleanupExpiredSessionsLocked(...);
```

메모리 세션 생성, 조회, 만료 정리에 쓰인다.

```cpp
MemberMapper mapper_;
std::unordered_map<std::string, SessionData> sessions_;
std::mutex sessionsMutex_;
```

- `mapper_`: DB 작업 담당 객체다.
- `sessions_`: 세션 토큰을 키로 하는 메모리 세션 저장소다.
- `sessionsMutex_`: 여러 요청이 동시에 세션 맵을 건드릴 때 데이터 충돌을 막는다.

## services/MemberService.cpp

이 파일은 로그인/회원가입의 핵심 규칙을 담고 있다.

### 파일 상단

```cpp
constexpr int kSessionDurationSeconds = 60 * 60 * 24;
```

세션 유지 시간이다. 현재는 24시간이다.

```cpp
std::string makeGenericLoginErrorMessage()
{
    return "Invalid email or password.";
}
```

로그인 실패 메시지를 하나로 통일한다. 이메일이 없는지, 비밀번호가 틀렸는지, 계정 상태가 문제인지 외부에 구분해서 알려주지 않는다.

```cpp
bool looksLikeDuplicateEmail(const std::string &message)
```

DB 예외 메시지가 이메일 중복 오류인지 추정한다.

```cpp
lowered.find("duplicate") != std::string::npos ||
lowered.find("uq_members_email") != std::string::npos;
```

DB 예외 문자열에 `duplicate` 또는 유니크 키 이름 `uq_members_email`이 있으면 이메일 중복으로 판단한다.

### signup

```cpp
AuthResultDTO result;
```

서비스 결과 객체를 만든다. 기본값은 실패 상태다.

```cpp
const auto name = trim(request.name);
const auto email = toLower(trim(request.email));
const auto password = request.password;
const auto confirmPassword = request.confirmPassword;
```

입력값을 정리한다.

- 이름은 앞뒤 공백을 제거한다.
- 이메일은 앞뒤 공백 제거 후 소문자로 바꾼다.
- 비밀번호는 원문 그대로 사용한다. 비밀번호 안의 공백은 사용자가 의도했을 수 있으므로 `trim`하지 않는다.

```cpp
if (name.empty() || email.empty() || password.empty() || confirmPassword.empty())
```

필수 입력값이 비어 있는지 검사한다.

```cpp
result.statusCode = 400;
result.message = "Please fill in all required fields.";
return result;
```

필수값 누락은 클라이언트 요청 오류이므로 `400`을 반환한다.

```cpp
if (!request.agreeTerms)
```

약관 동의 여부를 확인한다.

```cpp
if (!isValidEmail(email))
```

이메일에 `@`와 도메인 점이 있는지 최소 형식 검사를 한다.

```cpp
if (password.size() < 8U)
```

비밀번호 최소 길이를 검사한다. 현재 기준은 8자다.

```cpp
if (password != confirmPassword)
```

비밀번호와 확인 입력값이 같은지 검사한다.

```cpp
if (mapper_.findByEmail(email).has_value())
```

DB에서 같은 이메일 회원이 이미 있는지 조회한다.

```cpp
result.statusCode = 409;
result.message = "Email is already registered.";
return result;
```

이미 가입된 이메일이면 충돌 상태인 `409 Conflict`를 반환한다.

```cpp
const auto passwordHash = hashPasswordWithSalt(password);
```

비밀번호 원문을 저장하지 않고 salt가 붙은 해시 문자열로 바꾼다.

```cpp
auto createdMember = mapper_.createMember(email, passwordHash, name);
```

DB에 새 회원을 삽입하고, 생성된 회원을 다시 조회한다.

```cpp
if (!createdMember.has_value())
```

삽입 후 회원 조회가 실패하면 서버 내부 오류로 처리한다.

```cpp
mapper_.updateLastLoginAt(createdMember->memberId);
```

회원가입 성공 후 자동 로그인으로 간주하므로 마지막 로그인 시간을 현재 시각으로 업데이트한다.

```cpp
createdMember = mapper_.findById(createdMember->memberId);
```

업데이트된 `last_login_at` 값을 포함한 최신 회원 정보를 다시 읽는다.

```cpp
const auto sessionToken = createSession(createdMember->memberId);
```

회원 ID로 서버 메모리 세션을 만들고 세션 토큰을 얻는다.

```cpp
result.ok = true;
result.statusCode = 201;
result.message = "Signup success.";
result.member = mapper_.toMemberDTO(*createdMember);
result.sessionToken = sessionToken;
return result;
```

성공 결과를 채운다.

- `201`: 새 회원이 생성되었기 때문이다.
- `member`: 비밀번호 해시가 제거된 `MemberDTO`다.
- `sessionToken`: 컨트롤러가 쿠키로 내려줄 값이다.

```cpp
catch (const drogon::orm::DrogonDbException &e)
```

DB 관련 예외를 잡는다.

```cpp
LOG_ERROR << "signup db error: " << e.base().what();
```

서버 로그에 DB 오류 내용을 남긴다.

```cpp
if (looksLikeDuplicateEmail(e.base().what()))
```

동시 요청 등으로 사전 중복 체크를 통과했지만 DB 유니크 키에서 막힌 경우를 처리한다.

```cpp
catch (const std::exception &)
```

그 외 서버 오류는 `500`으로 처리한다.

### login

```cpp
AuthResultDTO result;
```

로그인 결과 객체다.

```cpp
const auto email = toLower(trim(request.email));
const auto password = request.password;
```

이메일은 정규화하고 비밀번호는 원문 그대로 둔다.

```cpp
if (email.empty() || password.empty())
```

로그인 필수값을 확인한다.

```cpp
auto member = mapper_.findByEmail(email);
```

이메일로 회원을 DB에서 조회한다.

```cpp
if (!member.has_value())
```

회원이 없으면 로그인 실패다.

```cpp
result.statusCode = 401;
result.message = makeGenericLoginErrorMessage();
```

인증 실패이므로 `401`을 반환한다. 메시지는 일반화한다.

```cpp
if (member->status != "ACTIVE")
```

계정 상태가 활성 상태인지 확인한다.

```cpp
if (!verifyPassword(password, member->passwordHash))
```

사용자가 입력한 비밀번호와 DB에 저장된 해시를 비교한다.

```cpp
mapper_.updateLastLoginAt(member->memberId);
```

로그인 성공 시 마지막 로그인 시간을 갱신한다.

```cpp
member = mapper_.findById(member->memberId);
```

갱신된 회원 정보를 다시 조회한다.

```cpp
const auto sessionToken = createSession(member->memberId);
```

로그인 세션을 만든다.

```cpp
result.ok = true;
result.statusCode = 200;
result.message = "Login success.";
result.member = mapper_.toMemberDTO(*member);
result.sessionToken = sessionToken;
return result;
```

성공 결과를 반환한다. 컨트롤러는 이 결과를 JSON 응답과 쿠키로 바꾼다.

```cpp
catch (const std::exception &)
```

로그인 중 예외가 발생하면 `500 Server error during login.`을 반환한다.

### getCurrentMember

```cpp
const auto memberId = resolveSessionMemberId(sessionToken);
```

세션 토큰에서 회원 ID를 찾는다.

```cpp
if (!memberId.has_value())
{
    return std::nullopt;
}
```

세션이 없거나 만료되었으면 현재 사용자가 없다고 반환한다.

```cpp
const auto member = mapper_.findById(*memberId);
```

세션에 저장된 회원 ID로 DB에서 최신 회원 정보를 조회한다.

```cpp
if (!member.has_value() || member->status != "ACTIVE")
```

회원이 삭제되었거나 비활성 상태면 로그인된 사용자로 인정하지 않는다.

```cpp
return mapper_.toMemberDTO(*member);
```

응답 가능한 회원 DTO로 변환해서 반환한다.

### logout

```cpp
if (sessionToken.empty())
{
    return false;
}
```

토큰이 없으면 삭제할 세션도 없다.

```cpp
std::lock_guard<std::mutex> lock(sessionsMutex_);
return sessions_.erase(sessionToken) > 0;
```

세션 맵을 잠근 뒤 해당 토큰을 삭제한다. 삭제된 항목이 있으면 `true`다.

### trim

```cpp
const auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };
```

공백이 아닌 문자인지 판단하는 람다다.

```cpp
value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
```

문자열 앞쪽 공백을 제거한다.

```cpp
value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
```

문자열 뒤쪽 공백을 제거한다.

### toLower

```cpp
std::transform(value.begin(), value.end(), value.begin(), ...);
```

문자열 전체를 소문자로 바꾼다. 이메일 정규화와 중복 오류 메시지 판별에 쓰인다.

### isValidEmail

```cpp
const auto atPos = email.find('@');
```

`@` 위치를 찾는다.

```cpp
if (atPos == std::string::npos || atPos == 0 || atPos + 1 >= email.size())
```

`@`가 없거나 맨 앞에 있거나 맨 뒤에 가까우면 잘못된 이메일로 본다.

```cpp
const auto dotPos = email.find('.', atPos + 1);
return dotPos != std::string::npos && dotPos + 1 < email.size();
```

`@` 뒤에 점이 있고, 점 뒤에도 문자가 있어야 유효하다고 본다.

### randomHex

```cpp
static constexpr std::array<char, 16> kHexChars{...};
```

16진수 문자 목록이다.

```cpp
std::random_device rd;
std::mt19937 gen(rd());
std::uniform_int_distribution<int> dist(0, 15);
```

랜덤 숫자를 만들기 위한 객체다.

```cpp
for (std::size_t i = 0; i < length; ++i)
{
    out.push_back(kHexChars[...]);
}
```

요청 길이만큼 랜덤 16진수 문자열을 만든다. salt와 세션 토큰 생성에 쓰인다.

### hashPasswordWithSalt

```cpp
const auto salt = randomHex(16);
```

비밀번호마다 다른 salt를 만든다.

```cpp
const auto hash =
    std::to_string(std::hash<std::string>{}(salt + "::" + password));
```

salt와 비밀번호를 합쳐 해시한다.

```cpp
return salt + "$" + hash;
```

DB에는 `salt$hash` 형식으로 저장한다.

주의할 점: `std::hash`는 학습/개발용으로는 단순하지만, 실제 서비스의 비밀번호 저장에는 bcrypt, Argon2, scrypt 같은 전용 password hashing 알고리즘을 써야 한다.

### verifyPassword

```cpp
const auto separatorPos = storedHash.find('$');
```

저장된 해시에서 salt와 hash를 나누는 `$` 위치를 찾는다.

```cpp
if (separatorPos == std::string::npos)
{
    return storedHash == password;
}
```

기존에 평문 비밀번호가 저장된 데이터가 있을 경우를 위한 호환 처리다.

```cpp
const auto salt = storedHash.substr(0, separatorPos);
const auto expectedHash = storedHash.substr(separatorPos + 1);
```

저장된 salt와 기대 hash를 분리한다.

```cpp
const auto actualHash =
    std::to_string(std::hash<std::string>{}(salt + "::" + password));
return expectedHash == actualHash;
```

입력 비밀번호를 같은 방식으로 해시한 뒤 저장값과 비교한다.

### createSession

```cpp
const auto now = std::chrono::system_clock::now();
const auto expiresAt = now + std::chrono::seconds(kSessionDurationSeconds);
```

현재 시각과 세션 만료 시각을 계산한다.

```cpp
const auto sessionToken = randomHex(48);
```

48자리 랜덤 16진수 세션 토큰을 만든다.

```cpp
std::lock_guard<std::mutex> lock(sessionsMutex_);
```

세션 맵을 수정하기 전에 mutex를 잠근다.

```cpp
cleanupExpiredSessionsLocked(now);
```

새 세션을 넣기 전에 만료된 세션을 정리한다.

```cpp
sessions_[sessionToken] = SessionData{memberId, expiresAt};
```

세션 토큰을 키로 회원 ID와 만료 시간을 저장한다.

```cpp
return sessionToken;
```

컨트롤러가 쿠키로 내려줄 토큰을 반환한다.

### resolveSessionMemberId

```cpp
if (sessionToken.empty())
{
    return std::nullopt;
}
```

빈 토큰은 유효하지 않다.

```cpp
const auto now = std::chrono::system_clock::now();
```

현재 시간을 구한다.

```cpp
std::lock_guard<std::mutex> lock(sessionsMutex_);
cleanupExpiredSessionsLocked(now);
```

세션 맵 접근을 잠그고 만료 세션을 정리한다.

```cpp
const auto it = sessions_.find(sessionToken);
if (it == sessions_.end())
{
    return std::nullopt;
}
```

토큰이 저장소에 없으면 유효하지 않다.

```cpp
if (it->second.expiresAt <= now)
{
    sessions_.erase(it);
    return std::nullopt;
}
```

토큰이 만료되었으면 삭제하고 실패로 처리한다.

```cpp
return it->second.memberId;
```

유효한 세션이면 회원 ID를 반환한다.

### cleanupExpiredSessionsLocked

```cpp
for (auto it = sessions_.begin(); it != sessions_.end();)
```

세션 저장소 전체를 순회한다.

```cpp
if (it->second.expiresAt <= now)
{
    it = sessions_.erase(it);
    continue;
}
```

만료된 세션은 삭제한다.

```cpp
++it;
```

만료되지 않은 세션은 다음 항목으로 넘어간다.

함수 이름의 `Locked`는 호출자가 이미 `sessionsMutex_`를 잠근 상태에서 호출해야 한다는 의미다.

## mappers/MemberMapper.h

Mapper는 DB 접근 계층이다. 서비스는 SQL을 직접 알지 않고 Mapper 메서드를 호출한다.

```cpp
std::optional<MemberModel> findByEmail(const std::string &email) const;
```

이메일로 회원 한 명을 찾는다. 로그인과 회원가입 중복 검사에서 쓰인다.

```cpp
std::optional<MemberModel> findById(std::uint64_t memberId) const;
```

회원 ID로 회원 한 명을 찾는다. 로그인 후 최신 회원 정보 조회와 세션 사용자 조회에 쓰인다.

```cpp
std::optional<MemberModel> createMember(...);
```

새 회원을 DB에 삽입한다.

```cpp
void updateLastLoginAt(std::uint64_t memberId) const;
```

마지막 로그인 시간을 현재 시각으로 업데이트한다.

```cpp
MemberDTO toMemberDTO(const MemberModel &member) const;
```

내부 DB 모델을 응답 가능한 DTO로 변환한다.

```cpp
void ensureMembersTable() const;
```

로컬 개발 환경에서 `members` 테이블이 없으면 생성한다.

```cpp
querySingleMember(...)
querySingleMemberById(...)
```

공통 단건 조회 로직이다.

## mappers/MemberMapper.cpp

이 파일은 실제 SQL을 실행한다.

### rowToMemberModel

```cpp
auth::MemberModel member;
```

DB row를 담을 모델 객체를 만든다.

```cpp
member.memberId = row["member_id"].as<std::uint64_t>();
member.email = row["email"].as<std::string>();
member.passwordHash = row["password_hash"].as<std::string>();
...
```

SQL 조회 결과의 각 컬럼을 `MemberModel` 필드로 옮긴다.

```cpp
const auto lastLoginAt = row["last_login_at"].as<std::string>();
if (!lastLoginAt.empty())
{
    member.lastLoginAt = lastLoginAt;
}
```

`last_login_at`이 없으면 빈 문자열로 오기 때문에, 값이 있을 때만 `optional`에 넣는다.

### findByEmail

```cpp
ensureMembersTable();
```

조회 전에 테이블 존재를 보장한다.

```cpp
static const std::string sql =
    "SELECT member_id, email, password_hash, name, role, status, level, exp, "
    "COALESCE(DATE_FORMAT(last_login_at, '%Y-%m-%d %H:%i:%s'), '') AS last_login_at "
    "FROM members WHERE email = ? LIMIT 1";
```

이메일로 회원을 찾는 SQL이다.

- `password_hash`를 함께 조회한다. 로그인 비밀번호 검증에 필요하다.
- `COALESCE(..., '')`는 `last_login_at`이 `NULL`일 때 빈 문자열로 바꾼다.
- `?`는 바인딩 파라미터다. 문자열을 직접 이어 붙이지 않아서 SQL injection 위험을 줄인다.
- `LIMIT 1`은 이메일 유니크 조건에 맞춰 한 명만 가져온다.

```cpp
return querySingleMember(sql, email);
```

공통 단건 조회 함수로 실행한다.

### findById

`findByEmail`과 거의 같지만 조건이 다르다.

```cpp
"FROM members WHERE member_id = ? LIMIT 1";
```

회원 ID 기준으로 조회한다.

```cpp
return querySingleMemberById(sql, memberId);
```

`uint64_t` ID를 바인딩해서 단건 조회한다.

### createMember

```cpp
ensureMembersTable();
```

삽입 전에 테이블 존재를 보장한다.

```cpp
const auto dbClient = drogon::app().getDbClient("default");
```

설정 파일의 `default` DB 클라이언트를 가져온다.

```cpp
dbClient->execSqlSync(
    "INSERT INTO members (email, password_hash, name) VALUES (?, ?, ?)",
    email,
    passwordHash,
    name);
```

새 회원을 삽입한다.

- `member_id`는 auto increment라 직접 넣지 않는다.
- `role`, `status`, `level`, `exp`는 DB 기본값을 사용한다.
- 이메일, 비밀번호 해시, 이름만 필수로 넣는다.

```cpp
return findByEmail(email);
```

삽입 후 생성된 회원을 이메일로 다시 조회해서 반환한다.

### updateLastLoginAt

```cpp
dbClient->execSqlSync(
    "UPDATE members SET last_login_at = NOW() WHERE member_id = ?",
    memberId);
```

해당 회원의 마지막 로그인 시간을 DB 현재 시각으로 업데이트한다.

회원가입 성공 후 자동 로그인, 로그인 성공 후 모두 이 함수를 호출한다.

### toMemberDTO

```cpp
MemberDTO dto;
dto.memberId = member.memberId;
dto.email = member.email;
dto.name = member.name;
dto.role = member.role;
dto.status = member.status;
dto.level = member.level;
dto.exp = member.exp;
dto.lastLoginAt = member.lastLoginAt;
return dto;
```

`MemberModel`에서 클라이언트에 내려도 되는 필드만 복사한다.

`passwordHash`는 복사하지 않는다. 이 함수가 내부 모델과 외부 응답 모델을 나누는 핵심 지점이다.

### ensureMembersTable

```cpp
static std::atomic_bool tableReady{false};
if (tableReady.load())
{
    return;
}
```

테이블 생성 SQL을 매 요청마다 실행하지 않기 위한 플래그다.

```cpp
CREATE TABLE IF NOT EXISTS members (...)
```

`members` 테이블이 없으면 만든다.

주요 컬럼은 다음과 같다.

- `member_id`: 회원 기본키, 자동 증가
- `email`: 로그인 ID로 쓰는 이메일
- `password_hash`: 해시된 비밀번호
- `name`: 사용자 이름
- `role`: 권한, 기본값 `USER`
- `status`: 계정 상태, 기본값 `ACTIVE`
- `level`: 사용자 레벨
- `exp`: 경험치
- `last_login_at`: 마지막 로그인 시각
- `created_at`: 생성 시각
- `updated_at`: 수정 시각

```cpp
PRIMARY KEY (member_id)
```

회원 ID를 기본키로 지정한다.

```cpp
UNIQUE KEY uq_members_email (email)
```

같은 이메일로 중복 가입할 수 없게 한다.

```cpp
KEY idx_members_status (status)
```

상태별 조회가 필요해질 때를 위한 인덱스다.

```cpp
tableReady.store(true);
```

테이블 준비가 끝났음을 표시한다.

### querySingleMember

```cpp
const auto dbClient = drogon::app().getDbClient("default");
const auto result = dbClient->execSqlSync(sql, value);
```

문자열 파라미터 하나를 바인딩해서 SQL을 실행한다.

```cpp
if (result.empty())
{
    return std::nullopt;
}
```

조회 결과가 없으면 빈 optional을 반환한다.

```cpp
return rowToMemberModel(result[0]);
```

첫 번째 row를 `MemberModel`로 변환해서 반환한다.

### querySingleMemberById

`querySingleMember`와 같은 역할이지만, 바인딩 값이 `std::uint64_t memberId`다.

## 회원가입 전체 흐름

```text
1. frontend/html-version/index.html에서 회원가입 form submit
2. frontend/html-version/js/app.js가 POST /api/auth/signup 호출
3. MemberController::handleSignup
4. JSON body를 SignupRequestDTO로 변환
5. MemberService::signup 호출
6. 필수값, 약관 동의, 이메일 형식, 비밀번호 길이, 비밀번호 확인 검사
7. MemberMapper::findByEmail로 중복 이메일 확인
8. MemberService::hashPasswordWithSalt로 비밀번호 해시 생성
9. MemberMapper::createMember로 members 테이블 INSERT
10. MemberMapper::updateLastLoginAt로 자동 로그인 시간 기록
11. MemberService::createSession으로 서버 메모리 세션 생성
12. AuthResultDTO에 member와 sessionToken을 담아 반환
13. MemberController가 JSON 응답 생성
14. Set-Cookie: isbox_session=... 헤더 추가
15. 프론트엔드는 회원가입 성공 응답과 쿠키를 받음
```

## 로그인 전체 흐름

```text
1. frontend/html-version/index.html에서 로그인 form submit
2. frontend/html-version/js/app.js가 POST /api/auth/login 호출
3. MemberController::handleLogin
4. JSON body를 LoginRequestDTO로 변환
5. MemberService::login 호출
6. 이메일/비밀번호 필수값 검사
7. MemberMapper::findByEmail로 회원 조회
8. 계정 status가 ACTIVE인지 확인
9. MemberService::verifyPassword로 비밀번호 검증
10. MemberMapper::updateLastLoginAt로 마지막 로그인 시간 갱신
11. MemberService::createSession으로 서버 메모리 세션 생성
12. AuthResultDTO에 member와 sessionToken을 담아 반환
13. MemberController가 JSON 응답 생성
14. Set-Cookie: isbox_session=... 헤더 추가
15. 프론트엔드는 로그인 성공 응답과 쿠키를 받음
```

## 현재 구현에서 알아야 할 점

현재 세션은 DB가 아니라 `MemberService`의 `sessions_` 메모리 맵에 저장된다. 서버 프로세스를 재시작하면 기존 세션은 사라진다.

비밀번호는 salt를 붙여 해시하지만 `std::hash` 기반이다. 실제 운영 수준의 보안이 필요하면 bcrypt, Argon2, scrypt 같은 전용 password hashing 알고리즘으로 바꾸는 것이 맞다.

`MemberModel`은 DB 내부 모델이고 `MemberDTO`는 응답용 모델이다. `passwordHash`를 응답에서 제거하기 위해 두 타입을 분리해 둔 구조다.

회원가입과 로그인은 둘 다 성공 시 자동으로 세션을 만들고 `isbox_session` 쿠키를 내려준다. 그래서 가입 직후에도 바로 로그인된 상태가 된다.
