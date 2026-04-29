이 프로젝트의 DB TEST는 “테이블 조회 테스트”가 아니라 **Drogon 백엔드가 MySQL에 붙을 수 있는지 확인하는 최소 연결 테스트 API** 흐름입니다.

**전체 흐름**

1. 빌드 대상에 DB 테스트 컨트롤러가 포함됨  
   [CMakeLists.txt](/C:/workspace/isbox/CMakeLists.txt:11)에서 실행 파일 `isbox_backend`를 만들 때 아래 두 파일만 포함합니다.

   ```cmake
   backend/main.cpp
   backend/controllers/DbTestController.cpp
   ```

   즉 현재는 `MemberController`, `MemberService`, `MemberMapper` 쪽 DB 흐름은 아직 연결되어 있지 않습니다.

2. Drogon + MySQL 의존성이 준비됨  
   [vcpkg.json](/C:/workspace/isbox/vcpkg.json:4)에서 `drogon`을 `mysql` feature와 함께 사용합니다. 이 설정 때문에 Drogon ORM이 MySQL/MariaDB 클라이언트와 함께 빌드됩니다.

3. 서버 시작 시 설정 파일을 읽음  
   [backend/main.cpp](/C:/workspace/isbox/backend/main.cpp:7)에서:

   ```cpp
   drogon::app().loadConfigFile("backend/config/config.json");
   ```

   이때 서버 포트, DB 접속 정보, 앱 설정이 로드됩니다.

4. DB 클라이언트가 설정에서 생성됨  
   [backend/config/config.json](/C:/workspace/isbox/backend/config/config.json:9)의 `db_clients`에 `default`라는 이름의 MySQL 클라이언트가 정의되어 있습니다.

   ```json
   {
     "name": "default",
     "rdbms": "mysql",
     "host": "127.0.0.1",
     "port": 3306,
     "dbname": "isbox",
     "user": "root",
     "passwd": "change-me"
   }
   ```

   여기서 중요한 건 이름이 `"default"`라는 점입니다. 나중에 코드에서 이 이름으로 DB 클라이언트를 꺼냅니다.

5. DB TEST 라우트를 등록함  
   [backend/main.cpp](/C:/workspace/isbox/backend/main.cpp:8)에서:

   ```cpp
   registerDbTestRoutes();
   ```

   이 함수는 [backend/controllers/DbTestController.cpp](/C:/workspace/isbox/backend/controllers/DbTestController.cpp:31)에 있습니다.

6. `/api/db/test` GET API가 등록됨  
   [backend/controllers/DbTestController.cpp](/C:/workspace/isbox/backend/controllers/DbTestController.cpp:33)에서 Drogon에 핸들러를 등록합니다.

   ```cpp
   drogon::app().registerHandler(
       "/api/db/test",
       ...
       {drogon::Get});
   ```

   그래서 서버가 떠 있으면 다음 요청이 DB TEST입니다.

   ```text
   GET http://localhost:8080/api/db/test
   ```

7. 요청이 들어오면 `default` DB 클라이언트를 가져옴  
   [backend/controllers/DbTestController.cpp](/C:/workspace/isbox/backend/controllers/DbTestController.cpp:39)에서:

   ```cpp
   auto dbClient = drogon::app().getDbClient("default");
   ```

   이 `"default"`는 `config.json`의 `db_clients[].name`과 맞아야 합니다.

8. DB 클라이언트가 없으면 즉시 실패 응답  
   [backend/controllers/DbTestController.cpp](/C:/workspace/isbox/backend/controllers/DbTestController.cpp:41)에서 `dbClient`가 없으면 로그를 찍고 500을 반환합니다.

   응답은:

   ```json
   {
     "ok": false,
     "message": "database test failed"
   }
   ```

9. DB 클라이언트가 있으면 `SELECT 1 AS ok` 실행  
   [backend/controllers/DbTestController.cpp](/C:/workspace/isbox/backend/controllers/DbTestController.cpp:48)에서 비동기 SQL을 실행합니다.

   ```cpp
   dbClient->execSqlAsync(
       "SELECT 1 AS ok",
       successCallback,
       errorCallback);
   ```

   이 쿼리는 테이블이 필요 없습니다. DB 연결과 쿼리 실행 가능 여부만 확인합니다.

10. 쿼리 성공 시 JSON 200 반환  
    [backend/controllers/DbTestController.cpp](/C:/workspace/isbox/backend/controllers/DbTestController.cpp:50) 성공 콜백에서 결과 첫 줄의 `ok` 값을 읽습니다.

    ```cpp
    body["ok"] = true;
    body["db"] = result.empty() ? 0 : result[0]["ok"].as<int>();
    ```

    정상 응답은:

    ```json
    {
      "ok": true,
      "db": 1
    }
    ```

11. 쿼리 실패 시 로그 찍고 500 반환  
    [backend/controllers/DbTestController.cpp](/C:/workspace/isbox/backend/controllers/DbTestController.cpp:57) 실패 콜백에서 DB 예외 메시지를 서버 로그에 남깁니다.

    ```cpp
    LOG_ERROR << "Database test query failed: " << error.base().what();
    ```

    클라이언트 응답은 마찬가지로:

    ```json
    {
      "ok": false,
      "message": "database test failed"
    }
    ```

**정리하면**

현재 DB TEST는 이 순서입니다.

```text
서버 실행
-> config.json 로드
-> MySQL db_client "default" 구성
-> /api/db/test 라우트 등록
-> 사용자가 GET /api/db/test 호출
-> getDbClient("default")
-> SELECT 1 AS ok 실행
-> 성공: 200 { ok: true, db: 1 }
-> 실패: 500 { ok: false, message: "database test failed" }
```

문서에도 같은 의도가 적혀 있습니다. [docs/backend/DB_Drogon_connection.md](/C:/workspace/isbox/docs/backend/DB_Drogon_connection.md:29)에 따르면 이 테스트의 목적은 “Drogon 앱이 뜬다 + MySQL client가 붙는다 + 쿼리가 실행된다”까지만 검증하는 것입니다. 테이블, 회원 도메인, `Controller -> Service -> Mapper` 전체 흐름은 아직 이 DB TEST에 포함되어 있지 않습니다.



`cmake -S . -B build-vcpkg -DCMAKE_TOOLCHAIN_FILE=C:\path\to\vcpkg\scripts\buildsystems\vcpkg.cmake`
CMake로 프로젝트를 빌드 준비하면서,
vcpkg 라이브러리를 사용할 수 있게 설정하는 명령어

`cmake --build build-vcpkg --config Debug`
build-vcpkg 폴더에 만들어진 설정을 이용해서 실제로 코드를 컴파일하고 실행파일을 만드는 명령어

.\build-vcpkg\Debug\isbox_backend.exe
