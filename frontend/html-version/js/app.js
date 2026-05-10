// 전역 상태 관리
const state = {
  user: null,
  ingredients: [],
  recipes: [],
  communityPosts: [],
  expiringItems: 0,
  level: 1,
  exp: 0
};

// 로컬 스토리지 유틸리티
const storage = {
  get(key) {
    try {
      const item = localStorage.getItem(key);
      return item ? JSON.parse(item) : null;
    } catch (error) {
      console.error(`Error reading ${key} from localStorage:`, error);
      return null;
    }
  },
  set(key, value) {
    try {
      localStorage.setItem(key, JSON.stringify(value));
    } catch (error) {
      console.error(`Error writing ${key} to localStorage:`, error);
    }
  },
  remove(key) {
    localStorage.removeItem(key);
  }
};

// Toast 알림
function showToast(message, type = 'info') {
  const toast = document.getElementById('toast');
  if (!toast) return;

  toast.className = `toast ${type} active`;
  toast.querySelector('.toast-message').textContent = message;

  setTimeout(() => {
    toast.classList.remove('active');
  }, 3000);
}

// 모달 제어
function openModal(modalId) {
  const modal = document.getElementById(modalId);
  if (modal) {
    modal.classList.add('active');
    document.body.style.overflow = 'hidden';
  }
}

function closeModal(modalId) {
  const modal = document.getElementById(modalId);
  if (modal) {
    modal.classList.remove('active');
    document.body.style.overflow = 'auto';
  }
}

// 알림 패널 토글
function toggleNotificationPanel() {
  const panel = document.getElementById('notificationPanel');
  const overlay = document.getElementById('notificationOverlay');

  if (panel && overlay) {
    panel.classList.toggle('active');
    overlay.classList.toggle('active');
  }
}

function closeNotificationPanel() {
  const panel = document.getElementById('notificationPanel');
  const overlay = document.getElementById('notificationOverlay');

  if (panel && overlay) {
    panel.classList.remove('active');
    overlay.classList.remove('active');
  }
}

// 사용자 인증
function login(email, password) {
  if (!email || !password) {
    showToast('이메일과 비밀번호를 입력해주세요.', 'error');
    return false;
  }

  const user = {
    email: email,
    name: email.split('@')[0],
    loggedIn: true
  };

  storage.set('user', user);
  state.user = user;
  showToast('로그인 성공!', 'success');

  setTimeout(() => {
    window.location.href = 'home.html';
  }, 1000);

  return true;
}

function signup(name, email, password, confirmPassword) {
  if (!name || !email || !password) {
    showToast('모든 필드를 입력해주세요.', 'error');
    return false;
  }

  if (password !== confirmPassword) {
    showToast('비밀번호가 일치하지 않습니다.', 'error');
    return false;
  }

  if (password.length < 8) {
    showToast('비밀번호는 최소 8자 이상이어야 합니다.', 'error');
    return false;
  }

  const user = {
    email: email,
    name: name,
    loggedIn: true
  };

  storage.set('user', user);
  state.user = user;
  showToast('회원가입 성공! 환영합니다!', 'success');

  setTimeout(() => {
    window.location.href = 'home.html';
  }, 1000);

  return true;
}

function logout() {
  storage.remove('user');
  state.user = null;
  showToast('로그아웃되었습니다.', 'success');

  setTimeout(() => {
    window.location.href = 'index.html';
  }, 1000);
}

function checkAuth() {
  const user = storage.get('user');
  if (user && user.loggedIn) {
    state.user = user;
    return true;
  }
  return false;
}

function requireAuth() {
  if (!checkAuth()) {
    window.location.href = 'index.html';
    return false;
  }
  return true;
}

function displayUserInfo() {
  const userNameEl = document.getElementById('userName');
  const userEmailEl = document.getElementById('userEmail');

  if (state.user) {
    if (userNameEl) userNameEl.textContent = state.user.name;
    if (userEmailEl) userEmailEl.textContent = state.user.email;
  }
}

function toggleUserMenu() {
  const dropdown = document.getElementById('userMenuDropdown');
  if (dropdown) {
    dropdown.classList.toggle('active');
  }
}

function toggleProfileEdit() {
  const nameInput = document.getElementById('profileName');
  const emailInput = document.getElementById('profileEmail');
  const actions = document.getElementById('profileActions');
  const editBtn = document.getElementById('editProfileBtn');

  if (nameInput && emailInput && actions && editBtn) {
    const isDisabled = nameInput.disabled;

    nameInput.disabled = !isDisabled;
    emailInput.disabled = !isDisabled;
    actions.style.display = isDisabled ? 'flex' : 'none';
    editBtn.style.display = isDisabled ? 'none' : 'inline-flex';
  }
}

function cancelProfileEdit() {
  const user = state.user;
  const nameInput = document.getElementById('profileName');
  const emailInput = document.getElementById('profileEmail');
  const actions = document.getElementById('profileActions');
  const editBtn = document.getElementById('editProfileBtn');

  if (user && nameInput && emailInput && actions && editBtn) {
    nameInput.value = user.name;
    emailInput.value = user.email;
    nameInput.disabled = true;
    emailInput.disabled = true;
    actions.style.display = 'none';
    editBtn.style.display = 'inline-flex';
  }
}

function updateProfile() {
  const nameInput = document.getElementById('profileName');
  const emailInput = document.getElementById('profileEmail');

  if (!nameInput || !emailInput) return;

  const name = nameInput.value.trim();
  const email = emailInput.value.trim();

  if (!name || !email) {
    showToast('모든 필드를 입력해주세요.', 'error');
    return;
  }

  const updatedUser = {
    ...state.user,
    name: name,
    email: email,
    loggedIn: true
  };

  storage.set('user', updatedUser);
  state.user = updatedUser;
  showToast('프로필이 업데이트되었습니다.', 'success');

  toggleProfileEdit();
  displayUserInfo();
}

function changePassword() {
  const currentPasswordInput = document.getElementById('currentPassword');
  const newPasswordInput = document.getElementById('newPassword');
  const confirmPasswordInput = document.getElementById('confirmPassword');

  if (!currentPasswordInput || !newPasswordInput || !confirmPasswordInput) return;

  const currentPassword = currentPasswordInput.value;
  const newPassword = newPasswordInput.value;
  const confirmPassword = confirmPasswordInput.value;

  if (!currentPassword || !newPassword || !confirmPassword) {
    showToast('모든 필드를 입력해주세요.', 'error');
    return;
  }

  if (newPassword !== confirmPassword) {
    showToast('새 비밀번호가 일치하지 않습니다.', 'error');
    return;
  }

  if (newPassword.length < 8) {
    showToast('비밀번호는 최소 8자 이상이어야 합니다.', 'error');
    return;
  }

  // Mock password change
  showToast('비밀번호가 변경되었습니다.', 'success');

  currentPasswordInput.value = '';
  newPasswordInput.value = '';
  confirmPasswordInput.value = '';
}

function loadIngredients() {
  const ingredients = storage.get('ingredients') || [];
  state.ingredients = ingredients;
  return ingredients;
}

function saveIngredients(ingredients) {
  storage.set('ingredients', ingredients);
  state.ingredients = ingredients;
}

function addIngredient(ingredient) {
  const newIngredient = {
    id: Date.now().toString(),
    ...ingredient
  };

  const ingredients = loadIngredients();
  ingredients.push(newIngredient);
  saveIngredients(ingredients);

  addExp(10, '냉장고 재료 추가');

  return newIngredient;
}

function deleteIngredient(id) {
  const ingredients = loadIngredients();
  const filtered = ingredients.filter(item => item.id !== id);
  saveIngredients(filtered);
  showToast('재료가 삭제되었습니다.', 'success');
}

function getDaysUntilExpiry(expiryDate) {
  const today = new Date();
  today.setHours(0, 0, 0, 0);
  const expiry = new Date(expiryDate);
  expiry.setHours(0, 0, 0, 0);
  const diffTime = expiry.getTime() - today.getTime();
  const diffDays = Math.ceil(diffTime / (1000 * 60 * 60 * 24));
  return diffDays;
}

function getExpiryBadgeClass(expiryDate) {
  const days = getDaysUntilExpiry(expiryDate);

  if (days < 0) {
    return 'badge-danger';
  } else if (days <= 3) {
    return 'badge-danger';
  } else if (days <= 7) {
    return 'badge-warning';
  } else {
    return 'badge-success';
  }
}

function getExpiryText(expiryDate) {
  const days = getDaysUntilExpiry(expiryDate);

  if (days < 0) {
    return '유통기한 만료';
  } else {
    return `D-${days}`;
  }
}

function checkExpiringItems() {
  const ingredients = loadIngredients();
  const today = new Date();
  const threeDaysLater = new Date(today.getTime() + 3 * 24 * 60 * 60 * 1000);

  const expiring = ingredients.filter(item => {
    const expiryDate = new Date(item.expiryDate);
    return expiryDate <= threeDaysLater && expiryDate >= today;
  });

  state.expiringItems = expiring.length;

  const badge = document.getElementById('notificationCount');
  if (badge) {
    if (expiring.length > 0) {
      badge.textContent = expiring.length;
      badge.style.display = 'flex';
    } else {
      badge.style.display = 'none';
    }
  }

  return expiring;
}

function loadCommunityPosts() {
  let posts = storage.get('communityPosts');

  if (!posts || posts.length === 0) {
    posts = [
      {
        id: '1',
        type: 'share',
        title: '치킨 남은 거 나눠드려요',
        description: '배달시킨 치킨이 너무 많이 남았어요. 가까운 분 가져가세요!',
        location: '서울시 강남구',
        author: '김철수',
        createdAt: new Date(Date.now() - 2 * 60 * 60 * 1000).toISOString(),
        status: 'available'
      },
      {
        id: '2',
        type: 'exchange',
        title: '양파 <-> 감자 교환해요',
        description: '양파가 너무 많아요. 감자와 교환하실 분 연락주세요.',
        location: '서울시 서초구',
        author: '이영희',
        createdAt: new Date(Date.now() - 5 * 60 * 60 * 1000).toISOString(),
        status: 'available'
      }
    ];
    storage.set('communityPosts', posts);
  }

  state.communityPosts = posts;
  return posts;
}

function saveCommunityPosts(posts) {
  storage.set('communityPosts', posts);
  state.communityPosts = posts;
}

function addCommunityPost(post) {
  const newPost = {
    id: Date.now().toString(),
    ...post,
    createdAt: new Date().toISOString(),
    status: 'available'
  };

  const posts = loadCommunityPosts();
  posts.unshift(newPost);
  saveCommunityPosts(posts);

  addExp(30, '커뮤니티 게시글 작성');

  return newPost;
}

function getTimeAgo(dateString) {
  const now = new Date();
  const past = new Date(dateString);
  const diffMs = now.getTime() - past.getTime();
  const diffMins = Math.floor(diffMs / 60000);
  const diffHours = Math.floor(diffMins / 60);
  const diffDays = Math.floor(diffHours / 24);

  if (diffMins < 60) {
    return `${diffMins}분 전`;
  } else if (diffHours < 24) {
    return `${diffHours}시간 전`;
  } else {
    return `${diffDays}일 전`;
  }
}

function recommendRecipes(selectedIngredientIds) {
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

function switchTab(tabName) {
  const triggers = document.querySelectorAll('.tab-trigger');
  const contents = document.querySelectorAll('.tab-content');

  triggers.forEach(trigger => {
    trigger.classList.remove('active');
    if (trigger.dataset.tab === tabName) {
      trigger.classList.add('active');
    }
  });

  contents.forEach(content => {
    content.classList.remove('active');
    if (content.dataset.tab === tabName) {
      content.classList.add('active');
    }
  });
}

function formatDate(dateString) {
  const date = new Date(dateString);
  return date.toLocaleDateString('ko-KR', {
    year: 'numeric',
    month: 'long',
    day: 'numeric'
  });
}

// 레벨 시스템
function loadLevelData() {
  const level = storage.get('userLevel') || 1;
  const exp = storage.get('userExp') || 0;
  state.level = level;
  state.exp = exp;
  return { level, exp };
}

function getExpToNextLevel(level) {
  return level * 100;
}

function addExp(amount, actionName) {
  const { level, exp } = loadLevelData();
  const newExp = exp + amount;
  const requiredExp = getExpToNextLevel(level);

  if (newExp >= requiredExp) {
    // 레벨업!
    const newLevel = level + 1;
    const remainingExp = newExp - requiredExp;

    storage.set('userLevel', newLevel);
    storage.set('userExp', remainingExp);
    state.level = newLevel;
    state.exp = remainingExp;

    showToast(`🎉 레벨업! Lv.${level} → Lv.${newLevel}\n${actionName}으로 레벨이 올랐습니다!`, 'success');
    updateLevelDisplay();
  } else {
    storage.set('userExp', newExp);
    state.exp = newExp;

    showToast(`+${amount} EXP\n${actionName}`, 'success');
    updateLevelDisplay();
  }
}

function updateLevelDisplay() {
  const levelBadge = document.getElementById('userLevelBadge');
  const levelText = document.getElementById('profileLevel');
  const expText = document.getElementById('profileExp');
  const expProgress = document.getElementById('expProgress');

  const { level, exp } = loadLevelData();
  const expToNext = getExpToNextLevel(level);
  const percentage = (exp / expToNext) * 100;

  if (levelBadge) {
    levelBadge.textContent = `Lv.${level}`;
  }

  if (levelText) {
    levelText.textContent = `레벨 ${level}`;
  }

  if (expText) {
    expText.textContent = `경험치 ${exp} / ${expToNext}`;
  }

  if (expProgress) {
    expProgress.style.width = `${percentage}%`;
  }
}

// 음식 MBTI
const mbtiResults = {
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

function saveFoodMBTI(answers) {
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

function loadFoodMBTI() {
  return storage.get('foodMBTI');
}

function activateCurrentNav() {
  const currentPage = window.location.pathname.split('/').pop().replace('.html', '');
  const navLinks = document.querySelectorAll('.nav-link');

  navLinks.forEach(link => {
    if (link.dataset.page === currentPage) {
      link.classList.add('active');
    }
  });
}

document.addEventListener('DOMContentLoaded', function() {
  if (!window.location.pathname.includes('index.html') &&
      window.location.pathname !== '/' &&
      !window.location.pathname.endsWith('/')) {
    if (!requireAuth()) return;
    displayUserInfo();
    checkExpiringItems();
    activateCurrentNav();
    loadLevelData();
    updateLevelDisplay();
  }

  const modals = document.querySelectorAll('.modal');
  modals.forEach(modal => {
    modal.addEventListener('click', function(e) {
      if (e.target === modal) {
        closeModal(modal.id);
      }
    });
  });

  const notificationOverlay = document.getElementById('notificationOverlay');
  if (notificationOverlay) {
    notificationOverlay.addEventListener('click', closeNotificationPanel);
  }
});

window.app = {
  login,
  signup,
  logout,
  checkAuth,
  requireAuth,
  displayUserInfo,
  toggleUserMenu,
  toggleProfileEdit,
  cancelProfileEdit,
  updateProfile,
  changePassword,
  loadIngredients,
  addIngredient,
  deleteIngredient,
  getDaysUntilExpiry,
  getExpiryBadgeClass,
  getExpiryText,
  checkExpiringItems,
  loadCommunityPosts,
  addCommunityPost,
  getTimeAgo,
  recommendRecipes,
  showToast,
  openModal,
  closeModal,
  toggleNotificationPanel,
  closeNotificationPanel,
  switchTab,
  formatDate,
  loadLevelData,
  getExpToNextLevel,
  addExp,
  updateLevelDisplay,
  saveFoodMBTI,
  loadFoodMBTI,
  mbtiResults,
  storage,
  state
};
