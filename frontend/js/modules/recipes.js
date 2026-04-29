import { state } from './state.js';
import { showToast } from './ui.js';
import { loadIngredients } from './ingredients.js';
import { addExp } from './level.js';

export function recommendRecipes(selectedIngredientIds) {
  const ingredients = loadIngredients();
  const selectedItems = ingredients.filter(item =>
    selectedIngredientIds.includes(item.id)
  );

  if (selectedItems.length === 0) {
    showToast('재료를 하나 이상 선택해주세요.', 'error');
    return [];
  }

  const mockRecipes = [
    {
      id: '1',
      name: '간단 볶음밥',
      description: `${selectedItems.slice(0, 2).map(i => i.name).join(', ')}로 만드는 간단하고 맛있는 볶음밥`,
      cookTime: '15분',
      servings: '2인분',
      difficulty: '쉬움',
      ingredients: [
        '밥 2공기',
        ...selectedItems.slice(0, 3).map(i => `${i.name} ${i.quantity}${i.unit}`),
        '간장 2스푼',
        '참기름 1스푼'
      ],
      steps: [
        '모든 재료를 한입 크기로 썰어주세요.',
        '팬에 기름을 두르고 재료를 볶아주세요.',
        '밥을 넣고 골고루 섞어가며 볶아주세요.',
        '간장과 참기름으로 간을 맞춰주세요.'
      ]
    },
    {
      id: '2',
      name: '건강 샐러드',
      description: `신선한 ${selectedItems[0]?.name || '채소'}를 활용한 영양 만점 샐러드`,
      cookTime: '10분',
      servings: '1인분',
      difficulty: '쉬움',
      ingredients: [
        ...selectedItems.slice(0, 4).map(i => `${i.name} ${i.quantity}${i.unit}`),
        '올리브유 2스푼',
        '레몬즙 1스푼',
        '소금, 후추 약간'
      ],
      steps: [
        '모든 채소를 깨끗이 씻어주세요.',
        '먹기 좋은 크기로 썰어주세요.',
        '볼에 담고 드레싱 재료를 섞어주세요.',
        '골고루 버무려 완성합니다.'
      ]
    },
    {
      id: '3',
      name: '든든한 국물요리',
      description: `${selectedItems[0]?.name || '재료'}로 끓이는 속 풀리는 국물요리`,
      cookTime: '25분',
      servings: '3인분',
      difficulty: '보통',
      ingredients: [
        '물 5컵',
        ...selectedItems.slice(0, 3).map(i => `${i.name} ${i.quantity}${i.unit}`),
        '다진 마늘 1스푼',
        '국간장 2스푼'
      ],
      steps: [
        '냄비에 물을 끓여주세요.',
        '재료를 넣고 중불에서 15분간 끓여주세요.',
        '간을 맞추고 5분 더 끓여주세요.',
        '그릇에 담아 완성합니다.'
      ]
    }
  ];

  state.recipes = mockRecipes;

  addExp(20, 'AI 요리 추천 이용');

  return mockRecipes;
}
