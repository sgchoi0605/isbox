#include <drogon/drogon.h>

#include <cstdlib>
#include <filesystem>
#include <vector>

#include "../controllers/MemberController.h"
#include "../controllers/BoardController.h"
#include "../controllers/IngredientController.h"

namespace
{

void configureMariaDbRuntimeForWindows()
{
#ifdef _WIN32
    _putenv_s("MARIADB_TLS_DISABLE_PEER_VERIFICATION", "1");

    const std::filesystem::path cwd = std::filesystem::current_path();
    const std::vector<std::filesystem::path> candidatePluginDirs = {
        cwd / "build" / "vcpkg_installed" / "x64-windows" / "plugins" /
            "libmariadb",
        cwd / "vcpkg_installed" / "x64-windows" / "plugins" / "libmariadb",
        cwd.parent_path() / "vcpkg_installed" / "x64-windows" / "plugins" /
            "libmariadb",
    };

    for (const auto &dir : candidatePluginDirs)
    {
        const auto pluginFile = dir / "caching_sha2_password.dll";
        if (std::filesystem::exists(pluginFile))
        {
            _putenv_s("MARIADB_PLUGIN_DIR", dir.string().c_str());
            break;
        }
    }
#endif
}

void registerApiCorsPreflight()
{
    // 게시판 API는 브라우저에서 X-Member-Id 헤더와 DELETE 메서드를 사용한다.
    // OPTIONS 사전 요청을 여기서 공통 처리해 각 컨트롤러가 실제 API 응답에만 집중하게 한다.
    drogon::app().registerPreRoutingAdvice(
        [](const drogon::HttpRequestPtr &request,
           drogon::AdviceCallback &&adviceCallback,
           drogon::AdviceChainCallback &&adviceChainCallback) {
            if (request->getMethod() == drogon::Options &&
                request->path().rfind("/api/", 0) == 0)
            {
                auto response = drogon::HttpResponse::newHttpResponse();
                response->setStatusCode(drogon::k204NoContent);

                auto origin = request->getHeader("Origin");
                if (origin.empty())
                {
                    origin = request->getHeader("origin");
                }
                if (!origin.empty())
                {
                    response->addHeader("Access-Control-Allow-Origin", origin);
                    response->addHeader("Vary", "Origin");
                }

                response->addHeader("Access-Control-Allow-Credentials", "true");
                response->addHeader("Access-Control-Allow-Headers",
                                    "Content-Type, X-Member-Id");
                response->addHeader("Access-Control-Allow-Methods",
                                    "GET, POST, PUT, DELETE, OPTIONS");

                adviceCallback(response);
                return;
            }

            adviceChainCallback();
        });
}

}  // namespace

int main()
{
    configureMariaDbRuntimeForWindows();
    drogon::app().loadConfigFile("config/config.json");
    drogon::app().setDocumentRoot("../frontend");

    registerApiCorsPreflight();

    // 로그인/회원가입/세션 확인 API를 먼저 등록한다.
    auth::MemberController memberController;
    memberController.registerHandlers();
    // 게시글 조회/생성/삭제 엔드포인트를 등록한다.
    board::BoardController boardController;
    boardController.registerHandlers();
    ingredient::IngredientController ingredientController;
    ingredientController.registerHandlers();

    drogon::app().registerHandler(
        "/",
        [](const drogon::HttpRequestPtr &,
           std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
            auto response = drogon::HttpResponse::newRedirectionResponse(
                "/html-version/index.html");
            callback(response);
        },
        {drogon::Get});

    drogon::app().registerHandler(
        "/index.html",
        [](const drogon::HttpRequestPtr &,
           std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
            auto response = drogon::HttpResponse::newRedirectionResponse(
                "/html-version/index.html");
            callback(response);
        },
        {drogon::Get});

    drogon::app().run();
}
