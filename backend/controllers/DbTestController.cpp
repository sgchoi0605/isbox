#include "DbTestController.hpp"

#include <drogon/drogon.h>
#include <json/json.h>
#include <mysql/mysql.h>

#include <fstream>
#include <functional>
#include <memory>
#include <string>

namespace
{
using ResponseCallback = std::function<void(const drogon::HttpResponsePtr &)>;

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

DbSettings readDbSettings()
{
    DbSettings settings;

    for (const auto &path : {"backend/config/config.json",
                             "../backend/config/config.json",
                             "../../backend/config/config.json"})
    {
        std::ifstream configFile(path);
        if (!configFile)
        {
            continue;
        }

        Json::Value root;
        Json::CharReaderBuilder builder;
        std::string errors;
        if (!Json::parseFromStream(builder, configFile, &root, &errors))
        {
            continue;
        }

        const auto db = root["database"];
        if (!db.isObject())
        {
            return settings;
        }

        settings.host = db.get("host", settings.host).asString();
        settings.port = db.get("port", settings.port).asUInt();
        settings.dbname = db.get("dbname", settings.dbname).asString();
        settings.user = db.get("user", settings.user).asString();
        settings.passwd = db.get("passwd", settings.passwd).asString();
        settings.pluginDir = db.get("plugin_dir", settings.pluginDir).asString();
        settings.ssl = db.get("ssl", settings.ssl).asBool();
        settings.connectTimeoutSeconds =
            db.get("connect_timeout_seconds", settings.connectTimeoutSeconds)
                .asUInt();
        return settings;
    }
    return settings;
}

std::string mysqlError(MYSQL *connection)
{
    return std::to_string(mysql_errno(connection)) + ": " +
           mysql_error(connection);
}

Json::Value testDatabase(drogon::HttpStatusCode &statusCode)
{
    const auto settings = readDbSettings();
    Json::Value body;

    MYSQL *connection = mysql_init(nullptr);
    if (connection == nullptr)
    {
        statusCode = drogon::k500InternalServerError;
        body["ok"] = false;
        body["stage"] = "init";
        body["detail"] = "mysql_init failed";
        return body;
    }

    mysql_options(connection,
                  MYSQL_OPT_CONNECT_TIMEOUT,
                  &settings.connectTimeoutSeconds);
    mysql_options(connection,
                  MYSQL_OPT_READ_TIMEOUT,
                  &settings.connectTimeoutSeconds);
    mysql_options(connection,
                  MYSQL_OPT_WRITE_TIMEOUT,
                  &settings.connectTimeoutSeconds);
    if (!settings.pluginDir.empty())
    {
        mysql_options(connection, MYSQL_PLUGIN_DIR, settings.pluginDir.c_str());
    }

    my_bool sslEnforce = settings.ssl ? 1 : 0;
    my_bool verifyServerCert = 0;
    mysql_options(connection, MYSQL_OPT_SSL_ENFORCE, &sslEnforce);
    mysql_options(connection,
                  MYSQL_OPT_SSL_VERIFY_SERVER_CERT,
                  &verifyServerCert);

    const unsigned long flags = settings.ssl ? CLIENT_SSL : 0;
    if (mysql_real_connect(connection,
                           settings.host.c_str(),
                           settings.user.c_str(),
                           settings.passwd.c_str(),
                           settings.dbname.c_str(),
                           settings.port,
                           nullptr,
                           flags) == nullptr)
    {
        statusCode = drogon::k500InternalServerError;
        body["ok"] = false;
        body["stage"] = "connect";
        body["detail"] = mysqlError(connection);
        mysql_close(connection);
        return body;
    }

    if (mysql_query(connection, "SELECT 1 AS ok") != 0)
    {
        statusCode = drogon::k500InternalServerError;
        body["ok"] = false;
        body["stage"] = "query";
        body["detail"] = mysqlError(connection);
        mysql_close(connection);
        return body;
    }

    MYSQL_RES *result = mysql_store_result(connection);
    if (result == nullptr)
    {
        statusCode = drogon::k500InternalServerError;
        body["ok"] = false;
        body["stage"] = "result";
        body["detail"] = mysqlError(connection);
        mysql_close(connection);
        return body;
    }

    MYSQL_ROW row = mysql_fetch_row(result);
    body["ok"] = true;
    body["stage"] = "ok";
    body["db"] = row && row[0] ? std::stoi(row[0]) : 0;
    statusCode = drogon::k200OK;

    mysql_free_result(result);
    mysql_close(connection);
    return body;
}
}

void registerDbTestRoutes()
{
    drogon::app().registerHandler(
        "/api/db/test",
        [](const drogon::HttpRequestPtr &,
           ResponseCallback &&callback) {
            drogon::HttpStatusCode statusCode;
            auto response =
                drogon::HttpResponse::newHttpJsonResponse(
                    testDatabase(statusCode));
            response->setStatusCode(statusCode);
            callback(response);
        },
        {drogon::Get});
}
