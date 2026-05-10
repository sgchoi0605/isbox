#include <drogon/drogon.h>
// Drogon의 핵심 헤더입니다. HTTP 서버(app), 라우터, 요청/응답 객체를 사용합니다.

#include <cstdlib>
// Windows에서 _putenv_s로 MariaDB Connector용 환경 변수를 설정하기 위해 사용합니다.

#include <filesystem>
// 현재 실행 경로를 기준으로 MariaDB 인증 플러그인 DLL 위치를 찾기 위해 사용합니다.

#include <functional>
// Drogon 응답 callback 타입인 std::function을 사용하기 위해 필요합니다.

#include <string>
// 예외 메시지와 파일 경로를 문자열로 다루기 위해 사용합니다.

#include <vector>
// MariaDB 플러그인 디렉터리 후보 목록을 담기 위해 사용합니다.

#include "repositories/HealthCheckRepository.h"
// /db-test에서 실제 DB row 객체 저장/조회 작업을 담당하는 Repository입니다.

namespace
{
// 이 파일 안에서만 쓰는 작은 도우미 함수들을 익명 namespace에 둡니다.
// 서버 시작과 HTTP 응답 생성처럼 main.cpp에 남겨도 되는 코드만 포함합니다.

drogon::HttpResponsePtr makeJsonResponse(
    const Json::Value &body,
    drogon::HttpStatusCode status = drogon::k200OK)
// JSON 본문과 HTTP 상태 코드를 받아 Drogon 응답 객체를 만듭니다.
{
    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    // Json::Value를 HTTP JSON 응답으로 바꾸는 Drogon 제공 함수입니다.

    response->setStatusCode(status);
    // 성공이면 200, 실패면 500처럼 호출자가 지정한 상태 코드를 적용합니다.

    return response;
}

Json::Value makeErrorBody(const std::string &message, const std::string &error)
// 예외가 발생했을 때 클라이언트에게 돌려줄 공통 JSON 모양을 만듭니다.
{
    Json::Value body;
    body["ok"] = false;
    body["message"] = message;
    body["error"] = error;
    return body;
}

void configureMariaDbRuntimeForWindows()
// Windows에서 MariaDB Connector/C가 인증 플러그인을 찾을 수 있도록 환경 변수를 설정합니다.
// 운영 코드라기보다는 로컬 테스트 접속을 안정적으로 만들기 위한 보조 설정입니다.
{
#ifdef _WIN32
    _putenv_s("MARIADB_TLS_DISABLE_PEER_VERIFICATION", "1");
    // 로컬 테스트용으로 TLS peer 검증을 끕니다. 운영 환경에서는 보안 정책에 맞게 다시 판단해야 합니다.

    const std::filesystem::path cwd = std::filesystem::current_path();
    // 실행 위치가 프로젝트 루트인지, build 폴더인지에 따라 플러그인 상대 경로가 달라질 수 있습니다.

    const std::vector<std::filesystem::path> candidatePluginDirs = {
        cwd / "build" / "vcpkg_installed" / "x64-windows" / "plugins" /
            "libmariadb",
        cwd / "vcpkg_installed" / "x64-windows" / "plugins" / "libmariadb",
        cwd.parent_path() / "vcpkg_installed" / "x64-windows" / "plugins" /
            "libmariadb",
    };
    // 자주 쓰는 실행 위치별 후보 경로를 순서대로 준비합니다.

    for (const auto &dir : candidatePluginDirs)
    {
        const auto pluginFile = dir / "caching_sha2_password.dll";
        // MySQL 8 계정 인증에 필요한 플러그인 DLL입니다.

        if (std::filesystem::exists(pluginFile))
        {
            _putenv_s("MARIADB_PLUGIN_DIR", dir.string().c_str());
            // DLL이 실제로 있는 디렉터리만 MariaDB 플러그인 경로로 지정합니다.
            break;
        }
    }
#endif
}

}  // namespace

int main()
// 서버 시작점입니다. main.cpp는 서버 설정과 라우터 연결만 담당하도록 단순하게 유지합니다.
{
    configureMariaDbRuntimeForWindows();
    // Drogon DB 클라이언트가 만들어지기 전에 MariaDB 런타임 환경을 먼저 준비합니다.

    drogon::app().loadConfigFile("config.json");
    // listener와 DB 접속 정보는 config.json에서 읽습니다.

    drogon::app().registerHandler(
        "/db-test",
        [](const drogon::HttpRequestPtr &,
           std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
            // 테스트용 라우터입니다. 요청 데이터는 쓰지 않고, DB에 샘플 row를 저장한 뒤 다시 조회합니다.
            // 학습용 예제라 흐름을 읽기 쉽게 동기 ORM 호출을 사용했습니다.
            try
            {
                repository::HealthCheckRepository repository(
                    drogon::app().getDbClient("default"));
                // Repository는 DB 작업을 맡는 객체입니다. main.cpp는 DB SQL 세부 내용을 몰라도 됩니다.

                const auto result = repository.runSimpleTest();
                // C++ row 객체를 만들고 저장한 뒤, ORM Mapper로 다시 읽어오는 간단한 테스트입니다.

                callback(makeJsonResponse(result.toJson()));
            }
            catch (const drogon::orm::DrogonDbException &e)
            {
                callback(makeJsonResponse(
                    makeErrorBody("MySQL ORM 테스트 실패", e.base().what()),
                    drogon::k500InternalServerError));
            }
            catch (const std::exception &e)
            {
                callback(makeJsonResponse(
                    makeErrorBody("서버 내부 오류", e.what()),
                    drogon::k500InternalServerError));
            }
        },
        {drogon::Get});
    // 브라우저 주소창 접근처럼 GET 요청으로 들어온 /db-test만 처리합니다.

    drogon::app().run();
    // 이벤트 루프를 시작합니다. 이 함수 이후 서버는 종료될 때까지 요청을 기다립니다.
}
