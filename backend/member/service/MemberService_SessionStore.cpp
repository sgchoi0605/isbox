/*
 * 파일 개요: 메모리 기반 세션 저장소 동작 구현 파일이다.
 * 주요 역할: 세션 토큰 생성/조회/삭제와 만료 세션 정리, 동시성 보호 처리.
 */
#include "member/service/MemberService_SessionStore.h"

#include <array>
#include <random>

namespace
{

// 세션 유효 시간: 24시간
constexpr int kSessionDurationSeconds = 60 * 60 * 24;

}  // namespace

namespace auth
{

std::string MemberService_SessionStore::randomHex(std::size_t length)
{
    // 세션 토큰 생성에 사용하는 난수 hex 문자열을 만든다.
    static constexpr std::array<char, 16> kHexChars{
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(0, 15);

    std::string out;
    out.reserve(length);
    for (std::size_t i = 0; i < length; ++i)
    {
        out.push_back(kHexChars[static_cast<std::size_t>(dist(gen))]);
    }
    return out;
}

std::string MemberService_SessionStore::createSession(std::uint64_t memberId)
{
    // 새 토큰을 발급하고 만료 시각과 함께 저장한다.
    const auto now = std::chrono::system_clock::now();
    const auto expiresAt = now + std::chrono::seconds(kSessionDurationSeconds);
    const auto sessionToken = randomHex(48);

    // 동시 접근 보호를 위해 락 안에서 정리+삽입을 수행한다.
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    cleanupExpiredSessionsLocked(now);
    sessions_[sessionToken] = SessionData{memberId, expiresAt};
    return sessionToken;
}

std::optional<std::uint64_t> MemberService_SessionStore::resolveSessionMemberId(
    const std::string &sessionToken)
{
    // 토큰이 유효하면 소유자 memberId를 반환한다.
    if (sessionToken.empty())
    {
        return std::nullopt;
    }

    const auto now = std::chrono::system_clock::now();
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    cleanupExpiredSessionsLocked(now);

    const auto it = sessions_.find(sessionToken);
    if (it == sessions_.end())
    {
        return std::nullopt;
    }

    if (it->second.expiresAt <= now)
    {
        sessions_.erase(it);
        return std::nullopt;
    }

    return it->second.memberId;
}

bool MemberService_SessionStore::removeSession(const std::string &sessionToken)
{
    // 로그아웃 시 토큰을 제거한다.
    if (sessionToken.empty())
    {
        return false;
    }

    std::lock_guard<std::mutex> lock(sessionsMutex_);
    return sessions_.erase(sessionToken) > 0;
}

void MemberService_SessionStore::cleanupExpiredSessionsLocked(
    const std::chrono::system_clock::time_point &now)
{
    // 순회 중 erase를 안전하게 수행하기 위해 iterator를 직접 제어한다.
    for (auto it = sessions_.begin(); it != sessions_.end();)
    {
        if (it->second.expiresAt <= now)
        {
            it = sessions_.erase(it);
            continue;
        }
        ++it;
    }
}

}  // namespace auth

