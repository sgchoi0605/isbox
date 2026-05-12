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

const API_BASE_URL = 'http://127.0.0.1:8080';

async function parseJsonSafe(response) {
  try {
    return await response.json();
  } catch (_error) {
    return null;
  }
}

async function postJson(path, body) {
  const response = await fetch(`${API_BASE_URL}${path}`, {
    method: 'POST',
    credentials: 'include',
    headers: {
      'Content-Type': 'application/json'
    },
    body: JSON.stringify(body)
  });

  const data = await parseJsonSafe(response);
  return { response, data };
}

async function putJson(path, body) {
  const response = await fetch(`${API_BASE_URL}${path}`, {
    method: 'PUT',
    credentials: 'include',
    headers: {
      'Content-Type': 'application/json'
    },
    body: JSON.stringify(body)
  });

  const data = await parseJsonSafe(response);
  return { response, data };
}

function buildMemberHeader() {
  const memberId = state.user?.memberId ?? storage.get('user')?.memberId;
  if (!memberId) {
    return {};
  }

  return { 'X-Member-Id': String(memberId) };
}

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

function setAuthenticatedUser(member) {
  if (!member) return;

  const level = Number(member.level) > 0 ? Number(member.level) : 1;
  const exp = Number(member.exp) >= 0 ? Number(member.exp) : 0;

  state.user = {
    ...member,
    level,
    exp,
    loggedIn: true
  };
  state.level = level;
  state.exp = exp;
  storage.set('user', state.user);
}

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
async function login(email, password) {
  if (!email || !password) {
    showToast('Please enter email and password.', 'error');
    return false;
  }

  try {
    const { response, data } = await postJson('/api/auth/login', {
      email,
      password
    });

    if (!response.ok || !data || data.ok === false) {
      throw new Error(data?.message || 'Login failed.');
    }

    setAuthenticatedUser(data.member);

    showToast('Login success!', 'success');
    setTimeout(() => {
      window.location.href = 'home.html';
    }, 500);

    return true;
  } catch (error) {
    showToast(error.message, 'error');
    return false;
  }
}
async function signup(name, email, password, confirmPassword) {
  const agreeTerms = document.getElementById('signupAgreeTerms')?.checked || false;

  if (!name || !email || !password || !confirmPassword) {
    showToast('Please fill in all fields.', 'error');
    return false;
  }

  if (!agreeTerms) {
    showToast('Please agree to the terms.', 'error');
    return false;
  }

  if (password !== confirmPassword) {
    showToast('Passwords do not match.', 'error');
    return false;
  }

  if (password.length < 8) {
    showToast('Password must be at least 8 characters.', 'error');
    return false;
  }

  try {
    const { response, data } = await postJson('/api/auth/signup', {
      name,
      email,
      password,
      confirmPassword,
      agreeTerms
    });

    if (!response.ok || !data || data.ok === false) {
      throw new Error(data?.message || 'Signup failed.');
    }

    setAuthenticatedUser(data.member);

    showToast('Signup success!', 'success');
    setTimeout(() => {
      window.location.href = 'home.html';
    }, 500);

    return true;
  } catch (error) {
    showToast(error.message, 'error');
    return false;
  }
}
async function logout() {
  try {
    await fetch(`${API_BASE_URL}/api/auth/logout`, {
      method: 'POST',
      credentials: 'include'
    });
  } catch (_error) {
    // Ignore network errors on logout.
  }

  storage.remove('user');
  state.user = null;
  showToast('Logged out.', 'success');

  setTimeout(() => {
    window.location.href = 'index.html';
  }, 500);
}
async function checkAuth() {
  try {
    const response = await fetch(`${API_BASE_URL}/api/members/me`, {
      credentials: 'include'
    });
    const data = await parseJsonSafe(response);

    if (!response.ok || !data || data.ok === false) {
      state.user = null;
      storage.remove('user');
      return false;
    }

    setAuthenticatedUser(data.member);
    return true;
  } catch (_error) {
    const user = storage.get('user');
    if (user && user.loggedIn) {
      state.user = user;
      return true;
    }

    state.user = null;
    return false;
  }
}

async function requireAuth() {
  if (!(await checkAuth())) {
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

async function updateProfile() {
  const nameInput = document.getElementById('profileName');
  const emailInput = document.getElementById('profileEmail');

  if (!nameInput || !emailInput) return;

  const name = nameInput.value.trim();
  const email = emailInput.value.trim();

  if (!name || !email) {
    showToast('모든 필드를 입력해주세요.', 'error');
    return;
  }

  try {
    const { response, data } = await putJson('/api/members/profile', {
      name,
      email
    });

    if (!response.ok || !data || data.ok === false || !data.member) {
      throw new Error(data?.message || '프로필 업데이트에 실패했습니다.');
    }

    setAuthenticatedUser(data.member);
    showToast('프로필이 업데이트되었습니다.', 'success');

    toggleProfileEdit();
    displayUserInfo();
  } catch (error) {
    showToast(error.message, 'error');
  }
}

async function changePassword() {
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

  try {
    const { response, data } = await putJson('/api/members/password', {
      currentPassword,
      newPassword,
      confirmPassword
    });

    if (!response.ok || !data || data.ok === false) {
      throw new Error(data?.message || '비밀번호 변경에 실패했습니다.');
    }

    showToast('비밀번호가 변경되었습니다.', 'success');

    currentPasswordInput.value = '';
    newPasswordInput.value = '';
    confirmPasswordInput.value = '';
  } catch (error) {
    showToast(error.message, 'error');
  }
}

function normalizeIngredientForClient(ingredient) {
  if (!ingredient) {
    return null;
  }

  const normalized = {
    ...ingredient
  };

  if (!normalized.id && normalized.ingredientId !== undefined) {
    normalized.id = String(normalized.ingredientId);
  }

  if (normalized.ingredientId === undefined && normalized.id) {
    const parsedId = Number(normalized.id);
    if (!Number.isNaN(parsedId)) {
      normalized.ingredientId = parsedId;
    }
  }

  if (!normalized.storage) {
    normalized.storage = 'other';
  }

  return normalized;
}

function saveIngredients(ingredients) {
  const safeIngredients = Array.isArray(ingredients) ? ingredients : [];
  storage.set('ingredients', safeIngredients);
  state.ingredients = safeIngredients;
}

function getCachedIngredients() {
  if (Array.isArray(state.ingredients) && state.ingredients.length > 0) {
    return state.ingredients;
  }

  const cached = storage.get('ingredients') || [];
  state.ingredients = cached;
  return cached;
}

async function loadIngredients() {
  try {
    const response = await fetch(`${API_BASE_URL}/api/ingredients`, {
      method: 'GET',
      credentials: 'include',
      headers: buildMemberHeader()
    });

    const body = await parseJsonSafe(response);
    if (!response.ok || !body || body.ok === false) {
      throw new Error(body?.message || 'Failed to load ingredients.');
    }

    const ingredients = (body.ingredients || [])
      .map(normalizeIngredientForClient)
      .filter(Boolean);

    saveIngredients(ingredients);
    return ingredients;
  } catch (_error) {
    return getCachedIngredients();
  }
}

function buildIngredientPayload(ingredient) {
  return {
    name: ingredient.name,
    category: ingredient.category,
    quantity: ingredient.quantity,
    unit: ingredient.unit,
    storage: ingredient.storage,
    expiryDate: ingredient.expiryDate,
    publicFoodCode: ingredient.publicFoodCode || undefined,
    nutritionBasisAmount: ingredient.nutritionBasisAmount || undefined,
    energyKcal: ingredient.energyKcal ?? undefined,
    proteinG: ingredient.proteinG ?? undefined,
    fatG: ingredient.fatG ?? undefined,
    carbohydrateG: ingredient.carbohydrateG ?? undefined,
    sugarG: ingredient.sugarG ?? undefined,
    sodiumMg: ingredient.sodiumMg ?? undefined,
    sourceName: ingredient.sourceName || undefined,
    manufacturerName: ingredient.manufacturerName || undefined,
    importYn: ingredient.importYn || undefined,
    originCountryName: ingredient.originCountryName || undefined,
    dataBaseDate: ingredient.dataBaseDate || undefined
  };
}

async function addIngredient(ingredient) {
  const payload = buildIngredientPayload(ingredient);

  const response = await fetch(`${API_BASE_URL}/api/ingredients`, {
    method: 'POST',
    credentials: 'include',
    headers: {
      'Content-Type': 'application/json',
      ...buildMemberHeader()
    },
    body: JSON.stringify(payload)
  });

  const body = await parseJsonSafe(response);
  if (!response.ok || !body || body.ok === false) {
    throw new Error(body?.message || 'Failed to create ingredient.');
  }

  const created = normalizeIngredientForClient(body.ingredient);
  if (!created) {
    throw new Error('Invalid ingredient response.');
  }
  const ingredients = getCachedIngredients().slice();
  ingredients.push(created);
  saveIngredients(ingredients);

  void addExp('INGREDIENT_ADD', '식재료 추가');
  return created;
}

async function updateIngredient(ingredientId, ingredient) {
  const payload = buildIngredientPayload(ingredient);

  const response = await fetch(
    `${API_BASE_URL}/api/ingredients?ingredientId=${encodeURIComponent(ingredientId)}`,
    {
      method: 'PUT',
      credentials: 'include',
      headers: {
        'Content-Type': 'application/json',
        ...buildMemberHeader()
      },
      body: JSON.stringify(payload)
    }
  );

  const body = await parseJsonSafe(response);
  if (!response.ok || !body || body.ok === false) {
    throw new Error(body?.message || 'Failed to update ingredient.');
  }

  const updated = normalizeIngredientForClient(body.ingredient);
  if (!updated) {
    throw new Error('Invalid ingredient response.');
  }
  const ingredients = getCachedIngredients().map(item =>
    String(item.id) === String(updated.id) ? updated : item
  );
  saveIngredients(ingredients);
  return updated;
}

async function deleteIngredient(id) {
  const response = await fetch(
    `${API_BASE_URL}/api/ingredients?ingredientId=${encodeURIComponent(id)}`,
    {
      method: 'DELETE',
      credentials: 'include',
      headers: buildMemberHeader()
    }
  );

  const body = await parseJsonSafe(response);
  if (!response.ok || !body || body.ok === false) {
    throw new Error(body?.message || 'Failed to delete ingredient.');
  }

  const ingredients = getCachedIngredients().filter(item => String(item.id) !== String(id));
  saveIngredients(ingredients);
  showToast('재료가 삭제되었습니다.', 'success');
  return true;
}

async function searchProcessedFoods(keyword) {
  const normalizedKeyword = String(keyword || '').trim();
  if (!normalizedKeyword) {
    return [];
  }

  const response = await fetch(
    `${API_BASE_URL}/api/nutrition/processed-foods?keyword=${encodeURIComponent(normalizedKeyword)}`,
    {
      method: 'GET',
      credentials: 'include'
    }
  );

  const body = await parseJsonSafe(response);
  if (!response.ok || !body || body.ok === false) {
    throw new Error(body?.message || 'Failed to search foods.');
  }

  return body.foods || [];
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
  const ingredients = getCachedIngredients();
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

async function loadCommunityPosts(filter = 'all') {
  const query = filter && filter !== 'all'
    ? `?type=${encodeURIComponent(filter)}`
    : '';

  const response = await fetch(`${API_BASE_URL}/api/board/posts${query}`, {
    method: 'GET',
    credentials: 'include'
  });

  const body = await parseJsonSafe(response);
  if (!response.ok || !body || body.ok === false) {
    throw new Error(body?.message || 'Failed to load community posts.');
  }

  state.communityPosts = body.posts || [];
  return state.communityPosts;
}

function saveCommunityPosts(posts) {
  state.communityPosts = posts;
}

async function addCommunityPost(post) {
  const payload = {
    type: post.type,
    title: post.title,
    content: post.content,
    location: post.location || ''
  };

  const response = await fetch(`${API_BASE_URL}/api/board/posts`, {
    method: 'POST',
    credentials: 'include',
    headers: {
      'Content-Type': 'application/json',
      ...buildMemberHeader()
    },
    body: JSON.stringify(payload)
  });

  const body = await parseJsonSafe(response);
  if (!response.ok || !body || body.ok === false) {
    throw new Error(body?.message || 'Failed to create post.');
  }

  await addExp('COMMUNITY_POST_CREATE', '커뮤니티 게시글 작성');
  return body.post;
}

async function deleteCommunityPost(postId) {
  const response = await fetch(
    `${API_BASE_URL}/api/board/posts?postId=${encodeURIComponent(postId)}`,
    {
      method: 'DELETE',
      credentials: 'include',
      headers: buildMemberHeader()
    }
  );

  const body = await parseJsonSafe(response);
  if (!response.ok || !body || body.ok === false) {
    throw new Error(body?.message || 'Failed to delete post.');
  }

  showToast('게시글이 삭제되었습니다.', 'success');
  return true;
}

function getTimeAgo(dateString) {
  const now = new Date();
  const past = new Date(dateString);
  const diffMs = now.getTime() - past.getTime();
  const diffMins = Math.floor(diffMs / 60000);
  const diffHours = Math.floor(diffMins / 60);
  const diffDays = Math.floor(diffHours / 24);

  if (diffMins < 1) {
    return '방금 전';
  }

  if (diffMins < 60) {
    return `${diffMins}분 전`;
  }

  if (diffHours < 24) {
    return `${diffHours}시간 전`;
  }

  return `${diffDays}일 전`;
}

function recommendRecipes(selectedIngredientIds) {
  const ingredients = getCachedIngredients();
  const selectedItems = ingredients.filter(item =>
    selectedIngredientIds.includes(item.id)
  );

  if (selectedItems.length === 0) {
    showToast('재료를 하나 이상 선택해주세요.', 'error');
    return [];
  }

  const selectedNames = selectedItems.slice(0, 3).map(item => item.name).join(', ');
  const mockRecipes = [
    {
      id: '1',
      name: '간단 볶음밥',
      description: `${selectedNames}을(를) 활용한 빠른 볶음밥`,
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
        '재료를 먹기 좋은 크기로 손질합니다.',
        '팬에 기름을 두르고 재료를 볶습니다.',
        '밥을 넣고 골고루 섞어 볶습니다.',
        '간장과 참기름으로 마무리합니다.'
      ]
    },
    {
      id: '2',
      name: '야채 샐러드',
      description: `${selectedNames}을(를) 활용한 가벼운 샐러드`,
      cookTime: '10분',
      servings: '1인분',
      difficulty: '쉬움',
      ingredients: [
        ...selectedItems.slice(0, 4).map(i => `${i.name} ${i.quantity}${i.unit}`),
        '올리브오일 2스푼',
        '레몬즙 1스푼'
      ],
      steps: [
        '재료를 깨끗이 씻습니다.',
        '한입 크기로 썰어 그릇에 담습니다.',
        '드레싱 재료를 섞어 뿌립니다.'
      ]
    },
    {
      id: '3',
      name: '재료 듬뿍 국물요리',
      description: `${selectedNames}을(를) 넣은 따뜻한 국물요리`,
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
        '냄비에 물을 끓입니다.',
        '재료를 넣고 중불에서 익힙니다.',
        '간을 맞추고 5분 더 끓입니다.'
      ]
    }
  ];

  state.recipes = mockRecipes;
  void addExp('RECIPE_RECOMMEND', 'AI 요리 추천 이용');

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
  const storedUser = storage.get('user');
  if (!state.user && storedUser && storedUser.loggedIn) {
    state.user = storedUser;
  }

  const levelSource = state.user?.level ?? storedUser?.level ?? 1;
  const expSource = state.user?.exp ?? storedUser?.exp ?? 0;
  const level = Number(levelSource) > 0 ? Number(levelSource) : 1;
  const exp = Number(expSource) >= 0 ? Number(expSource) : 0;

  state.level = level;
  state.exp = exp;

  if (state.user) {
    state.user = {
      ...state.user,
      level,
      exp,
      loggedIn: true
    };
    storage.set('user', state.user);
  }

  return { level, exp };
}

function getExpToNextLevel(level) {
  const normalizedLevel = Number(level) > 0 ? Number(level) : 1;
  return normalizedLevel * 100;
}

async function addExp(actionType, actionName) {
  if (!actionType || !state.user?.loggedIn) {
    return false;
  }

  const { level: localLevelBefore } = loadLevelData();

  try {
    const { response, data } = await postJson('/api/members/exp', {
      actionType
    });

    if (!response.ok || !data || data.ok === false || !data.member) {
      throw new Error(data?.message || 'Failed to update experience.');
    }

    setAuthenticatedUser(data.member);
    const { level: currentLevel } = loadLevelData();
    updateLevelDisplay();

    const awardedExp = Number(data.awardedExp || 0);
    const previousLevel = Number(data.previousLevel || localLevelBefore);
    const newLevel = Number(data.newLevel || currentLevel);

    if (newLevel > previousLevel) {
      showToast(`레벨업! Lv.${previousLevel} -> Lv.${newLevel}\n${actionName}으로 레벨이 올랐습니다.`, 'success');
    } else if (awardedExp > 0) {
      showToast(`+${awardedExp} EXP\n${actionName}`, 'success');
    }

    return true;
  } catch (error) {
    console.error('Failed to award experience:', error);
    return false;
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

// 푸드 MBTI
const mbtiResults = {
  SQKFAH: {
    type: 'SQKFAH',
    title: '불닭 도전가',
    description: '매운 음식을 즐기고 새로운 맛에 도전하는 타입입니다.',
    traits: ['매운맛 선호', '빠른 결정', '도전적'],
    recommendedFoods: ['불닭볶음면', '떡볶이', '매운 치킨'],
    color: 'red-orange'
  },
  MCLWLR: {
    type: 'MCLWLR',
    title: '슬로우 미식가',
    description: '부드럽고 안정적인 맛을 선호하는 타입입니다.',
    traits: ['담백한 맛', '안정 선호', '편안한 식사'],
    recommendedFoods: ['크림파스타', '리조또', '스테이크'],
    color: 'purple-pink'
  },
  default: {
    type: 'UNIQUE',
    title: '밸런스 미식가',
    description: '다양한 음식을 상황에 맞게 즐기는 타입입니다.',
    traits: ['균형 잡힌 취향', '유연함', '탐색형'],
    recommendedFoods: ['샐러드', '한식', '파스타'],
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
  void addExp('FOOD_MBTI_COMPLETE', '푸드 MBTI 테스트 완료');

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

document.addEventListener('DOMContentLoaded', async function() {
  if (!window.location.pathname.includes('index.html') &&
      window.location.pathname !== '/' &&
      !window.location.pathname.endsWith('/')) {
    if (!(await requireAuth())) return;
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
  updateIngredient,
  deleteIngredient,
  searchProcessedFoods,
  getDaysUntilExpiry,
  getExpiryBadgeClass,
  getExpiryText,
  checkExpiringItems,
  loadCommunityPosts,
  addCommunityPost,
  deleteCommunityPost,
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
