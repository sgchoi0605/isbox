/*
 * 파일 개요: 인증 세션 저장소 인터페이스와 내부 상태 구조 정의 파일이다.
 * 주요 역할: 세션 생명주기 API와 저장 구조(SessionData) 계약 제공.
 */
#pragma once

#include <chrono>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace auth
{

// 메모리 기반 세션 저장소다.
// 토큰 발급/조회/삭제와 만료 정리를 책임진다.
class MemberService_SessionStore
{
  public:
    // 회원 ID로 새 세션을 발급한다.
    // 반환값은 클라이언트 쿠키에 저장할 세션 토큰이다.
    std::string createSession(std::uint64_t memberId);

    // 세션 토큰으로 회원 ID를 조회한다.
    // 토큰이 없거나 만료되었으면 nullopt를 반환한다.
    std::optional<std::uint64_t> resolveSessionMemberId(const std::string &sessionToken);

    // 세션 토큰을 저장소에서 제거한다(로그아웃).
    // 실제로 삭제된 경우 true를 반환한다.
    bool removeSession(const std::string &sessionToken);

  private:
    // 세션 토큰이 가리키는 최소 상태 데이터다.
    // expiresAt은 조회 시점마다 만료 정리에 사용된다.
    struct SessionData
    {
        std::uint64_t memberId{0};
        std::chrono::system_clock::time_point expiresAt;
    };

    // 토큰 생성 및 만료 정리 유틸.
    // cleanupExpiredSessionsLocked는 반드시 mutex를 잡은 상태에서만 호출된다.
    static std::string randomHex(std::size_t length);
    void cleanupExpiredSessionsLocked(const std::chrono::system_clock::time_point &now);

    // 메모리 세션 테이블과 동시성 보호 mutex.
    std::unordered_map<std::string, SessionData> sessions_;
    std::mutex sessionsMutex_;
};

}  // namespace auth

