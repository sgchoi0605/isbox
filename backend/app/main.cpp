// 드로곤 프레임워크 핵심 기능을 사용하기 위한 헤더를 포함한다.
#include <drogon/drogon.h>

// 문자열/컨테이너 알고리즘 유틸리티를 사용하기 위한 헤더를 포함한다.
#include <algorithm>
// 환경 변수 접근 함수를 사용하기 위한 헤더를 포함한다.
#include <cstdlib>
// 문자 판별/변환 함수를 사용하기 위한 헤더를 포함한다.
#include <cctype>
// 파일 시스템 경로와 존재 여부 확인을 위한 헤더를 포함한다.
#include <filesystem>
// 문자열 타입 사용을 위한 헤더를 포함한다.
#include <string>
// 동적 배열 컨테이너 사용을 위한 헤더를 포함한다.
#include <vector>

// 회원 컨트롤러 선언을 가져온다.
#include "member/controller/MemberController.h"
// 친구 컨트롤러 선언을 가져온다.
#include "friend/controller/FriendController.h"
// 게시판 컨트롤러 선언을 가져온다.
#include "board/controller/BoardController.h"
// 식재료/가공식품 컨트롤러 선언을 가져온다.
#include "ingredient/controller/IngredientController.h"

// 외부에 노출하지 않을 내부 헬퍼 영역을 시작한다.
namespace
// 새 코드 블록 범위를 시작한다.
{

// 환경 변수 값이 활성화 의미인지 판별하는 헬퍼 함수를 정의한다.
bool isEnabledByEnv(const char *key)
// 새 코드 블록 범위를 시작한다.
{
    // 환경 변수 원본 포인터를 조회한다.
    const auto *raw = std::getenv(key);
    // 환경 변수가 없으면 비활성으로 처리한다.
    if (raw == nullptr)
    // 새 코드 블록 범위를 시작한다.
    {
        // 비활성 결과를 반환한다.
        return false;
    // 현재 코드 블록 범위를 종료한다.
    }

    // 환경 변수 값을 문자열로 복사한다.
    std::string value(raw);
    // 공백 문자를 모두 제거한다.
    value.erase(std::remove_if(value.begin(),
                               // 다음 코드 구문을 실행한다.
                               value.end(),
                               // 다음 코드 구문을 실행한다.
                               [](unsigned char ch) {
                                   // 현재 함수에서 값을 반환한다.
                                   return std::isspace(ch) != 0;
                               // 다음 코드 구문을 실행한다.
                               }),
                // 다음 코드 구문을 실행한다.
                value.end());

    // 대소문자 차이 없이 비교할 수 있도록 소문자로 통일한다.
    std::transform(value.begin(),
                   // 다음 코드 구문을 실행한다.
                   value.end(),
                   // 다음 코드 구문을 실행한다.
                   value.begin(),
                   // 다음 코드 구문을 실행한다.
                   [](unsigned char ch) {
                       // 현재 함수에서 값을 반환한다.
                       return static_cast<char>(std::tolower(ch));
                   // 등록 함수 호출 체인을 마무리한다.
                   });

    // 일반적인 참 값 표현이면 활성으로 판정한다.
    return value == "1" || value == "true" || value == "yes" || value == "on";
// 현재 코드 블록 범위를 종료한다.
}

// 윈도우에서 MariaDB 런타임 동작을 보정하는 헬퍼 함수를 정의한다.
void configureMariaDbRuntimeForWindows()
// 새 코드 블록 범위를 시작한다.
{
// 윈도우 빌드일 때만 아래 설정을 적용한다.
#ifdef _WIN32
    // 엄격 TLS 모드 여부를 환경 변수로 확인한다.
    if (isEnabledByEnv("ISBOX_STRICT_DB_TLS"))
    // 새 코드 블록 범위를 시작한다.
    {
        // 엄격 TLS 모드에서는 피어 검증 비활성화를 해제한다.
        _putenv_s("MARIADB_TLS_DISABLE_PEER_VERIFICATION", "");
        // 엄격 TLS 모드가 활성화되었음을 로그로 남긴다.
        LOG_INFO << "Strict DB TLS mode enabled via ISBOX_STRICT_DB_TLS.";
    // 현재 코드 블록 범위를 종료한다.
    }
    // 엄격 TLS 모드가 아니면 개발 편의 설정을 적용한다.
    else
    // 새 코드 블록 범위를 시작한다.
    {
        // 로컬 개발 기본값으로 피어 검증을 비활성화한다.
        _putenv_s("MARIADB_TLS_DISABLE_PEER_VERIFICATION", "1");
        // 보안 검증 비활성화 상태임을 경고 로그로 남긴다.
        LOG_WARN << "DB TLS peer verification is disabled by default on Windows "
                 // 다음 코드 구문을 실행한다.
                 << "for local development. Set ISBOX_STRICT_DB_TLS=1 to enable "
                 // 다음 코드 구문을 실행한다.
                 << "strict verification.";
    // 현재 코드 블록 범위를 종료한다.
    }

    // 현재 작업 디렉터리 경로를 얻는다.
    const std::filesystem::path cwd = std::filesystem::current_path();
    // 플러그인 DLL이 있을 수 있는 후보 디렉터리 목록을 만든다.
    const std::vector<std::filesystem::path> candidatePluginDirs = {
        // 다음 코드 구문을 실행한다.
        cwd / "build" / "vcpkg_installed" / "x64-windows" / "plugins" /
            // 다음 코드 구문을 실행한다.
            "libmariadb",
        // 다음 코드 구문을 실행한다.
        cwd / "vcpkg_installed" / "x64-windows" / "plugins" / "libmariadb",
        // 다음 코드 구문을 실행한다.
        cwd.parent_path() / "vcpkg_installed" / "x64-windows" / "plugins" /
            // 다음 코드 구문을 실행한다.
            "libmariadb",
    // 다음 코드 구문을 실행한다.
    };

    // 후보 디렉터리를 순회하며 사용 가능한 플러그인 경로를 찾는다.
    for (const auto &dir : candidatePluginDirs)
    // 새 코드 블록 범위를 시작한다.
    {
        // 인증 플러그인 DLL 경로를 구성한다.
        const auto pluginFile = dir / "caching_sha2_password.dll";
        // DLL 파일이 존재하면 해당 디렉터리를 환경 변수로 설정한다.
        if (std::filesystem::exists(pluginFile))
        // 새 코드 블록 범위를 시작한다.
        {
            // MariaDB 플러그인 디렉터리를 런타임에 지정한다.
            _putenv_s("MARIADB_PLUGIN_DIR", dir.string().c_str());
            // 첫 유효 경로를 찾으면 반복을 종료한다.
            break;
        // 현재 코드 블록 범위를 종료한다.
        }
    // 현재 코드 블록 범위를 종료한다.
    }
// 윈도우 전용 설정 블록을 종료한다.
#endif
// 현재 코드 블록 범위를 종료한다.
}

// `/api/*` 경로의 CORS 사전 요청(OPTIONS)을 공통 처리하는 함수를 정의한다.
void registerApiCorsPreflight()
// 새 코드 블록 범위를 시작한다.
{
    // 라우팅 전에 실행되는 전역 어드바이스를 등록한다.
    drogon::app().registerPreRoutingAdvice(
        // 요청을 검사하고 필요하면 즉시 응답하는 람다를 정의한다.
        [](const drogon::HttpRequestPtr &request,
           // 드로곤 애플리케이션 동작을 설정하거나 실행한다.
           drogon::AdviceCallback &&adviceCallback,
           // 드로곤 애플리케이션 동작을 설정하거나 실행한다.
           drogon::AdviceChainCallback &&adviceChainCallback) {
            // 메서드가 OPTIONS 이고 `/api/` 경로로 시작하는지 확인한다.
            if (request->getMethod() == drogon::Options &&
                // 다음 코드 구문을 실행한다.
                request->path().rfind("/api/", 0) == 0)
            // 새 코드 블록 범위를 시작한다.
            {
                // 빈 본문 응답 객체를 생성한다.
                auto response = drogon::HttpResponse::newHttpResponse();
                // 사전 요청 성공 상태 코드(204)를 설정한다.
                response->setStatusCode(drogon::k204NoContent);

                // 우선 표준 대문자 Origin 헤더를 읽는다.
                auto origin = request->getHeader("Origin");
                // 비어 있으면 소문자 origin 헤더도 확인한다.
                if (origin.empty())
                // 새 코드 블록 범위를 시작한다.
                {
                    // 소문자 키에서도 Origin 값을 읽는다.
                    origin = request->getHeader("origin");
                // 현재 코드 블록 범위를 종료한다.
                }
                // Origin 값이 있으면 해당 출처를 허용한다.
                if (!origin.empty())
                // 새 코드 블록 범위를 시작한다.
                {
                    // 요청 출처를 그대로 허용 응답 헤더에 반영한다.
                    response->addHeader("Access-Control-Allow-Origin", origin);
                    // Origin 값에 따라 응답이 달라짐을 캐시에 알린다.
                    response->addHeader("Vary", "Origin");
                // 현재 코드 블록 범위를 종료한다.
                }

                // 쿠키/인증정보 포함 요청을 허용한다.
                response->addHeader("Access-Control-Allow-Credentials", "true");
                // 브라우저가 보낼 수 있는 요청 헤더 목록을 지정한다.
                response->addHeader("Access-Control-Allow-Headers",
                                    // 다음 코드 구문을 실행한다.
                                    "Content-Type, X-Member-Id");
                // 브라우저가 호출 가능한 메서드 목록을 지정한다.
                response->addHeader("Access-Control-Allow-Methods",
                                    // 다음 코드 구문을 실행한다.
                                    "GET, POST, PUT, DELETE, OPTIONS");

                // 사전 요청 응답을 즉시 반환한다.
                adviceCallback(response);
                // 람다 실행을 종료한다.
                return;
            // 현재 코드 블록 범위를 종료한다.
            }

            // 사전 요청 대상이 아니면 다음 체인으로 넘긴다.
            adviceChainCallback();
        // 등록 함수 호출 체인을 마무리한다.
        });
// 현재 코드 블록 범위를 종료한다.
}

// 내부 헬퍼 네임스페이스를 종료한다.
}  // namespace

// 프로그램 시작점(main 함수)을 정의한다.
int main()
// 새 코드 블록 범위를 시작한다.
{
    // 윈도우 환경 DB 런타임 보정 설정을 먼저 적용한다.
    configureMariaDbRuntimeForWindows();
    // 드로곤 설정 파일을 로드한다.
    drogon::app().loadConfigFile("config/config.json");
    // 정적 파일 서비스 루트 디렉터리를 지정한다.
    drogon::app().setDocumentRoot("../frontend");

    // API CORS 사전 요청 처리기를 등록한다.
    registerApiCorsPreflight();

    // 인증/회원 비즈니스 서비스를 생성한다.
    auth::MemberService memberService;
    // 회원 API 컨트롤러를 서비스 의존성과 함께 생성한다.
    auth::MemberController memberController(memberService);
    // 회원 API 라우트를 등록한다.
    memberController.registerHandlers();
    // 친구 API 컨트롤러를 회원 서비스와 함께 생성한다.
    friendship::FriendController friendController(memberService);
    // 친구 API 라우트를 등록한다.
    friendController.registerHandlers();
    // 게시판 API 컨트롤러를 생성한다.
    board::BoardController boardController;
    // 게시판 API 라우트를 등록한다.
    boardController.registerHandlers();
    // 식재료/가공식품 API 컨트롤러를 생성한다.
    ingredient::IngredientController ingredientController;
    // 식재료/가공식품 API 라우트를 등록한다.
    ingredientController.registerHandlers();

    // 루트 경로(`/`) 요청을 프론트 페이지로 리다이렉트하는 핸들러를 등록한다.
    drogon::app().registerHandler(
        // 매칭할 URL 경로를 지정한다.
        "/",
        // 요청 시 리다이렉트 응답을 생성하는 람다를 정의한다.
        [](const drogon::HttpRequestPtr &,
           // 표준 라이브러리 기능을 호출한다.
           std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
            // 실제 프론트 페이지 경로로 이동시키는 응답을 만든다.
            auto response = drogon::HttpResponse::newRedirectionResponse(
                // 다음 코드 구문을 실행한다.
                "/html-version/index.html");
            // 생성한 응답을 콜백으로 전달한다.
            callback(response);
        // 람다 본문 구성을 마무리하고 다음 인자로 이동한다.
        },
        // GET 메서드에서만 이 핸들러를 허용한다.
        {drogon::Get});

    // `/index.html` 요청도 동일한 프론트 페이지로 리다이렉트한다.
    drogon::app().registerHandler(
        // 매칭할 URL 경로를 지정한다.
        "/index.html",
        // 요청 시 리다이렉트 응답을 생성하는 람다를 정의한다.
        [](const drogon::HttpRequestPtr &,
           // 표준 라이브러리 기능을 호출한다.
           std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
            // 실제 프론트 페이지 경로로 이동시키는 응답을 만든다.
            auto response = drogon::HttpResponse::newRedirectionResponse(
                // 다음 코드 구문을 실행한다.
                "/html-version/index.html");
            // 생성한 응답을 콜백으로 전달한다.
            callback(response);
        // 람다 본문 구성을 마무리하고 다음 인자로 이동한다.
        },
        // GET 메서드에서만 이 핸들러를 허용한다.
        {drogon::Get});

    // 드로곤 이벤트 루프를 시작해 서버를 실행한다.
    drogon::app().run();
// 현재 코드 블록 범위를 종료한다.
}
