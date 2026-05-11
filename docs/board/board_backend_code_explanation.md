# Board 백엔드 코드 상세 설명

이 문서는 `backend`의 `app`, `controllers`, `services`, `mappers`, `models` 계층에서 Board 기능이 어떻게 동작하는지 코드 기준으로 설명한다.

설명 대상 파일:

- `backend/app/main.cpp`
- `backend/controllers/BoardController.h`
- `backend/controllers/BoardController.cpp`
- `backend/services/BoardService.h`
- `backend/services/BoardService.cpp`
- `backend/mappers/BoardMapper.h`
- `backend/mappers/BoardMapper.cpp`
- `backend/models/BoardTypes.h`
- `backend/models/BoardPostModel.h`

## 전체 요청 흐름

Board 기능은 두 개의 API를 중심으로 동작한다.

```http
GET /api/board/posts
GET /api/board/posts?type=share
GET /api/board/posts?type=exchange
POST /api/board/posts
```

목록 조회 흐름:

```text
브라우저
  -> GET /api/board/posts
  -> BoardController::handleListPosts
  -> BoardService::getPosts
  -> BoardMapper::findPosts
  -> board_posts + members 테이블 SELECT
  -> BoardPostDTO 목록
  -> JSON 응답
```

게시글 작성 흐름:

```text
브라우저
  -> POST /api/board/posts
  -> X-Member-Id 헤더 확인
  -> JSON body 파싱
  -> BoardController::handleCreatePost
  -> BoardService::createPost
  -> BoardMapper::insertPost
  -> BoardPostModel로 INSERT
  -> 생성된 post_id로 다시 SELECT
  -> BoardPostDTO
  -> JSON 응답
```

계층별 책임:

- `app`: Drogon 앱 초기화, DB 설정 로드, CORS preflight 처리, Board 라우터 등록.
- `controllers`: HTTP 요청/응답 처리, 헤더와 JSON body 파싱, DTO를 JSON으로 변환.
- `services`: 입력값 검증, 타입 정규화, 비즈니스 규칙 적용, 에러 메시지 결정.
- `mappers`: SQL 실행, DB row와 DTO 변환, 로컬 개발용 테이블 생성.
- `models`: DB INSERT에 필요한 ORM 모델과 API 계층에서 주고받는 DTO 정의.

## `app/main.cpp`

### include 영역

```cpp
#include <drogon/drogon.h>
```

Drogon 웹 프레임워크 전체 기능을 사용하기 위한 헤더다. `drogon::app()`, `registerHandler`, `HttpResponse` 같은 서버 기능이 여기서 제공된다.

```cpp
#include <cstdlib>
#include <filesystem>
#include <vector>
```

Windows 환경 변수 설정, 파일 경로 탐색, 후보 경로 목록 저장에 사용된다. Board 전용 코드는 아니지만 DB 연결을 정상화하는 데 필요하다.

```cpp
#include "../controllers/MemberController.h"
#include "../controllers/BoardController.h"
```

컨트롤러 계층을 앱 진입점에 연결한다. Board 기능은 `BoardController`를 include하고 `main()`에서 인스턴스를 만든 뒤 라우터를 등록해야 실제 HTTP API로 노출된다.

### 익명 namespace

```cpp
namespace
{
```

이 파일 내부에서만 쓰는 helper 함수를 숨기는 C++ 패턴이다. `configureMariaDbRuntimeForWindows`, `registerApiCorsPreflight`는 다른 파일에서 호출할 필요가 없으므로 익명 namespace 안에 둔다.

### `configureMariaDbRuntimeForWindows`

```cpp
void configureMariaDbRuntimeForWindows()
{
#ifdef _WIN32
```

Windows에서만 MariaDB 런타임 설정을 적용한다. Linux나 macOS 빌드에서는 이 블록이 컴파일되지 않는다.

```cpp
    _putenv_s("MARIADB_TLS_DISABLE_PEER_VERIFICATION", "1");
```

MariaDB TLS 인증서 검증을 비활성화한다. 로컬 개발 환경에서 인증서 문제로 DB 연결이 실패하는 것을 피하기 위한 설정이다.

```cpp
    const std::filesystem::path cwd = std::filesystem::current_path();
```

현재 실행 디렉터리를 구한다. 이후 MariaDB 플러그인 DLL 위치 후보를 만들 때 기준 경로로 사용한다.

```cpp
    const std::vector<std::filesystem::path> candidatePluginDirs = {
        cwd / "build" / "vcpkg_installed" / "x64-windows" / "plugins" /
            "libmariadb",
        cwd / "vcpkg_installed" / "x64-windows" / "plugins" / "libmariadb",
        cwd.parent_path() / "vcpkg_installed" / "x64-windows" / "plugins" /
            "libmariadb",
    };
```

MariaDB 인증 플러그인이 있을 만한 경로를 여러 개 만든다. 빌드 디렉터리에서 실행하는 경우와 프로젝트 루트 근처에서 실행하는 경우를 모두 고려한다.

```cpp
    for (const auto &dir : candidatePluginDirs)
```

후보 디렉터리를 하나씩 검사한다.

```cpp
        const auto pluginFile = dir / "caching_sha2_password.dll";
```

MariaDB/MySQL 인증에 필요한 `caching_sha2_password.dll` 파일 경로를 만든다.

```cpp
        if (std::filesystem::exists(pluginFile))
```

해당 DLL이 실제로 존재하는지 확인한다.

```cpp
            _putenv_s("MARIADB_PLUGIN_DIR", dir.string().c_str());
            break;
```

플러그인을 찾으면 `MARIADB_PLUGIN_DIR` 환경 변수에 디렉터리를 설정하고 탐색을 중단한다. 이 설정이 없으면 DB 연결 중 인증 플러그인을 찾지 못할 수 있다.

### `registerApiCorsPreflight`

```cpp
void registerApiCorsPreflight()
{
    drogon::app().registerPreRoutingAdvice(
```

모든 라우팅 전에 실행되는 Drogon advice를 등록한다. Board API의 `OPTIONS` preflight 요청을 컨트롤러까지 보내지 않고 여기서 바로 처리한다.

```cpp
        [](const drogon::HttpRequestPtr &request,
           drogon::AdviceCallback &&adviceCallback,
           drogon::AdviceChainCallback &&adviceChainCallback) {
```

람다 함수가 모든 요청을 먼저 받는다. `adviceCallback`은 여기서 응답을 끝낼 때 사용하고, `adviceChainCallback`은 다음 라우팅 단계로 넘길 때 사용한다.

```cpp
            if (request->getMethod() == drogon::Options &&
                request->path().rfind("/api/", 0) == 0)
```

HTTP 메서드가 `OPTIONS`이고 경로가 `/api/`로 시작하는 요청만 CORS preflight로 처리한다. Board API인 `/api/board/posts`도 이 조건에 포함된다.

```cpp
                auto response = drogon::HttpResponse::newHttpResponse();
                response->setStatusCode(drogon::k204NoContent);
```

본문 없는 204 응답을 만든다. preflight는 실제 데이터를 내려주는 요청이 아니므로 body가 필요 없다.

```cpp
                auto origin = request->getHeader("Origin");
                if (origin.empty())
                {
                    origin = request->getHeader("origin");
                }
```

브라우저가 보낸 `Origin` 헤더를 읽는다. 대소문자 차이를 방어하기 위해 `origin`도 한 번 더 확인한다.

```cpp
                if (!origin.empty())
                {
                    response->addHeader("Access-Control-Allow-Origin", origin);
                    response->addHeader("Vary", "Origin");
                }
```

Origin이 있으면 같은 값을 허용 origin으로 돌려준다. `Vary: Origin`은 캐시가 Origin별로 응답을 구분해야 함을 알려준다.

```cpp
                response->addHeader("Access-Control-Allow-Credentials", "true");
```

인증 정보가 포함된 요청을 허용한다. 현재 Board 작성은 쿠키가 아니라 `X-Member-Id` 헤더를 쓰지만, API 전체 CORS 정책으로 credentials를 열어 둔다.

```cpp
                response->addHeader("Access-Control-Allow-Headers",
                                    "Content-Type, X-Member-Id");
```

브라우저가 `Content-Type`과 `X-Member-Id` 헤더를 보낼 수 있도록 허용한다. Board 작성 요청은 `X-Member-Id`를 사용하므로 이 값이 필요하다.

```cpp
                response->addHeader("Access-Control-Allow-Methods",
                                    "GET, POST, OPTIONS");
```

Board API에서 사용하는 `GET`, `POST`와 preflight용 `OPTIONS`를 허용한다.

```cpp
                adviceCallback(response);
                return;
```

preflight 응답을 즉시 반환하고 라우터로 내려가지 않는다.

```cpp
            adviceChainCallback();
```

preflight 조건이 아니면 정상 라우팅을 계속 진행한다.

### `main`

```cpp
int main()
{
```

백엔드 실행 시작점이다.

```cpp
    configureMariaDbRuntimeForWindows();
```

Drogon 앱 시작 전에 MariaDB 런타임 환경을 준비한다. Board mapper는 DB를 사용하므로 이 설정이 먼저 끝나야 한다.

```cpp
    drogon::app().loadConfigFile("config/config.json");
```

DB client, listener 등 Drogon 설정을 `config/config.json`에서 읽는다. Board mapper의 `drogon::app().getDbClient("default")`는 이 설정에 등록된 DB client를 사용한다.

```cpp
    drogon::app().setDocumentRoot("../frontend");
```

정적 파일 루트를 `frontend`로 설정한다. Board API 자체와 직접 관련은 없지만, 프론트엔드 `html-version` 페이지를 같은 서버에서 제공한다.

```cpp
    registerApiCorsPreflight();
```

API CORS preflight 처리를 등록한다. Board API 호출 전에 브라우저가 보내는 `OPTIONS /api/board/posts`가 여기서 처리된다.

```cpp
    auth::MemberController memberController;
    memberController.registerHandlers();
```

회원 API 라우터를 등록한다. Board 작성은 member id를 사용하므로 회원 기능과 데이터상 연결된다.

```cpp
    board::BoardController boardController;
    boardController.registerHandlers();
```

Board 컨트롤러 객체를 만들고 `/api/board/posts` 라우터를 등록한다. 이 두 줄이 없으면 Board API는 HTTP 요청을 받을 수 없다.

```cpp
    drogon::app().registerHandler(
        "/",
```

루트 경로 요청을 등록한다.

```cpp
            auto response = drogon::HttpResponse::newRedirectionResponse(
                "/html-version/index.html");
```

브라우저가 `/`로 접근하면 `frontend/html-version/index.html`로 이동시킨다.

```cpp
    drogon::app().registerHandler(
        "/index.html",
```

`/index.html` 요청도 별도로 등록한다.

```cpp
            auto response = drogon::HttpResponse::newRedirectionResponse(
                "/html-version/index.html");
```

`/index.html`도 동일하게 `html-version`의 index로 이동시킨다.

```cpp
    drogon::app().run();
```

서버 이벤트 루프를 시작한다. 이 이후부터 Board API가 실제 요청을 처리한다.

## `controllers/BoardController.h`

이 파일은 Board HTTP 컨트롤러의 공개 인터페이스와 내부 helper 함수 목록을 선언한다.

```cpp
#pragma once
```

헤더가 한 번만 include되도록 한다.

```cpp
#include <drogon/HttpTypes.h>
#include <drogon/drogon.h>
```

Drogon 요청/응답 타입과 앱 API를 사용하기 위한 include다.

```cpp
#include <functional>
#include <optional>
#include <string>
```

콜백 타입, nullable 값, 문자열 처리를 위해 필요하다.

```cpp
#include "../services/BoardService.h"
```

컨트롤러가 비즈니스 로직을 직접 처리하지 않고 `BoardService`에 위임하기 위해 include한다.

```cpp
namespace board
{
```

Board 관련 타입을 `board` namespace에 묶는다.

```cpp
class BoardController
{
```

Board API의 HTTP 계층을 담당하는 클래스다.

```cpp
  public:
    BoardController() = default;
```

기본 생성자를 사용한다. 내부 멤버인 `BoardService boardService_`도 기본 생성된다.

```cpp
    void registerHandlers();
```

라우터 등록 함수다. `main.cpp`에서 호출되며, `GET /api/board/posts`와 `POST /api/board/posts`를 등록한다.

```cpp
  private:
    using Callback = std::function<void(const drogon::HttpResponsePtr &)>;
```

Drogon handler의 응답 콜백 타입을 짧게 부르는 alias다. 각 handler는 응답 객체를 만들어 이 callback에 넘긴다.

```cpp
    static Json::Value buildPostJson(const BoardPostDTO &post);
```

서비스/매퍼에서 받은 `BoardPostDTO`를 HTTP JSON 응답 형태로 바꾸는 helper다.

```cpp
    static void applyCors(const drogon::HttpRequestPtr &request,
                          const drogon::HttpResponsePtr &response);
```

실제 `GET`, `POST` 응답에도 CORS 헤더를 붙이는 helper다. preflight는 `main.cpp`에서 처리하고, 본 요청 응답은 여기서 처리한다.

```cpp
    static std::optional<std::uint64_t> extractMemberIdHeader(
        const drogon::HttpRequestPtr &request);
```

게시글 작성 시 `X-Member-Id` 헤더에서 회원 id를 꺼낸다. 없거나 잘못된 값이면 `std::nullopt`를 반환한다.

```cpp
    void handleListPosts(const drogon::HttpRequestPtr &request, Callback &&callback);
```

게시글 목록 조회 요청을 처리한다.

```cpp
    void handleCreatePost(const drogon::HttpRequestPtr &request, Callback &&callback);
```

게시글 작성 요청을 처리한다.

```cpp
    BoardService boardService_;
```

컨트롤러가 사용하는 서비스 객체다. HTTP 파싱 이후의 검증과 DB 작업은 이 객체로 넘긴다.

## `controllers/BoardController.cpp`

이 파일은 Board HTTP 요청을 실제로 처리한다.

### 파일 내부 error body helper

```cpp
namespace
{
```

이 cpp 파일에서만 쓰는 helper를 숨긴다.

```cpp
Json::Value makeErrorBody(const std::string &message)
{
    Json::Value body;
    body["ok"] = false;
    body["message"] = message;
    return body;
}
```

에러 응답 JSON을 만드는 함수다. Board API 에러는 공통적으로 `ok: false`, `message: "..."`
형태를 사용한다.

### `registerHandlers`

```cpp
void BoardController::registerHandlers()
{
    auto &app = drogon::app();
```

Drogon 전역 앱 객체를 가져온다. 라우터 등록을 간단히 쓰기 위해 참조로 저장한다.

```cpp
    app.registerHandler(
        "/api/board/posts",
```

`/api/board/posts` 경로를 등록한다.

```cpp
        [this](const drogon::HttpRequestPtr &request, Callback &&callback) {
            handleListPosts(request, std::move(callback));
        },
        {drogon::Get});
```

GET 요청은 `handleListPosts`로 보낸다. `this`를 캡처하기 때문에 컨트롤러의 멤버 함수와 `boardService_`에 접근할 수 있다. `std::move(callback)`은 콜백 소유권을 handler 함수로 넘긴다.

```cpp
    app.registerHandler(
        "/api/board/posts",
```

같은 경로를 다시 등록한다.

```cpp
        [this](const drogon::HttpRequestPtr &request, Callback &&callback) {
            handleCreatePost(request, std::move(callback));
        },
        {drogon::Post});
```

POST 요청은 `handleCreatePost`로 보낸다. 같은 URL이지만 HTTP method가 달라 목록 조회와 작성이 구분된다.

### `buildPostJson`

```cpp
Json::Value BoardController::buildPostJson(const BoardPostDTO &post)
{
    Json::Value postJson;
```

응답에 넣을 게시글 JSON 객체를 만든다.

```cpp
    postJson["postId"] = static_cast<Json::UInt64>(post.postId);
    postJson["memberId"] = static_cast<Json::UInt64>(post.memberId);
```

C++의 `std::uint64_t` 값을 JsonCpp가 이해하는 `Json::UInt64`로 변환해 넣는다.

```cpp
    postJson["type"] = post.type;
    postJson["title"] = post.title;
    postJson["content"] = post.content;
```

클라이언트가 화면에 표시할 게시글 유형, 제목, 내용을 넣는다. `type`은 mapper에서 `share` 또는 `exchange` 같은 클라이언트용 소문자 값으로 변환된 상태다.

```cpp
    if (post.location.has_value())
    {
        postJson["location"] = *post.location;
    }
```

위치가 있을 때만 `location` 필드를 응답에 포함한다. DB에는 빈 문자열로 들어갈 수 있지만 DTO에서는 optional로 표현한다.

```cpp
    postJson["authorName"] = post.authorName;
    postJson["status"] = post.status;
    postJson["createdAt"] = post.createdAt;
```

작성자 이름, 게시글 상태, 생성일을 응답에 넣는다.

```cpp
    return postJson;
```

완성된 게시글 JSON을 반환한다.

### `applyCors`

```cpp
void BoardController::applyCors(const drogon::HttpRequestPtr &request,
                                const drogon::HttpResponsePtr &response)
```

본 요청 응답에 CORS 헤더를 붙이는 함수다.

```cpp
    std::string origin = request->getHeader("Origin");
    if (origin.empty())
    {
        origin = request->getHeader("origin");
    }
```

요청의 Origin을 읽는다. 대소문자 차이를 방어한다.

```cpp
    if (!origin.empty())
    {
        response->addHeader("Access-Control-Allow-Origin", origin);
        response->addHeader("Vary", "Origin");
    }
```

Origin이 있으면 그 origin을 허용한다. 개발 중 프론트엔드 주소가 달라도 API 응답을 읽을 수 있게 한다.

```cpp
    response->addHeader("Access-Control-Allow-Credentials", "true");
    response->addHeader("Access-Control-Allow-Headers",
                        "Content-Type, X-Member-Id");
    response->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
```

브라우저가 Board API 호출에 필요한 헤더와 메서드를 사용할 수 있게 한다. 특히 게시글 작성에는 `X-Member-Id`가 필요하다.

### `extractMemberIdHeader`

```cpp
std::optional<std::uint64_t> BoardController::extractMemberIdHeader(
    const drogon::HttpRequestPtr &request)
```

회원 id 헤더를 읽고 숫자로 변환한다. 실패할 수 있으므로 optional을 반환한다.

```cpp
    auto memberIdValue = request->getHeader("X-Member-Id");
    if (memberIdValue.empty())
    {
        memberIdValue = request->getHeader("x-member-id");
    }
```

대문자 헤더 이름과 소문자 헤더 이름을 모두 확인한다.

```cpp
    if (memberIdValue.empty())
    {
        return std::nullopt;
    }
```

헤더가 없으면 인증되지 않은 요청으로 본다.

```cpp
    try
    {
        const auto parsed = std::stoull(memberIdValue);
```

문자열을 unsigned long long 숫자로 변환한다.

```cpp
        if (parsed == 0)
        {
            return std::nullopt;
        }
```

0은 유효한 회원 id로 보지 않는다.

```cpp
        return parsed;
```

유효한 숫자면 회원 id를 반환한다.

```cpp
    catch (const std::exception &)
    {
        return std::nullopt;
    }
```

숫자 변환에 실패하면 예외를 밖으로 던지지 않고 인증 실패 값으로 바꾼다.

### `handleListPosts`

```cpp
void BoardController::handleListPosts(const drogon::HttpRequestPtr &request,
                                      Callback &&callback)
```

게시글 목록 조회 API의 handler다.

```cpp
    const auto type = request->getParameter("type");
```

쿼리스트링의 `type` 값을 읽는다. 예시는 `/api/board/posts?type=share`다.

```cpp
    std::optional<std::string> filter;
    if (!type.empty())
    {
        filter = type;
    }
```

`type`이 있으면 optional 필터로 감싸고, 없으면 필터 없는 전체 조회로 둔다.

```cpp
    const auto result = boardService_.getPosts(filter);
```

서비스 계층에 목록 조회를 위임한다. 필터 검증과 DB 조회는 서비스와 mapper가 담당한다.

```cpp
    Json::Value body;
    body["ok"] = result.ok;
    body["message"] = result.message;
```

응답 body의 공통 필드를 채운다.

```cpp
    Json::Value posts(Json::arrayValue);
    for (const auto &post : result.posts)
    {
        posts.append(buildPostJson(post));
    }
    body["posts"] = posts;
```

서비스가 반환한 DTO 목록을 JSON 배열로 변환한다.

```cpp
    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
```

JSON 응답 객체를 만든다.

```cpp
    response->setStatusCode(static_cast<drogon::HttpStatusCode>(result.statusCode));
```

서비스가 정한 HTTP status code를 응답에 반영한다. 성공은 200, 필터 오류는 400, 서버 오류는 500이다.

```cpp
    applyCors(request, response);
    callback(response);
```

CORS 헤더를 붙인 뒤 Drogon callback으로 응답을 보낸다.

### `handleCreatePost`

```cpp
void BoardController::handleCreatePost(const drogon::HttpRequestPtr &request,
                                       Callback &&callback)
```

게시글 작성 API의 handler다.

```cpp
    const auto memberId = extractMemberIdHeader(request);
```

`X-Member-Id`에서 작성자 회원 id를 읽는다.

```cpp
    if (!memberId.has_value())
```

회원 id가 없거나 유효하지 않으면 작성 요청을 거절한다.

```cpp
        auto response =
            drogon::HttpResponse::newHttpJsonResponse(makeErrorBody("Unauthorized."));
        response->setStatusCode(drogon::k401Unauthorized);
```

401 Unauthorized 응답을 만든다.

```cpp
        applyCors(request, response);
        callback(response);
        return;
```

에러 응답을 보내고 함수를 종료한다.

```cpp
    const auto json = request->getJsonObject();
    if (!json)
```

요청 body를 JSON으로 파싱한다. JSON이 아니거나 파싱 실패하면 `nullptr`이 된다.

```cpp
        auto response =
            drogon::HttpResponse::newHttpJsonResponse(makeErrorBody("Invalid JSON body."));
        response->setStatusCode(drogon::k400BadRequest);
```

잘못된 body는 400 Bad Request로 응답한다.

```cpp
    CreateBoardPostRequestDTO dto;
    dto.type = json->get("type", "").asString();
    dto.title = json->get("title", "").asString();
    dto.content = json->get("content", "").asString();
```

JSON body에서 게시글 유형, 제목, 내용을 꺼내 DTO에 담는다. 값이 없으면 빈 문자열이 들어간다.

```cpp
    const auto location = json->get("location", "").asString();
    if (!location.empty())
    {
        dto.location = location;
    }
```

위치 값은 선택 항목이다. 빈 문자열이 아니면 optional에 넣고, 비어 있으면 값 없음으로 둔다.

```cpp
    const auto result = boardService_.createPost(*memberId, dto);
```

회원 id와 작성 요청 DTO를 서비스 계층에 넘긴다. 검증, 정규화, DB 저장은 서비스 아래에서 처리된다.

```cpp
    Json::Value body;
    body["ok"] = result.ok;
    body["message"] = result.message;
```

서비스 결과를 응답 body에 반영한다.

```cpp
    if (result.post.has_value())
    {
        body["post"] = buildPostJson(*result.post);
    }
```

생성 성공 시 생성된 게시글을 `post` 필드에 넣는다. 실패 시에는 `post`가 없다.

```cpp
    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(static_cast<drogon::HttpStatusCode>(result.statusCode));
    applyCors(request, response);
    callback(response);
```

JSON 응답을 만들고, 서비스가 정한 status code를 넣고, CORS 헤더를 붙인 뒤 응답한다.

## `services/BoardService.h`

이 파일은 Board 비즈니스 로직 계층의 인터페이스를 선언한다.

```cpp
#pragma once
```

헤더 중복 include를 막는다.

```cpp
#include <optional>
#include <string>
```

필터, 위치 같은 optional 값과 문자열 처리를 위해 사용한다.

```cpp
#include "../mappers/BoardMapper.h"
#include "../models/BoardTypes.h"
```

서비스는 DB 접근을 `BoardMapper`에 위임하고, 입출력 결과는 `BoardTypes.h`의 DTO를 사용한다.

```cpp
class BoardService
{
```

Board 도메인의 검증과 비즈니스 규칙을 담당하는 클래스다.

```cpp
    BoardListResultDTO getPosts(const std::optional<std::string> &typeFilter);
```

게시글 목록을 가져온다. `typeFilter`가 없으면 전체, 있으면 `share` 또는 `exchange` 필터로 조회한다.

```cpp
    BoardCreateResultDTO createPost(std::uint64_t memberId,
                                    const CreateBoardPostRequestDTO &request);
```

게시글을 생성한다. 컨트롤러에서 받은 회원 id와 요청 DTO를 검증하고 DB 저장으로 넘긴다.

```cpp
    static std::string trim(std::string value);
```

문자열 앞뒤 공백을 제거하는 helper다.

```cpp
    static std::string toLower(std::string value);
```

문자열을 소문자로 변환하는 helper다.

```cpp
    static bool isAllowedType(const std::string &value);
```

게시글 type이 허용된 값인지 검사한다.

```cpp
    static std::string toDbType(const std::string &value);
```

클라이언트 type인 `share`, `exchange`를 DB enum 값인 `SHARE`, `EXCHANGE`로 변환한다.

```cpp
    BoardMapper mapper_;
```

DB 접근 담당 객체다. 서비스는 SQL을 직접 실행하지 않고 mapper를 호출한다.

## `services/BoardService.cpp`

이 파일은 Board 비즈니스 규칙을 구현한다.

### include

```cpp
#include "BoardService.h"
```

자기 헤더를 include한다.

```cpp
#include <drogon/orm/Exception.h>
```

Drogon DB 예외 타입인 `DrogonDbException`을 잡기 위해 필요하다.

```cpp
#include <algorithm>
#include <cctype>
```

`std::find_if`, `std::transform`, `std::isspace`, `std::tolower`를 사용한다.

### `getPosts`

```cpp
BoardListResultDTO BoardService::getPosts(
    const std::optional<std::string> &typeFilter)
{
    BoardListResultDTO result;
```

목록 조회 결과 DTO를 만든다. 기본값은 `ok=false`, `statusCode=400`이다.

```cpp
    std::optional<std::string> dbTypeFilter;
```

DB 조회에 넘길 type 필터다. 클라이언트 값이 아닌 DB enum 값(`SHARE`, `EXCHANGE`)으로 들어간다.

```cpp
    if (typeFilter.has_value())
```

쿼리스트링에 type 필터가 있을 때만 검증한다.

```cpp
        const auto normalized = toLower(trim(*typeFilter));
```

필터 값의 앞뒤 공백을 제거하고 소문자로 만든다. `" SHARE "` 같은 입력도 `"share"`로 정규화된다.

```cpp
        if (!normalized.empty() && normalized != "all")
```

빈 문자열과 `all`은 전체 조회로 처리한다.

```cpp
            if (!isAllowedType(normalized))
            {
                result.statusCode = 400;
                result.message = "Invalid post type filter.";
                return result;
            }
```

`share`, `exchange`가 아니면 잘못된 필터로 보고 400 결과를 반환한다.

```cpp
            dbTypeFilter = toDbType(normalized);
```

허용된 클라이언트 type을 DB type으로 바꾼다.

```cpp
    try
    {
        result.posts = mapper_.findPosts(dbTypeFilter);
```

mapper에 DB 조회를 위임한다. DB 예외가 날 수 있으므로 try 안에서 실행한다.

```cpp
        result.ok = true;
        result.statusCode = 200;
        result.message = "Posts loaded.";
        return result;
```

조회 성공 결과를 만든다.

```cpp
    catch (const drogon::orm::DrogonDbException &)
```

Drogon ORM/DB 계층에서 발생한 예외를 잡는다.

```cpp
        result.statusCode = 500;
        result.message = "Database error while loading posts.";
        return result;
```

DB 오류를 500 응답으로 바꾼다.

```cpp
    catch (const std::exception &)
```

그 외 표준 예외를 잡는다.

```cpp
        result.statusCode = 500;
        result.message = "Server error while loading posts.";
        return result;
```

알 수 없는 서버 오류도 500으로 응답하게 한다.

### `createPost`

```cpp
BoardCreateResultDTO BoardService::createPost(
    std::uint64_t memberId,
    const CreateBoardPostRequestDTO &request)
{
    BoardCreateResultDTO result;
```

게시글 작성 결과 DTO를 만든다. 기본값은 실패 상태다.

```cpp
    if (memberId == 0)
```

회원 id가 0이면 유효하지 않은 작성자로 판단한다.

```cpp
        result.statusCode = 401;
        result.message = "Unauthorized.";
        return result;
```

인증 실패 결과를 반환한다. 컨트롤러에서도 헤더 검증을 하지만 서비스에서도 한 번 더 방어한다.

```cpp
    const auto normalizedType = toLower(trim(request.type));
    const auto normalizedTitle = trim(request.title);
    const auto normalizedContent = trim(request.content);
```

type은 공백 제거 후 소문자로 만들고, 제목과 내용은 앞뒤 공백을 제거한다.

```cpp
    std::optional<std::string> normalizedLocation;
    if (request.location.has_value())
```

위치 값은 선택 항목이므로 optional로 정규화한다.

```cpp
        const auto location = trim(*request.location);
        if (!location.empty())
        {
            normalizedLocation = location;
        }
```

위치 문자열의 앞뒤 공백을 제거하고, 비어 있지 않을 때만 값으로 인정한다.

```cpp
    if (!isAllowedType(normalizedType))
```

type이 허용된 값인지 검사한다.

```cpp
        result.statusCode = 400;
        result.message = "Type must be share or exchange.";
        return result;
```

잘못된 type은 400 에러로 반환한다.

```cpp
    if (normalizedTitle.empty())
```

제목이 비어 있는지 검사한다.

```cpp
        result.statusCode = 400;
        result.message = "Title is required.";
        return result;
```

제목은 필수 입력이다.

```cpp
    if (normalizedTitle.size() > 100U)
```

제목 길이를 100자 이하로 제한한다. DB 컬럼도 `VARCHAR(100)`이므로 서비스에서 먼저 막는다.

```cpp
        result.statusCode = 400;
        result.message = "Title must be 100 characters or fewer.";
        return result;
```

길이 초과 시 400을 반환한다.

```cpp
    if (normalizedContent.empty())
```

본문이 비어 있는지 검사한다.

```cpp
        result.statusCode = 400;
        result.message = "Content is required.";
        return result;
```

본문은 필수 입력이다.

```cpp
    if (normalizedLocation.has_value() && normalizedLocation->size() > 100U)
```

위치가 있으면 100자 이하인지 검사한다. DB 컬럼 `location VARCHAR(100)`과 맞춘다.

```cpp
        result.statusCode = 400;
        result.message = "Location must be 100 characters or fewer.";
        return result;
```

위치 길이 초과 시 400을 반환한다.

```cpp
    CreateBoardPostRequestDTO normalizedRequest;
    normalizedRequest.type = toDbType(normalizedType);
    normalizedRequest.title = normalizedTitle;
    normalizedRequest.content = normalizedContent;
    normalizedRequest.location = normalizedLocation;
```

DB 저장에 사용할 정규화된 요청 DTO를 만든다. type은 DB enum 값(`SHARE` 또는 `EXCHANGE`)으로 바꾼다.

```cpp
    try
    {
        const auto created = mapper_.insertPost(memberId, normalizedRequest);
```

mapper에 INSERT를 위임한다. DB 작업이므로 예외 처리를 위해 try 안에서 실행한다.

```cpp
        if (!created.has_value())
```

INSERT 후 생성된 게시글을 다시 조회하지 못한 경우를 검사한다.

```cpp
            result.statusCode = 500;
            result.message = "Failed to create post.";
            return result;
```

생성 결과가 없으면 서버 오류로 반환한다.

```cpp
        result.ok = true;
        result.statusCode = 201;
        result.message = "Post created.";
        result.post = created;
        return result;
```

생성 성공 결과를 만든다. HTTP 201 Created를 사용하고 생성된 게시글 DTO를 포함한다.

```cpp
    catch (const drogon::orm::DrogonDbException &)
```

DB 예외를 잡는다. 예를 들어 FK 제약 조건 실패, 연결 문제 등이 여기에 들어올 수 있다.

```cpp
        result.statusCode = 500;
        result.message = "Database error while creating post.";
        return result;
```

DB 오류를 500으로 반환한다.

```cpp
    catch (const std::exception &)
```

기타 표준 예외를 잡는다.

```cpp
        result.statusCode = 500;
        result.message = "Server error while creating post.";
        return result;
```

예상하지 못한 서버 오류를 500으로 반환한다.

### 문자열 helper

```cpp
std::string BoardService::trim(std::string value)
```

문자열 양쪽 공백을 제거한다.

```cpp
    const auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };
```

공백이 아닌 문자인지 검사하는 람다다. `unsigned char`를 쓰는 이유는 `std::isspace`에 음수 `char`가 들어가는 문제를 피하기 위해서다.

```cpp
    value.erase(value.begin(),
                std::find_if(value.begin(), value.end(), notSpace));
```

문자열 앞쪽 공백을 지운다.

```cpp
    value.erase(
        std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
```

문자열 뒤쪽 공백을 지운다.

```cpp
    return value;
```

정리된 문자열을 반환한다.

```cpp
std::string BoardService::toLower(std::string value)
```

문자열을 소문자로 변환한다.

```cpp
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
```

각 문자를 `std::tolower`로 변환해서 같은 문자열에 덮어쓴다.

```cpp
bool BoardService::isAllowedType(const std::string &value)
{
    return value == "share" || value == "exchange";
}
```

허용된 게시글 타입은 `share`, `exchange` 두 개뿐이다.

```cpp
std::string BoardService::toDbType(const std::string &value)
{
    if (value == "share")
    {
        return "SHARE";
    }
    return "EXCHANGE";
}
```

클라이언트용 소문자 타입을 DB enum 값으로 변환한다. `isAllowedType` 검증 이후 호출된다는 전제라서 `share`가 아니면 `EXCHANGE`로 처리한다.

## `mappers/BoardMapper.h`

이 파일은 Board DB 접근 계층의 인터페이스를 선언한다.

```cpp
#include <drogon/orm/Row.h>
```

DB SELECT 결과 row를 DTO로 변환할 때 사용한다.

```cpp
#include <optional>
#include <string>
#include <vector>
```

필터, 문자열, 게시글 목록을 표현하기 위해 사용한다.

```cpp
#include "../models/BoardTypes.h"
```

mapper가 반환하는 DTO 타입을 사용한다.

```cpp
class BoardMapper
{
```

Board 관련 SQL 실행과 row 변환을 담당하는 클래스다.

```cpp
    std::vector<BoardPostDTO> findPosts(
        const std::optional<std::string> &dbTypeFilter) const;
```

게시글 목록을 조회한다. `dbTypeFilter`가 있으면 `SHARE` 또는 `EXCHANGE`만 조회하고, 없으면 전체 조회한다.

```cpp
    std::optional<BoardPostDTO> findById(std::uint64_t postId) const;
```

게시글 id로 게시글 하나를 조회한다. 게시글 작성 후 생성된 데이터를 다시 읽을 때 사용한다.

```cpp
    std::optional<BoardPostDTO> insertPost(
        std::uint64_t memberId,
        const CreateBoardPostRequestDTO &request) const;
```

게시글을 DB에 삽입한다. 성공하면 생성된 게시글 DTO를 반환한다.

```cpp
    static BoardPostDTO rowToBoardPostDTO(const drogon::orm::Row &row);
```

SQL 결과 row를 API 응답용 DTO로 변환한다.

```cpp
    static std::string toClientType(const std::string &dbType);
```

DB enum 값 `SHARE`, `EXCHANGE`를 클라이언트 값 `share`, `exchange`로 변환한다.

```cpp
    static std::string toClientStatus(const std::string &dbStatus);
```

DB enum 값 `AVAILABLE`, `CLOSED`, `DELETED`를 클라이언트 값 `available`, `closed`, `deleted`로 변환한다.

```cpp
    void ensureTablesForLocalDev() const;
```

로컬 개발 중 필요한 테이블이 없으면 생성한다. 실제 운영 마이그레이션 도구 대신 개발 편의를 위해 mapper에 들어간 기능이다.

## `mappers/BoardMapper.cpp`

이 파일은 SQL과 DB row 변환을 구현한다.

### include와 내부 helper

```cpp
#include "BoardMapper.h"
```

mapper 선언을 가져온다.

```cpp
#include <drogon/drogon.h>
#include <drogon/orm/Mapper.h>
```

DB client 접근과 Drogon ORM mapper를 사용한다.

```cpp
#include <algorithm>
#include <atomic>
#include <cctype>
```

문자열 소문자 변환과 테이블 생성 1회 보장을 위해 사용한다.

```cpp
#include "../models/BoardPostModel.h"
```

INSERT 시 Drogon ORM mapper가 사용할 Board DB 모델을 가져온다.

```cpp
namespace
{
std::string toLowerCopy(std::string value)
```

이 cpp 파일 안에서만 쓰는 소문자 변환 helper다.

```cpp
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
```

문자열 전체를 소문자로 바꾼다.

### `findPosts`

```cpp
std::vector<BoardPostDTO> BoardMapper::findPosts(
    const std::optional<std::string> &dbTypeFilter) const
{
    ensureTablesForLocalDev();
```

게시글 목록을 조회하기 전에 로컬 개발용 테이블 존재를 보장한다.

```cpp
    const auto dbClient = drogon::app().getDbClient("default");
```

Drogon 설정에 등록된 `default` DB client를 가져온다.

```cpp
    static const std::string baseSql =
        "SELECT "
        "p.post_id, p.member_id, p.post_type, p.title, p.content, "
        "COALESCE(p.location, '') AS location, p.status, "
        "COALESCE(DATE_FORMAT(p.created_at, '%Y-%m-%d %H:%i:%s'), '') AS created_at, "
        "m.name AS author_name "
        "FROM board_posts p "
        "JOIN members m ON m.member_id = p.member_id "
        "WHERE p.status <> 'DELETED' ";
```

게시글 목록 조회 기본 SQL이다.

- `board_posts p`에서 게시글 정보를 읽는다.
- `members m`과 join해서 작성자 이름을 가져온다.
- `COALESCE(p.location, '')`는 DB의 NULL 위치 값을 빈 문자열로 바꾼다.
- `DATE_FORMAT`은 DB 시간을 문자열로 포맷한다.
- `p.status <> 'DELETED'`로 삭제 상태 게시글은 목록에서 제외한다.

```cpp
    static const std::string orderSql = "ORDER BY p.created_at DESC LIMIT 50";
```

최신 글부터 최대 50개만 가져온다.

```cpp
    drogon::orm::Result result;
```

SQL 실행 결과를 담을 객체다.

```cpp
    if (dbTypeFilter.has_value())
    {
        result = dbClient->execSqlSync(baseSql + "AND p.post_type = ? " + orderSql,
                                       *dbTypeFilter);
    }
```

type 필터가 있으면 prepared parameter `?`에 `SHARE` 또는 `EXCHANGE` 값을 바인딩해서 조회한다.

```cpp
    else
    {
        result = dbClient->execSqlSync(baseSql + orderSql);
    }
```

type 필터가 없으면 전체 게시글을 조회한다.

```cpp
    std::vector<BoardPostDTO> posts;
    posts.reserve(result.size());
```

결과 row 수만큼 vector capacity를 미리 확보한다.

```cpp
    for (const auto &row : result)
    {
        posts.push_back(rowToBoardPostDTO(row));
    }
```

각 DB row를 `BoardPostDTO`로 변환해 vector에 넣는다.

```cpp
    return posts;
```

게시글 DTO 목록을 반환한다.

### `findById`

```cpp
std::optional<BoardPostDTO> BoardMapper::findById(std::uint64_t postId) const
{
    ensureTablesForLocalDev();
```

게시글 단건 조회 전에도 테이블 존재를 보장한다.

```cpp
    const auto dbClient = drogon::app().getDbClient("default");
```

DB client를 가져온다.

```cpp
    static const std::string sql =
        "SELECT "
        "p.post_id, p.member_id, p.post_type, p.title, p.content, "
        "COALESCE(p.location, '') AS location, p.status, "
        "COALESCE(DATE_FORMAT(p.created_at, '%Y-%m-%d %H:%i:%s'), '') AS created_at, "
        "m.name AS author_name "
        "FROM board_posts p "
        "JOIN members m ON m.member_id = p.member_id "
        "WHERE p.post_id = ? LIMIT 1";
```

게시글 id로 하나만 조회하는 SQL이다. `members`와 join해서 작성자 이름까지 가져오는 구조는 목록 조회와 같다.

```cpp
    const auto result = dbClient->execSqlSync(sql, postId);
```

`postId`를 prepared parameter로 바인딩해서 SQL을 실행한다.

```cpp
    if (result.empty())
    {
        return std::nullopt;
    }
```

해당 id의 게시글이 없으면 값 없음으로 반환한다.

```cpp
    return rowToBoardPostDTO(result[0]);
```

첫 번째 row를 DTO로 변환해 반환한다.

### `insertPost`

```cpp
std::optional<BoardPostDTO> BoardMapper::insertPost(
    std::uint64_t memberId,
    const CreateBoardPostRequestDTO &request) const
{
    ensureTablesForLocalDev();
```

게시글 삽입 전에도 테이블 존재를 보장한다.

```cpp
    const auto dbClient = drogon::app().getDbClient("default");
    drogon::orm::Mapper<BoardPostModel> mapper(dbClient);
```

DB client를 가져오고, `BoardPostModel`용 Drogon ORM mapper를 만든다.

```cpp
    BoardPostModel post(memberId,
                        request.type,
                        request.title,
                        request.content,
                        request.location.value_or(""),
                        "AVAILABLE");
```

INSERT할 모델 객체를 만든다.

- `memberId`: 작성자 id.
- `request.type`: 서비스에서 이미 `SHARE` 또는 `EXCHANGE`로 변환된 값.
- `request.title`: 검증된 제목.
- `request.content`: 검증된 본문.
- `request.location.value_or("")`: 위치가 없으면 빈 문자열 저장.
- `"AVAILABLE"`: 새 게시글의 기본 상태.

```cpp
    mapper.insert(post);
```

Drogon ORM mapper를 통해 INSERT를 실행한다. 성공하면 `post.updateId()`가 호출되어 생성된 primary key가 모델에 반영된다.

```cpp
    return findById(post.postId());
```

생성된 id로 다시 SELECT해서 작성자 이름과 포맷된 생성일이 포함된 DTO를 반환한다.

### `rowToBoardPostDTO`

```cpp
BoardPostDTO BoardMapper::rowToBoardPostDTO(const drogon::orm::Row &row)
{
    BoardPostDTO dto;
```

DB row를 API 응답용 DTO로 변환한다.

```cpp
    dto.postId = row["post_id"].as<std::uint64_t>();
    dto.memberId = row["member_id"].as<std::uint64_t>();
```

게시글 id와 작성자 회원 id를 숫자로 읽는다.

```cpp
    dto.type = toClientType(row["post_type"].as<std::string>());
```

DB enum 값을 클라이언트가 쓰는 소문자 type으로 바꾼다.

```cpp
    dto.title = row["title"].as<std::string>();
    dto.content = row["content"].as<std::string>();
```

제목과 본문을 읽는다.

```cpp
    const auto location = row["location"].as<std::string>();
    if (!location.empty())
    {
        dto.location = location;
    }
```

위치가 빈 문자열이 아니면 DTO의 optional location에 넣는다.

```cpp
    dto.authorName = row["author_name"].as<std::string>();
```

join된 `members.name` 값을 작성자 이름으로 넣는다.

```cpp
    dto.status = toClientStatus(row["status"].as<std::string>());
```

DB enum 상태를 클라이언트용 소문자 상태로 바꾼다.

```cpp
    dto.createdAt = row["created_at"].as<std::string>();
```

SQL에서 포맷된 생성일 문자열을 넣는다.

```cpp
    return dto;
```

완성된 DTO를 반환한다.

### 변환 helper

```cpp
std::string BoardMapper::toClientType(const std::string &dbType)
{
    const auto lowered = toLowerCopy(dbType);
```

DB에서 온 type 문자열을 소문자로 바꾼다.

```cpp
    if (lowered == "share")
    {
        return "share";
    }
    if (lowered == "exchange")
    {
        return "exchange";
    }
    return lowered;
```

알려진 값은 명시적으로 반환하고, 예상 밖 값은 소문자 변환 결과를 그대로 반환한다. DB 값이 늘어나도 최소한 문자열은 클라이언트에 전달된다.

```cpp
std::string BoardMapper::toClientStatus(const std::string &dbStatus)
```

상태 값을 클라이언트용 문자열로 바꾼다.

```cpp
    if (lowered == "available")
    {
        return "available";
    }
    if (lowered == "closed")
    {
        return "closed";
    }
    if (lowered == "deleted")
    {
        return "deleted";
    }
    return lowered;
```

DB 상태 enum의 대표 값을 소문자로 변환한다. 예상 밖 상태도 소문자 문자열로 전달한다.

### `ensureTablesForLocalDev`

```cpp
void BoardMapper::ensureTablesForLocalDev() const
{
    static std::atomic_bool tablesReady{false};
```

테이블 생성 SQL을 프로세스 생명주기 동안 한 번만 실행하기 위한 플래그다. `atomic_bool`을 사용해 여러 요청이 동시에 들어오는 상황을 고려한다.

```cpp
    if (tablesReady.load())
    {
        return;
    }
```

이미 테이블 준비가 끝났으면 바로 반환한다.

```cpp
    const auto dbClient = drogon::app().getDbClient("default");
```

DDL 실행에 사용할 DB client를 가져온다.

```cpp
    dbClient->execSqlSync(
        "CREATE TABLE IF NOT EXISTS members ("
```

`members` 테이블이 없으면 생성한다. Board 게시글은 `member_id` FK를 가지므로 회원 테이블이 먼저 필요하다.

주요 컬럼:

- `member_id`: 회원 primary key.
- `email`: 로그인 이메일, unique key.
- `password_hash`: 비밀번호 해시.
- `name`: 작성자 표시 이름.
- `role`, `status`, `level`, `exp`: 회원 상태/레벨 정보.
- `created_at`, `updated_at`: 생성/수정 시간.

```cpp
    dbClient->execSqlSync(
        "CREATE TABLE IF NOT EXISTS board_posts ("
```

Board 게시글 테이블이 없으면 생성한다.

주요 컬럼:

- `post_id`: 게시글 primary key.
- `member_id`: 작성자 회원 id.
- `post_type`: `SHARE` 또는 `EXCHANGE`.
- `title`: 제목, 최대 100자.
- `content`: 본문.
- `location`: 선택 위치, 최대 100자.
- `status`: `AVAILABLE`, `CLOSED`, `DELETED`.
- `view_count`: 조회수.
- `created_at`, `updated_at`: 생성/수정 시간.

주요 인덱스와 제약:

- `idx_board_posts_member_id`: 작성자 기준 조회를 위한 인덱스.
- `idx_board_posts_type_status_created`: 타입/상태/최신순 목록 조회를 위한 인덱스.
- `idx_board_posts_created_at`: 최신순 조회를 위한 인덱스.
- `fk_board_posts_member`: 게시글 작성자는 `members.member_id`를 참조한다.
- `ON DELETE CASCADE`: 회원이 삭제되면 해당 회원 게시글도 같이 삭제된다.

```cpp
    tablesReady.store(true);
```

테이블 준비가 끝났음을 기록한다.

## `models/BoardTypes.h`

이 파일은 Controller, Service, Mapper 사이에서 주고받는 DTO를 정의한다. DB row 자체가 아니라 API와 비즈니스 계층에서 쓰기 좋은 구조다.

```cpp
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
```

정수 id, optional 필드, 문자열, 목록을 표현하기 위해 사용한다.

### `CreateBoardPostRequestDTO`

```cpp
class CreateBoardPostRequestDTO
{
public:
    std::string type;
    std::string title;
    std::string content;
    std::optional<std::string> location;
};
```

게시글 작성 요청을 담는다.

- `type`: 클라이언트에서는 `share` 또는 `exchange`, 서비스 정규화 이후에는 mapper로 `SHARE` 또는 `EXCHANGE`가 전달된다.
- `title`: 게시글 제목.
- `content`: 게시글 본문.
- `location`: 선택 위치. 값이 없을 수 있으므로 optional이다.

### `BoardPostDTO`

```cpp
class BoardPostDTO
{
public:
    std::uint64_t postId{0};
    std::uint64_t memberId{0};
    std::string type;
    std::string title;
    std::string content;
    std::optional<std::string> location;
    std::string authorName;
    std::string status;
    std::string createdAt;
};
```

클라이언트에 내려줄 게시글 데이터를 담는다.

- `postId`: 게시글 id.
- `memberId`: 작성자 회원 id.
- `type`: 클라이언트용 타입, `share` 또는 `exchange`.
- `title`: 제목.
- `content`: 본문.
- `location`: 선택 위치.
- `authorName`: `members.name`에서 가져온 작성자 이름.
- `status`: 클라이언트용 상태, 예: `available`.
- `createdAt`: 문자열로 포맷된 생성일.

### `BoardListResultDTO`

```cpp
class BoardListResultDTO
{
public:
    bool ok{false};
    int statusCode{400};
    std::string message;
    std::vector<BoardPostDTO> posts;
};
```

목록 조회 서비스 결과다.

- `ok`: 성공 여부.
- `statusCode`: HTTP status code로 사용할 숫자.
- `message`: 클라이언트에 줄 메시지.
- `posts`: 게시글 DTO 목록.

기본값이 실패 상태인 이유는 검증 실패나 예외 상황에서 실수로 성공 응답을 보내지 않게 하기 위함이다.

### `BoardCreateResultDTO`

```cpp
class BoardCreateResultDTO
{
public:
    bool ok{false};
    int statusCode{400};
    std::string message;
    std::optional<BoardPostDTO> post;
};
```

게시글 작성 서비스 결과다.

- `ok`: 성공 여부.
- `statusCode`: HTTP status code.
- `message`: 결과 메시지.
- `post`: 생성 성공 시 생성된 게시글 DTO. 실패 시 값이 없다.

## `models/BoardPostModel.h`

이 파일은 Drogon ORM mapper가 `board_posts` 테이블에 INSERT할 때 사용하는 모델이다. `BoardPostDTO`가 API 응답용이라면, `BoardPostModel`은 DB 테이블 조작용이다.

### include

```cpp
#include <drogon/orm/Row.h>
#include <drogon/orm/SqlBinder.h>
```

DB row 생성자와 SQL parameter binding에 사용한다.

```cpp
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
```

id 타입, nullable 위치, 예외, 문자열 move, 컬럼 목록 타입을 위해 사용한다.

### 클래스 기본 정의

```cpp
class BoardPostModel final
{
```

`board_posts` 테이블을 표현하는 모델이다. `final`은 상속을 막는다.

```cpp
    using PrimaryKeyType = std::uint64_t;
```

primary key 타입을 Drogon mapper가 알 수 있게 정의한다.

```cpp
    inline static const std::string tableName = "board_posts";
    inline static const std::string primaryKeyName = "post_id";
```

이 모델이 어떤 테이블과 primary key 컬럼을 사용하는지 지정한다.

### INSERT용 생성자

```cpp
    BoardPostModel(std::uint64_t memberId,
                   std::string postType,
                   std::string title,
                   std::string content,
                   std::string location,
                   std::string status = "AVAILABLE")
```

새 게시글을 만들 때 사용하는 생성자다. `post_id`는 DB auto increment이므로 받지 않는다.

```cpp
        : postId_(0),
          memberId_(memberId),
          postType_(std::move(postType)),
          title_(std::move(title)),
          content_(std::move(content)),
          location_(std::move(location)),
          status_(std::move(status))
```

멤버 변수를 초기화한다. 문자열은 복사 대신 move로 받아 저장한다.

```cpp
    {
    }
```

생성자 본문은 비어 있다. 초기화 리스트만으로 충분하다.

### Row 생성자

```cpp
    explicit BoardPostModel(const drogon::orm::Row &row)
```

DB row에서 모델을 만들 때 사용하는 생성자다.

```cpp
        : postId_(row["post_id"].as<std::uint64_t>()),
          memberId_(row["member_id"].as<std::uint64_t>()),
          postType_(row["post_type"].as<std::string>()),
          title_(row["title"].as<std::string>()),
          content_(row["content"].as<std::string>()),
          location_(row["location"].as<std::string>()),
          status_(row["status"].as<std::string>()),
          createdAt_(row["created_at"].as<std::string>())
```

각 컬럼 값을 C++ 타입으로 읽어 멤버 변수에 저장한다.

### Primary key 조회 SQL

```cpp
    static std::string sqlForFindingByPrimaryKey()
```

Drogon mapper가 primary key로 row를 찾을 때 사용할 SQL을 제공한다.

```cpp
        return "SELECT post_id, member_id, post_type, title, content, "
               "COALESCE(location, '') AS location, status, "
               "COALESCE(DATE_FORMAT(created_at, '%Y-%m-%d %H:%i:%s'), '') AS "
               "created_at "
               "FROM board_posts WHERE post_id = ?";
```

`post_id = ?` 조건으로 게시글을 찾는다. `location`과 `created_at`은 NULL 방지를 위해 `COALESCE`를 사용한다.

### 컬럼 이름 매핑

```cpp
    static std::string getColumnName(size_t index)
```

컬럼 인덱스를 컬럼 이름으로 바꾼다. Drogon ORM mapper가 내부적으로 컬럼 이름을 알아야 할 때 사용한다.

```cpp
        switch (index)
        {
            case 0:
                return "post_id";
            ...
            case 7:
                return "created_at";
```

0부터 7까지 모델이 아는 컬럼 이름을 반환한다.

```cpp
            default:
                throw std::out_of_range("BoardPostModel column index out of range");
```

정의되지 않은 인덱스가 들어오면 예외를 던진다.

### INSERT SQL

```cpp
    std::string sqlForInserting(bool &needSelection) const
    {
        needSelection = false;
```

INSERT 후 별도 selection이 필요한지 Drogon mapper에 알려준다. 여기서는 false로 둔다.

```cpp
        return "INSERT INTO board_posts "
               "(member_id, post_type, title, content, location, status) "
               "VALUES (?, ?, ?, ?, ?, ?)";
```

INSERT SQL을 반환한다. `post_id`, `created_at`, `updated_at`은 DB 기본값 또는 auto increment에 맡긴다.

### INSERT parameter binding

```cpp
    void outputArgs(drogon::orm::internal::SqlBinder &binder) const
    {
        binder << memberId_ << postType_ << title_ << content_ << location_
               << status_;
    }
```

INSERT SQL의 `?` 자리에 들어갈 값을 순서대로 바인딩한다. SQL 컬럼 순서와 이 바인딩 순서가 반드시 맞아야 한다.

### UPDATE 관련 함수

```cpp
    std::vector<std::string> updateColumns() const
    {
        return {};
    }
```

현재 모델은 UPDATE 기능을 사용하지 않으므로 수정 컬럼 목록을 빈 vector로 반환한다.

```cpp
    void updateArgs(drogon::orm::internal::SqlBinder &) const
    {
    }
```

UPDATE parameter도 없으므로 함수 본문이 비어 있다.

### Primary key 접근

```cpp
    std::uint64_t getPrimaryKey() const noexcept
    {
        return postId_;
    }
```

현재 모델의 primary key 값을 반환한다.

```cpp
    void updateId(std::uint64_t id) noexcept
    {
        postId_ = id;
    }
```

INSERT 후 DB가 생성한 auto increment id를 모델에 반영한다.

```cpp
    std::uint64_t postId() const noexcept
    {
        return postId_;
    }
```

게시글 id getter다. `BoardMapper::insertPost`에서 `findById(post.postId())` 호출에 사용한다.

### 필드 getter

```cpp
    std::uint64_t memberId() const noexcept
    {
        return memberId_;
    }
```

작성자 회원 id를 반환한다.

```cpp
    const std::string &postType() const noexcept
    {
        return postType_;
    }
```

DB 저장용 게시글 type을 반환한다.

```cpp
    const std::string &title() const noexcept
    {
        return title_;
    }
```

제목을 반환한다.

```cpp
    const std::string &content() const noexcept
    {
        return content_;
    }
```

본문을 반환한다.

```cpp
    std::optional<std::string> location() const
    {
        if (location_.empty())
        {
            return std::nullopt;
        }
        return location_;
    }
```

위치가 빈 문자열이면 값 없음으로 반환하고, 있으면 문자열을 반환한다.

```cpp
    const std::string &status() const noexcept
    {
        return status_;
    }
```

게시글 상태를 반환한다.

```cpp
    const std::string &createdAt() const noexcept
    {
        return createdAt_;
    }
```

생성일 문자열을 반환한다.

### private 멤버 변수

```cpp
    std::uint64_t postId_;
```

게시글 primary key다. 새 모델 생성 시 0이고, INSERT 후 DB id로 갱신된다.

```cpp
    std::uint64_t memberId_;
```

작성자 회원 id다.

```cpp
    std::string postType_;
```

DB enum 값으로 저장되는 게시글 타입이다. 현재 `SHARE` 또는 `EXCHANGE`가 들어간다.

```cpp
    std::string title_;
    std::string content_;
```

게시글 제목과 본문이다.

```cpp
    std::string location_;
```

위치 문자열이다. 선택값이지만 DB INSERT 편의를 위해 모델 내부에서는 빈 문자열로 표현한다.

```cpp
    std::string status_;
```

게시글 상태다. 새 글은 기본적으로 `AVAILABLE`이다.

```cpp
    std::string createdAt_;
```

생성 시각 문자열이다. INSERT 직후 모델 생성자에는 세팅되지 않고, DB row에서 모델을 만들 때 채워진다.

## API 응답 형태

목록 조회 성공 예시:

```json
{
  "ok": true,
  "message": "Posts loaded.",
  "posts": [
    {
      "postId": 1,
      "memberId": 3,
      "type": "share",
      "title": "남는 재료 나눔",
      "content": "양파가 남았습니다.",
      "location": "서울",
      "authorName": "tester",
      "status": "available",
      "createdAt": "2026-05-11 12:30:00"
    }
  ]
}
```

작성 성공 예시:

```json
{
  "ok": true,
  "message": "Post created.",
  "post": {
    "postId": 2,
    "memberId": 3,
    "type": "exchange",
    "title": "감자 교환",
    "content": "감자를 당근과 교환하고 싶습니다.",
    "authorName": "tester",
    "status": "available",
    "createdAt": "2026-05-11 12:40:00"
  }
}
```

## 현재 구현 기준으로 중요한 동작

- 게시글 목록은 삭제 상태가 아닌 글만 가져온다.
- 게시글 목록은 최신순으로 최대 50개만 가져온다.
- `type=all` 또는 type 없음은 전체 조회다.
- 허용 type은 `share`, `exchange`뿐이다.
- DB에는 type이 `SHARE`, `EXCHANGE`로 저장되고, 응답에는 `share`, `exchange`로 내려간다.
- 새 게시글의 status는 항상 `AVAILABLE`로 시작한다.
- 게시글 작성에는 `X-Member-Id` 헤더가 필요하다.
- 작성자 이름은 `members` 테이블 join으로 가져온다.
- 로컬 개발에서는 mapper가 `members`, `board_posts` 테이블을 자동 생성한다.
