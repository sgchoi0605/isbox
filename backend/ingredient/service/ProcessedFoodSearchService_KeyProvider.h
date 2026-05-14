/*
 * 파일 개요: 공공 API 서비스 키 조회 제공자 인터페이스 선언 파일이다.
 * 주요 역할: 서비스 키 조회 API와 내부 조회 전략(env/file) 메서드 계약을 정의한다.
 * 사용 위치: 검색 서비스 계층에서 API 키 소스 추상화가 필요할 때 공통으로 include 된다.
 */
#pragma once

#include <mutex>
#include <optional>
#include <string>

namespace ingredient
{

// 공공 API 키 조회 제공자(env 우선, 파일 fallback).
class ProcessedFoodSearchService_KeyProvider
{
  public:
    // 서비스 키를 반환한다. 없으면 nullopt.
    std::optional<std::string> getServiceKey();

  private:
    // 환경변수 읽기 헬퍼.
    static std::optional<std::string> getEnvValue(const char *key);
    // 로컬 설정 파일에서 키 읽기.
    std::optional<std::string> getServiceKeyFromLocalFile();

    std::mutex cacheMutex_;
    std::optional<std::string> cached_;
};

}  // namespace ingredient


