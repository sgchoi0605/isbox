# database 문서 목차

이 폴더는 `drogon-db-test2` 예제의 DB 흐름을 실제 백엔드 구조로 옮기기 위해 정리한 문서 모음이다.

## 문서 목록

- [01_drogon_db_test2_overview.md](01_drogon_db_test2_overview.md)
  - `drogon-db-test2` 전체 역할, 요청 흐름, DB 흐름을 설명한다.
- [02_main_cpp_explanation.md](02_main_cpp_explanation.md)
  - `src/main.cpp`의 include, helper 함수, 라우터 등록, 예외 처리, 서버 실행 흐름을 설명한다.
- [03_models_explanation.md](03_models_explanation.md)
  - `HealthCheckModel`, `HealthCheckLogModel`이 Drogon ORM Mapper와 연결되기 위해 제공하는 함수들을 설명한다.
- [04_repository_explanation.md](04_repository_explanation.md)
  - `HealthCheckRepository`, `HealthCheckTestResult`의 역할과 DB 작업 순서를 설명한다.
- [05_backend_layering_guide.md](05_backend_layering_guide.md)
  - 실제 `backend` 폴더에 적용할 때 controllers, services, repositories, mappers, models를 어떻게 나눌지 정리한다.

## 한 줄 요약

`drogon-db-test2`는 `main.cpp`가 HTTP 요청을 받고, `HealthCheckRepository`가 DB 작업을 처리하며, `models`의 클래스들이 DB row와 C++ 객체 사이의 변환 규칙을 제공하는 작은 DB 연결 예제다.

