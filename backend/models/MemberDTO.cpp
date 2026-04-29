#include "../common.hpp"

//memberDTO
//회원의 데이터를 보관하는 클래스. 데이터베이스에서 가져옴.


class MemberDTO {
public:
  unsigned long long memberId;
  string email;
  string name;
  string role;
  string status;
  unsigned int level;
  unsigned int exp;
  optional<string> foodMbti;
  string createdAt;
  string updatedAt;
  optional<string> lastLoginAt;
};
