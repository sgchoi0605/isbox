import { storage } from './storage.js';
import { addExp } from './level.js';

// 음식 MBTI
export const mbtiResults = {
  SQKFAH: {
    type: 'SQKFAH',
    title: '불타는 미식 탐험가 🔥',
    description: '매운 한식을 빠르게 즐기며, 새로운 핫플을 찾아다니는 당신! 트렌디한 맛집 탐방을 좋아하는 열정적인 푸디입니다.',
    traits: ['매운맛 선호', '빠른 식사', '한식 애호가', '모험적 성향'],
    recommendedFoods: ['떡볶이', '불닭볶음면', '매운 닭발', '신라면', '김치찌개'],
    color: 'red-orange'
  },
  MCLWLR: {
    type: 'MCLWLR',
    title: '우아한 미식가 🍷',
    description: '부드러운 양식을 여유롭게 즐기는 당신! 정성스럽게 요리하고 익숙한 맛집에서 천천히 식사하는 것을 선호합니다.',
    traits: ['순한맛 선호', '여유로운 식사', '양식 애호가', '안정적 성향'],
    recommendedFoods: ['크림파스타', '리조또', '스테이크', '그라탕', '수프'],
    color: 'purple-pink'
  },
  SQWFAH: {
    type: 'SQWFAH',
    title: '글로벌 스피드 러너 🌍',
    description: '매운 양식을 빠르게 즐기며 새로운 핫플을 탐방하는 당신! 국경을 넘나드는 미식 경험을 추구합니다.',
    traits: ['매운맛 선호', '빠른 식사', '양식 애호가', '모험적 성향'],
    recommendedFoods: ['매운 까르보나라', '칠리 피자', '타코', '치폴레 부리또', '핫윙'],
    color: 'yellow-red'
  },
  MCKFTR: {
    type: 'MCKFTR',
    title: '전통 한식 장인 🏯',
    description: '순한 한식을 정성스럽게 만들어 익숙한 곳에서 즐기는 당신! 전통적인 맛과 안정감을 중요하게 생각합니다.',
    traits: ['순한맛 선호', '정성 요리', '한식 애호가', '안정적 성향'],
    recommendedFoods: ['된장찌개', '비빔밥', '갈비찜', '불고기', '잡채'],
    color: 'green-emerald'
  },
  default: {
    type: 'UNIQUE',
    title: '유니크 미식가 ✨',
    description: '당신만의 독특한 음식 취향을 가진 특별한 미식가입니다! 다양한 음식을 균형있게 즐기는 스타일이에요.',
    traits: ['균형잡힌 취향', '다양성 추구', '유연한 식습관', '개성있는 선택'],
    recommendedFoods: ['퓨전 요리', '브런치', '샐러드', '베이커리', '간식'],
    color: 'blue-cyan'
  }
};

export function saveFoodMBTI(answers) {
  const mbtiType = answers.join('');
  let result = mbtiResults[mbtiType];

  if (!result) {
    const matchingKey = Object.keys(mbtiResults).find(key => {
      if (key === 'default') return false;
      const matches = answers.filter((answer, idx) => key[idx] === answer);
      return matches.length >= 4;
    });

    result = matchingKey ? mbtiResults[matchingKey] : mbtiResults.default;
  }

  const mbtiData = {
    type: result.type,
    title: result.title,
    date: new Date().toISOString()
  };

  storage.set('foodMBTI', mbtiData);
  addExp(50, '음식 MBTI 테스트 완료');

  return result;
}

export function loadFoodMBTI() {
  return storage.get('foodMBTI');
}
