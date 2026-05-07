#include <drogon/drogon.h>   // Drogon 웹 프레임워크 (HTTP 서버/라우팅/응답 생성)
#include <mysql/mysql.h>      // MySQL C API (mysql_init, mysql_real_connect 등)

#include <cstdlib>            // std::atoi
#include <string>             // std::string, std::to_string

namespace // 이 안에 있는 함수/변수는 이 cpp 파일 전용
{
// MySQL 에러번호(mysql_errno)와 에러문구(mysql_error)를 "번호:문구" 형태로 합쳐 반환한다.
std::string mysqlErr(MYSQL *conn) // conn은 MYSQL 타입 객체를 가리키는 포인터.
{
    return std::to_string(mysql_errno(conn)) + ": " + mysql_error(conn);
}
}

int main()
{
    // config.json에서 리스너(주소/포트) 같은 런타임 설정을 로드한다.
    // - "config.json": 실행 파일 기준 상대 경로의 설정 파일명
    drogon::app().loadConfigFile("config.json"); // drogon::app() 은 drogon 서버 전체를 관리하는 전역 app 객체를 가져온다.
    // 이 app 객체가 하는 일은 서버 포트 설정, 라우터 등록, DB 연결 설정, 로그 설정, 스레드 설정, 정적 파일 설정 등이 있다.
    // loadConfigFile 은 config 파일을 읽는 함수


    // GET /db-test 요청이 들어오면 아래 람다가 실행되도록 핸들러를 등록한다.
    drogon::app().registerHandler(
        "/db-test",  // 첫 번째 인자: URL 경로 패턴, 사용자가 http://localhost:8080/db-test로 접속하면 아래 람다 함수를 실행해라.

        // 구조는 [](요청 객체, 응답 callback){ 요청처리코드 } 이다.
        [](const drogon::HttpRequestPtr &, // http 요청 정보. method, query string, body, header 같은 거 확인 가능. 근데 지금은 사용하지 않을거라 이름은 생략

           std::function<void(const drogon::HttpResponsePtr &)> &&callback) { // Drogon에서는 응답을 바로 return하는 게 아니라, 마지막에 이 callback을 호출해서 응답을 보낸다

            // 최종 응답 JSON 본문 객체. 성공/실패 여부와 메시지를 담는다.
            Json::Value body; // 예를 들어 body["ok"] = true 하면, json 파일엔 "ok": true 로 저장.



            // MySQL 연결 핸들을 생성한다. (실제 네트워크 연결은 아직 아님)
            // - nullptr: 기본 할당자/기본 초기화로 핸들 생성
            MYSQL *conn = mysql_init(nullptr);
            if (!conn)
            {
                // 핸들 생성 자체가 실패한 경우.
                body["ok"] = false;
                body["message"] = "mysql_init 실패";
            }

            else // 연결 핸들 생성 성공했으면
            {
                // 테스트 환경에서 SSL 강제/검증을 끄기 위한 옵션 값.
                unsigned int sslOff = 0; // SSL : 인터넷 통신을 암호화하던 예전 보안 프로토콜

                // TLS 버전 문자열 (MariaDB 계열 옵션에서 사용 가능).
                const char *tlsVersions = "TLSv1.2,TLSv1.3"; // TLS : 인터넷 통신을 암호화하는 보안 프로토콜. 즉 클라이언트와 서버가 데이터를 주고받을 때, 중간에서 누가 훔쳐봐도 내용을 알 수 없게 함



                // 사용 중인 클라이언트 라이브러리가 해당 매크로를 제공할 때만 옵션 적용. 지금은 보지 마세요.
#ifdef MYSQL_OPT_SSL_ENFORCE
                // - MYSQL_OPT_SSL_ENFORCE: SSL 강제 여부
                // - &sslOff(0): 강제하지 않음
                mysql_options(conn, MYSQL_OPT_SSL_ENFORCE, &sslOff);
#endif
#ifdef MYSQL_OPT_SSL_VERIFY_SERVER_CERT
                // - MYSQL_OPT_SSL_VERIFY_SERVER_CERT: 서버 인증서 검증 여부
                // - &sslOff(0): 검증 비활성화
                mysql_options(conn, MYSQL_OPT_SSL_VERIFY_SERVER_CERT, &sslOff);
#endif
#ifdef MARIADB_OPT_TLS_VERSION
                // - MARIADB_OPT_TLS_VERSION: 허용 TLS 버전 문자열
                mysql_optionsv(conn, MARIADB_OPT_TLS_VERSION, tlsVersions);
#endif
#ifdef MYSQL_OPT_SSL_MODE
                auto sslMode = SSL_MODE_DISABLED;
                // - MYSQL_OPT_SSL_MODE: SSL 동작 모드
                // - SSL_MODE_DISABLED: SSL 비사용
                mysql_optionsv(conn, MYSQL_OPT_SSL_MODE, &sslMode);
#endif

                // 실제 MySQL 서버 연결 시도.
                // host: 127.0.0.1, user: drogon_test, password: drogon_test_pw!, db: test_db, port: 3306

                if (!mysql_real_connect(conn,   // Mysql 객체. mysql_real_connect() 가 진짜 DB서버에 접속하는 함수임.

                                        "127.0.0.1",      // host: MySQL 서버 주소

                                        "drogon_test",    // user: 접속 계정

                                        "drogon_test_pw!",// passwd: 계정 비밀번호

                                        "test_db",        // db: 기본 사용할 데이터베이스

                                        3306,             // port: MySQL 포트
                                        
                                        nullptr,          // unix_socket: TCP 사용 시 보통 nullptr

                                        0))               // client_flag: 추가 클라이언트 플래그(기본 0)
                    {
                        // 연결 실패 시: 실패 메시지 + 상세 MySQL 에러를 응답에 담는다.
                        body["ok"] = false;
                        body["message"] = "MySQL 연결 실패";
                        body["error"] = mysqlErr(conn); // mysqlErr(conn)은 처음에 봤던 에러 함수임.
                    }



                // 연결 성공 후 간단 쿼리(SELECT 1) 실행으로 쿼리 수행 가능 여부까지 확인한다.
                // - "SELECT 1 AS ok": 연결 확인용 최소 쿼리
                else if (mysql_query(conn, "SELECT 1 AS ok") != 0) // mysql_query()는 실제로 MySQL 서버에 SQL문을 보내서 실행한다. 성공하면 0을 반환한다.
                    // select 1 as ok 는 ok라는 이름의 컬럼에 값 1을 반환하는 쿼리
                {
                    // 쿼리 실행 실패 시: 실패 메시지 + 상세 MySQL 에러를 응답에 담는다.
                    body["ok"] = false;
                    body["message"] = "MySQL 쿼리 실패";
                    body["error"] = mysqlErr(conn);
                }


                else
                {
                    // 쿼리 성공 시 결과셋을 가져와 첫 번째 행/컬럼 값을 읽는다.

                    MYSQL_RES *res = mysql_store_result(conn); // 방금 실행한 SELECT 1 AS ok의 결과셋을 가져온다.
                    // MYSQL_RES 는 MySQL 쿼리 결과를 담는 자료형. res는 이 쿼리 결과 (표)를 가리키는 포인터.
                    // mysql_store_result(conn) 는 conn 연결에서 최근 실행한 쿼리 결과를 가져와 c++ 메모리에 저장한다는 뜻. 

                    MYSQL_ROW row = res ? mysql_fetch_row(res) : nullptr; // res가 있으면 첫번째 행을 가져오고 없으면 res를 nullptr로 둔다는 뜻.
                    // mysql_fetch_row(res) 는 res 결과(표)에서 첫 번째 행의 값을 가져옴. 
                    

                    // 정상 케이스 응답 구성.
                    body["ok"] = true;
                    body["message"] = "success";
                    body["value"] = (row && row[0]) ? std::atoi(row[0]) : 0; // row가 있고, 첫 번째 컬럼 값도 있으면 그 값을 int로 바꾸고, 아니면 0을 넣어라.
                    // SELECT 1 AS ok의 결과는 문자열 "1"처럼 들어오니까 정수 1로 바꾸라는 거임.
                    // 따라서 성공 응답은 "ok" : true, "messege" : "success", "value" : 1 이 됨.


                    // 결과셋이 생성되었으면 반드시 해제한다. 안 그럼 메모리 누수됨.
                    if (res)
                    {
                        mysql_free_result(res);
                    }
                }

                // 연결 성공/실패와 무관하게 MySQL 핸들을 닫아 리소스를 정리한다.
                mysql_close(conn);
            }


            // JSON 응답 객체를 생성한다.
            // - body: 직렬화되어 HTTP 응답 본문(JSON)으로 전송됨

            drogon::HttpResponsePtr resp = drogon::HttpResponse::newHttpJsonResponse(body); // 앞에서 만든 body 를 HTTP 응답 본문으로 보내는 객체 resp 를 만든다.
            // HttpResponse 는 HTTP 응답 클래스임.
            // 즉, HttpResponse 클래스 안에 있는 newHttpJsonResponse라는 함수를 호출하겠다.는 뜻임. static 함수라서 :: 를 쓴다는 거임.
            // newHttpJsonResponse 는 body를 JSON 응답으로 만들어주는 함수임.


            // 실패 케이스는 HTTP 상태코드를 500으로 지정한다.
            if (!body["ok"].asBool())  // 즉, ok의 값이 0이라면 그걸 false로 치환.
            {
                resp->setStatusCode(drogon::k500InternalServerError); // 만약 false라면 상태코드 500.
            }



            // 비동기 응답 콜백 호출로 요청 처리를 마무리한다.
            // - resp: 상태코드/본문이 모두 담긴 최종 응답 객체
            callback(resp); // 이게 좀 중요한데, drogon에서는 핸들러 함수 안에서 return하는 방식이 아니라, callback 함수를 통해서 응답을 보낸다.
        },
        {drogon::Get});  // registerHandler()의 세 번째 인자: 허용 HTTP 메서드 목록(여기선 GET만)
        // 우리가 설정한 URL(registerHandler()의 첫 번째 인자)은 HTTP get 요청으로 들어왔을 때만 이 핸들러를 실행하겠다는 뜻임.
        // {} 가 붙는 이유는, 리스트 형태로 HTTP 메서드를 넘기기 때문임.
        // 따라서, {drogon::Get, drogon::Post} 가능.
        // 쉽게 말하면, 브라우저에서 주소창에 접속하면 보통 GET 요청이니까 실행된다.
        // GET 요청은 서버에서 데이터를 가져올 때 주로 사용한다. ex) 브라우저 주소창에 URL 입력해서 접속하는 것
        // POST 요청은 서버에 데이터를 보내서 생성/처리할 때 사용한다. ex) 회원가입, 게시글 작성

    // 이벤트 루프를 시작하고 서버를 실제로 구동한다. (블로킹 호출)
    drogon::app().run();  // 블로킹 호출은 이 함수가 실행되면 서버가 계속 켜져있다는 뜻임.
}
