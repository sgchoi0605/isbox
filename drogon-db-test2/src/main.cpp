#include <drogon/drogon.h>
// Drogon의 핵심 헤더입니다. HTTP 서버(app), 요청/응답 객체, 라우터 등록, JSON 응답 생성 기능을 사용합니다.

#include <drogon/orm/Mapper.h>
// Drogon ORM의 Mapper를 사용하기 위한 헤더입니다. 아래에서 HealthCheckModel을 기본키로 조회할 때 사용합니다.

#include <cstdlib>
// Windows에서 _putenv_s로 환경 변수를 설정하기 위해 필요합니다. MariaDB Connector 런타임 설정에 사용됩니다.

#include <filesystem>
// 현재 실행 경로와 후보 플러그인 디렉터리를 조합하고, DLL 파일 존재 여부를 확인하기 위해 사용합니다.

#include <functional>
// std::function을 사용하기 위한 헤더입니다. Drogon의 비동기 응답 callback 타입을 저장하고 전달할 때 필요합니다.

#include <memory>
// std::make_shared를 사용하기 위한 헤더입니다. 비동기 콜백 체인에서 응답 callback의 수명을 안전하게 유지합니다.

#include <string>
// SQL 문자열, JSON 메시지, 파일 경로 문자열 등을 std::string으로 다루기 위해 사용합니다.

#include <vector>
// MariaDB 플러그인 디렉터리 후보 목록을 std::vector에 담기 위해 사용합니다.

#include "models/HealthCheckModel.h"
// health_checks 테이블 한 행을 C++ 객체로 표현하는 로컬 ORM 모델입니다. Mapper<HealthCheckModel>에서 사용합니다.

namespace
{
// 익명 namespace입니다. 이 파일 안에서만 쓰는 보조 함수들을 외부 링크 대상에서 숨깁니다.
// 같은 이름의 함수가 다른 번역 단위에 있어도 충돌하지 않게 만드는 C++ 관용 패턴입니다.

drogon::HttpResponsePtr makeJsonResponse(
    const Json::Value &body,
    drogon::HttpStatusCode status = drogon::k200OK)
// JSON 본문과 HTTP 상태 코드를 받아 Drogon 응답 객체를 만드는 작은 헬퍼 함수입니다.
// status의 기본값은 200 OK라서 성공 응답에서는 상태 코드를 따로 넘기지 않아도 됩니다.
{
    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    // Json::Value를 HTTP 응답 본문으로 직렬화하는 Drogon 제공 팩토리 함수입니다.
    // Content-Type도 JSON에 맞게 설정되므로 매 라우터에서 반복 작성할 필요가 없습니다.

    response->setStatusCode(status);
    // 호출자가 지정한 HTTP 상태 코드를 응답에 적용합니다.
    // 예외 처리 경로에서는 500 Internal Server Error를 넘겨 실패 응답으로 만듭니다.

    return response;
    // 완성된 응답 객체를 호출자에게 돌려줍니다. Drogon의 callback은 이 포인터를 받아 클라이언트로 전송합니다.
}

void configureMariaDbRuntimeForWindows()
// Windows에서 MariaDB Connector/C가 필요한 런타임 환경 변수를 미리 설정하는 함수입니다.
// Linux/macOS에서는 아래 _WIN32 블록이 컴파일되지 않으므로 아무 작업도 하지 않습니다.
{
#ifdef _WIN32
    // 이 코드는 Windows 빌드에서만 포함됩니다. _putenv_s는 MSVC/Windows 런타임 전용 함수입니다.
    // MariaDB Connector/C 3.4.x 조합에서 로컬 개발 DB 접속 시 TLS 검증이나 인증 플러그인 경로 문제로 실패할 수 있습니다.
    // 서버 코드가 시작되기 전에 환경 변수를 설정해 두면 이후 생성되는 MariaDB 연결이 이 설정을 참조합니다.
    _putenv_s("MARIADB_TLS_DISABLE_PEER_VERIFICATION", "1");
    // MariaDB Connector가 TLS peer 검증을 생략하도록 지시합니다.
    // 로컬 테스트 DB 접속성을 높이기 위한 설정이며, 운영 환경에서는 보안 요구에 맞게 다시 판단해야 합니다.

    const std::filesystem::path cwd = std::filesystem::current_path();
    // 프로그램이 실행되는 현재 작업 디렉터리를 얻습니다.
    // Visual Studio, 터미널, build/Debug 실행 등 실행 위치에 따라 플러그인 상대 경로가 달라질 수 있습니다.

    const std::vector<std::filesystem::path> candidatePluginDirs = {
        cwd / "build" / "vcpkg_installed" / "x64-windows" / "plugins" /
            "libmariadb",
        // 후보 1: 프로젝트 루트에서 실행하는 경우입니다.
        // 예: cwd가 drogon-db-test2이면 build/vcpkg_installed/... 아래에 플러그인이 있을 수 있습니다.

        cwd / "vcpkg_installed" / "x64-windows" / "plugins" / "libmariadb",
        // 후보 2: build 디렉터리에서 실행하는 경우입니다.
        // 예: cwd가 build이면 그 안의 vcpkg_installed/... 경로를 바로 확인합니다.

        cwd.parent_path() / "vcpkg_installed" / "x64-windows" / "plugins" /
            "libmariadb",
        // 후보 3: build/Debug 또는 build/Release 같은 하위 출력 디렉터리에서 실행하는 경우입니다.
        // parent_path()로 한 단계 올라간 뒤 vcpkg_installed/... 경로를 확인합니다.
    };

    for (const auto &dir : candidatePluginDirs)
    // 위에서 만든 후보 디렉터리를 앞에서부터 순서대로 검사합니다.
    // const auto&를 사용해 filesystem::path 객체를 복사하지 않고 참조합니다.
    {
        const auto pluginFile = dir / "caching_sha2_password.dll";
        // MariaDB/MySQL 인증 방식 중 하나인 caching_sha2_password를 처리하는 플러그인 DLL의 예상 경로입니다.
        // 이 파일이 실제로 존재하면 해당 디렉터리를 MariaDB 플러그인 디렉터리로 사용할 수 있다고 판단합니다.
        if (std::filesystem::exists(pluginFile))
        // 파일 시스템에 DLL이 있는지 확인합니다. 존재하지 않는 경로를 환경 변수로 넣지 않기 위한 방어 코드입니다.
        {
            _putenv_s("MARIADB_PLUGIN_DIR", dir.string().c_str());
            // MariaDB Connector가 인증 플러그인을 찾을 디렉터리를 지정합니다.
            // dir.string()은 std::string 임시 객체를 만들고, c_str()은 _putenv_s 호출 동안 사용할 C 문자열 포인터를 제공합니다.
            break;
            // 유효한 플러그인 디렉터리를 하나 찾았으므로 더 이상 후보를 검사하지 않습니다.
        }
    }
#endif
}

}  // namespace
// 익명 namespace 종료입니다. 위의 헬퍼 함수들은 이 파일 내부에서만 사용할 수 있습니다.

int main()
// C++ 프로그램의 진입점입니다. 서버 설정, 라우터 등록, 이벤트 루프 시작을 순서대로 수행합니다.
{
    configureMariaDbRuntimeForWindows();
    // Windows 환경에서 MariaDB Connector가 DB 접속에 필요한 환경 변수를 먼저 적용합니다.
    // DB 클라이언트가 생성되기 전에 호출해야 설정이 안정적으로 반영됩니다.

    drogon::app().loadConfigFile("config.json");
    // Drogon 애플리케이션 설정을 config.json에서 읽습니다.
    // 여기에는 HTTP listener 주소/포트와 db_clients의 default DB 연결 정보가 들어 있습니다.

    drogon::app().registerHandler(
        "/db-test",
        // /db-test URL에 직접 라우터를 등록합니다.
        // 아래 마지막 인자의 {drogon::Get} 때문에 GET 요청만 이 핸들러로 들어옵니다.
        [](const drogon::HttpRequestPtr &,
           std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
            // Drogon 라우터 핸들러 람다입니다.
            // 첫 번째 인자는 HTTP 요청 객체지만, 이 엔드포인트는 요청 본문이나 쿼리 파라미터를 쓰지 않으므로 이름을 생략했습니다.
            // 두 번째 인자는 응답을 비동기로 반환하기 위한 callback입니다. 응답 객체를 만들어 이 callback에 넘기면 클라이언트로 전송됩니다.
            auto responseCallback =
                std::make_shared<std::function<void(const drogon::HttpResponsePtr &)>>(
                    std::move(callback));
            // callback은 rvalue reference로 들어오므로 std::move로 소유권을 가져옵니다.
            // 이후 여러 단계의 비동기 람다에서 같은 callback을 호출해야 하므로 shared_ptr에 담아 수명을 연장합니다.
            // 이렇게 하지 않으면 바깥 핸들러 함수가 끝난 뒤 callback 참조가 사라져 안전하지 않을 수 있습니다.

            auto dbClient = drogon::app().getDbClient("default");
            // config.json의 db_clients 배열에서 name이 "default"인 DB 클라이언트를 가져옵니다.
            // 이 객체로 SQL 실행과 ORM Mapper 조회를 모두 수행합니다.

            auto sendDbError =
                [responseCallback](const char *stage,
                                   const drogon::orm::DrogonDbException &e) {
                    // DB 작업 중 어느 단계에서든 예외가 발생했을 때 공통으로 JSON 에러 응답을 만드는 람다입니다.
                    // stage는 실패한 작업 이름이고, e는 Drogon ORM/DB 계층에서 전달한 예외 객체입니다.
                    Json::Value body;
                    // JSON 응답 본문을 구성할 Json::Value 객체입니다.
                    body["ok"] = false;
                    // 호출 결과가 실패임을 클라이언트가 쉽게 판단할 수 있도록 불리언 필드를 둡니다.
                    body["message"] = "MySQL ORM 쿼리 실패";
                    // 사람이 읽기 쉬운 실패 메시지입니다.
                    body["stage"] = stage;
                    // 어떤 단계에서 실패했는지 응답에 포함합니다. 비동기 체인 디버깅에 중요합니다.
                    body["error"] = e.base().what();
                    // 실제 DB/ORM 예외 메시지를 문자열로 담습니다. base().what()은 하위 예외의 원문 메시지에 접근합니다.
                    (*responseCallback)(
                        makeJsonResponse(body, drogon::k500InternalServerError));
                    // 공통 JSON 응답 헬퍼로 500 응답을 만들고, 저장해 둔 Drogon callback을 호출해 클라이언트에 전송합니다.
                };

            const std::string createHealthTableSql =
                "CREATE TABLE IF NOT EXISTS `health_checks` ("
                "`id` INT NOT NULL,"
                "`ok` INT NOT NULL,"
                "`note` VARCHAR(255) NOT NULL,"
                "PRIMARY KEY (`id`)"
                ")";
            // health_checks 테이블이 없으면 생성하는 SQL입니다.
            // id는 기본키, ok는 성공 여부를 숫자로 저장하는 필드, note는 상태 설명 문자열입니다.
            // IF NOT EXISTS를 사용해 이미 테이블이 있어도 에러가 나지 않게 합니다.

            const std::string createLogTableSql =
                "CREATE TABLE IF NOT EXISTS `health_check_logs` ("
                "`id` BIGINT NOT NULL AUTO_INCREMENT,"
                "`note` VARCHAR(255) NOT NULL,"
                "`created_at` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,"
                "PRIMARY KEY (`id`)"
                ")";
            // health_check_logs 테이블이 없으면 생성하는 SQL입니다.
            // /db-test가 호출될 때마다 로그 행을 하나씩 쌓기 위한 테이블입니다.
            // id는 자동 증가 기본키이고, created_at은 INSERT 시점의 현재 시간이 기본값으로 들어갑니다.

            const std::string upsertSql =
                "INSERT INTO `health_checks` (`id`, `ok`, `note`) "
                "VALUES (1, 1, 'success') "
                "ON DUPLICATE KEY UPDATE "
                "`ok` = VALUES(`ok`), "
                "`note` = VALUES(`note`)";
            // health_checks 테이블의 id=1 행을 항상 성공 상태로 맞추는 UPSERT SQL입니다.
            // 행이 없으면 INSERT하고, 기본키 id=1이 이미 있으면 ok와 note만 UPDATE합니다.
            // 이 덕분에 /db-test를 여러 번 호출해도 health_checks에는 대표 상태 행 하나만 유지됩니다.

            const std::string insertLogSql =
                "INSERT INTO `health_check_logs` (`note`) "
                "VALUES ('/db-test 호출로 기록된 샘플 로그')";
            // /db-test 호출 이력을 health_check_logs에 한 줄 추가하는 SQL입니다.
            // AUTO_INCREMENT id와 created_at은 DB가 자동으로 채워 줍니다.

            dbClient->execSqlAsync(
                createHealthTableSql,
                // 1단계: ORM 조회 대상인 health_checks 테이블을 준비합니다.
                // execSqlAsync는 SQL을 비동기로 실행하고, 성공 콜백 또는 실패 콜백 중 하나를 나중에 호출합니다.
                [dbClient,
                 responseCallback,
                 sendDbError,
                 createLogTableSql,
                 upsertSql,
                 insertLogSql](const drogon::orm::Result &) {
                    // 1단계 성공 콜백입니다. Result는 CREATE TABLE 결과라 현재 로직에서는 사용하지 않습니다.
                    // 다음 비동기 단계에서도 필요한 dbClient, callback, SQL 문자열들을 값으로 캡처합니다.
                    dbClient->execSqlAsync(
                        createLogTableSql,
                        // 2단계: 호출 로그를 저장할 health_check_logs 테이블을 준비합니다.
                        [dbClient,
                         responseCallback,
                         sendDbError,
                         upsertSql,
                         insertLogSql](const drogon::orm::Result &) {
                            // 2단계 성공 콜백입니다. 로그 테이블 준비가 끝나면 대표 상태 행을 upsert합니다.
                            dbClient->execSqlAsync(
                                upsertSql,
                                // 3단계: health_checks의 id=1 행을 success 상태로 INSERT 또는 UPDATE합니다.
                                [dbClient,
                                 responseCallback,
                                 sendDbError,
                                 insertLogSql](const drogon::orm::Result &) {
                                    // 3단계 성공 콜백입니다. 대표 상태 행이 준비되었으므로 호출 로그를 남깁니다.
                                    dbClient->execSqlAsync(
                                        insertLogSql,
                                        // 4단계: health_check_logs에 이번 /db-test 호출 로그를 INSERT합니다.
                                        [dbClient, responseCallback, sendDbError](
                                            const drogon::orm::Result &insertResult) {
                                            // 4단계 성공 콜백입니다. insertResult에는 INSERT 결과와 생성된 auto increment id가 들어 있습니다.
                                            drogon::orm::Mapper<model::HealthCheckModel> mapper(
                                                dbClient);
                                            // HealthCheckModel용 ORM Mapper를 생성합니다.
                                            // 이 Mapper는 HealthCheckModel에 정의된 tableName, primaryKeyName, 생성자 등을 사용해 DB row를 C++ 객체로 변환합니다.

                                            mapper.findByPrimaryKey(
                                                1,
                                                // health_checks 테이블에서 기본키 id=1인 행을 ORM 방식으로 조회합니다.
                                                [responseCallback, insertResult](
                                                    model::HealthCheckModel row) {
                                                    // ORM 조회 성공 콜백입니다. row는 health_checks의 id=1 행을 C++ 객체로 변환한 값입니다.
                                                    Json::Value body;
                                                    // 최종 성공 응답 JSON을 구성합니다.
                                                    body["ok"] = true;
                                                    // 전체 DB 테스트 플로우가 성공했음을 나타냅니다.
                                                    body["message"] = "success";
                                                    // 클라이언트가 읽을 수 있는 간단한 성공 메시지입니다.
                                                    body["value"] = row.ok();
                                                    // ORM 모델에서 읽은 ok 값을 응답에 포함합니다. 위 upsertSql 때문에 보통 1입니다.
                                                    body["row_id"] = row.id();
                                                    // ORM 모델에서 읽은 기본키 id를 응답에 포함합니다. 여기서는 1입니다.
                                                    body["row_note"] = row.note();
                                                    // ORM 모델에서 읽은 note 값을 응답에 포함합니다. 여기서는 success입니다.
                                                    body["log_insert_id"] =
                                                        static_cast<Json::UInt64>(
                                                            insertResult.insertId());
                                                    // 로그 테이블 INSERT로 생성된 AUTO_INCREMENT id를 응답에 포함합니다.
                                                    // Json::UInt64로 캐스팅해 큰 정수도 JSON 값으로 안전하게 표현합니다.
                                                    (*responseCallback)(makeJsonResponse(body));
                                                    // 최종 JSON 성공 응답을 만들어 Drogon callback으로 반환합니다.
                                                },
                                                [sendDbError](
                                                    const drogon::orm::DrogonDbException &e) {
                                                    // ORM 기본키 조회 실패 콜백입니다.
                                                    sendDbError(
                                                        "mapper.findByPrimaryKey", e);
                                                    // 공통 에러 응답 람다에 실패 단계 이름과 예외를 전달합니다.
                                                });
                                        },
                                        [sendDbError](
                                            const drogon::orm::DrogonDbException &e) {
                                            // 4단계 INSERT 로그 SQL 실패 콜백입니다.
                                            sendDbError(
                                                "execSqlAsync(insert health_check_logs)", e);
                                        });
                                },
                                [sendDbError](const drogon::orm::DrogonDbException &e) {
                                    // 3단계 UPSERT SQL 실패 콜백입니다.
                                    sendDbError("execSqlAsync(upsert health_checks)", e);
                                });
                        },
                        [sendDbError](const drogon::orm::DrogonDbException &e) {
                            // 2단계 로그 테이블 생성 SQL 실패 콜백입니다.
                            sendDbError("execSqlAsync(create health_check_logs)", e);
                        });
                },
                [sendDbError](const drogon::orm::DrogonDbException &e) {
                    // 1단계 health_checks 테이블 생성 SQL 실패 콜백입니다.
                    sendDbError("execSqlAsync(create health_checks)", e);
                });
        },
        {drogon::Get});
    // /db-test 라우터는 GET 메서드만 허용합니다.
    // POST/PUT 같은 다른 메서드는 이 핸들러에 매칭되지 않습니다.

    drogon::app().run();
    // Drogon 이벤트 루프를 시작합니다.
    // 이 호출 이후 서버는 config.json의 listener 주소/포트에서 요청을 기다리며, 일반적으로 프로세스 종료 전까지 반환되지 않습니다.
}
