// 전역 상태 관리
const state = {
  user: null,
  ingredients: [],
  communityPosts: [],
  communityPagination: {
    page: 1,
    size: 9,
    totalItems: 0,
    totalPages: 0,
    hasPrev: false,
    hasNext: false
  },
  expiringItems: 0,
  level: 1,
  exp: 0
};

const API_BASE_URL = 'http://127.0.0.1:8080';

const TOAST_MESSAGE_TRANSLATIONS = Object.freeze({
  'Please enter email and password.': '이메일과 비밀번호를 입력해주세요.',
  'Please fill in all fields.': '모든 필드를 입력해주세요.',
  'Please fill in all required fields.': '필수 항목을 모두 입력해주세요.',
  'Please agree to the terms.': '약관에 동의해주세요.',
  'You must agree to the terms.': '약관에 동의해주세요.',
  'Invalid email format.': '이메일 형식이 올바르지 않습니다.',
  'Password must be at least 8 characters.': '비밀번호는 최소 8자 이상이어야 합니다.',
  'Passwords do not match.': '비밀번호가 일치하지 않습니다.',
  'New password must be different.': '새 비밀번호는 현재 비밀번호와 달라야 합니다.',
  'Current password is incorrect.': '현재 비밀번호가 올바르지 않습니다.',
  'Email is already registered.': '이미 가입된 이메일입니다.',
  'Unauthorized.': '로그인이 필요합니다.',
  'Forbidden.': '친구 관계에서만 조회할 수 있습니다.',
  'Member not found.': '사용자를 찾을 수 없습니다.',
  'Invalid member id.': '잘못된 사용자 ID입니다.',
  'Invalid action type.': '잘못된 작업 유형입니다.',
  'Invalid storage value.': '보관 위치 값이 올바르지 않습니다.',
  'Invalid expiry date format.': '유통기한 형식이 올바르지 않습니다.',
  'Invalid post type filter.': '게시글 필터 값이 올바르지 않습니다.',
  'Invalid page parameter.': '페이지 값이 올바르지 않습니다.',
  'Invalid size parameter.': '페이지 크기 값이 올바르지 않습니다.',
  'Type must be share or exchange.': '게시글 유형은 나눔 또는 교환이어야 합니다.',
  'Title is required.': '제목을 입력해주세요.',
  'Title must be 100 characters or fewer.': '제목은 100자 이하여야 합니다.',
  'Content is required.': '내용을 입력해주세요.',
  'Location must be 100 characters or fewer.': '거래 위치는 100자 이하여야 합니다.',
  'Keyword is required.': '검색어를 입력해주세요.',
  'Ingredient id is required.': '재료 ID가 필요합니다.',
  'Post id is required.': '게시글 ID가 필요합니다.',
  'Ingredient not found.': '재료를 찾을 수 없습니다.',
  'Post not found.': '게시글을 찾을 수 없습니다.',
  'You can delete only your own post.': '본인이 작성한 게시글만 삭제할 수 있습니다.',
  'Required ingredient fields are missing.': '필수 재료 항목이 누락되었습니다.',
  'Ingredient field length exceeds limit.': '재료 입력 길이가 제한을 초과했습니다.',
  'Failed to create member.': '회원 생성에 실패했습니다.',
  'Failed to load member.': '회원 정보를 불러오지 못했습니다.',
  'Failed to load updated ingredient.': '수정된 재료 정보를 불러오지 못했습니다.',
  'Failed to create ingredient.': '재료 등록에 실패했습니다.',
  'Failed to update ingredient.': '재료 수정에 실패했습니다.',
  'Failed to delete ingredient.': '재료 삭제에 실패했습니다.',
  'Failed to search foods.': '식품 검색에 실패했습니다.',
  'Failed to create post.': '게시글 등록에 실패했습니다.',
  'Failed to load ingredients.': '재료 목록을 불러오지 못했습니다.',
  'Failed to load community posts.': '게시글을 불러오지 못했습니다.',
  'Failed to delete post.': '게시글 삭제에 실패했습니다.',
  'Failed to update experience.': '경험치 업데이트에 실패했습니다.',
  'Database error during signup.': '회원가입 중 데이터베이스 오류가 발생했습니다.',
  'Server error during signup.': '회원가입 중 서버 오류가 발생했습니다.',
  'Server error during login.': '로그인 중 서버 오류가 발생했습니다.',
  'Database error while updating profile.': '프로필 수정 중 데이터베이스 오류가 발생했습니다.',
  'Server error while updating profile.': '프로필 수정 중 서버 오류가 발생했습니다.',
  'Server error while updating password.': '비밀번호 변경 중 서버 오류가 발생했습니다.',
  'Server error while updating experience.': '경험치 업데이트 중 서버 오류가 발생했습니다.',
  'Server error while loading ingredients.': '재료 목록 조회 중 서버 오류가 발생했습니다.',
  'Server error while creating ingredient.': '재료 등록 중 서버 오류가 발생했습니다.',
  'Server error while updating ingredient.': '재료 수정 중 서버 오류가 발생했습니다.',
  'Server error while deleting ingredient.': '재료 삭제 중 서버 오류가 발생했습니다.',
  'Database error while loading posts.': '게시글 조회 중 데이터베이스 오류가 발생했습니다.',
  'Server error while loading posts.': '게시글 조회 중 서버 오류가 발생했습니다.',
  'Database error while creating post.': '게시글 등록 중 데이터베이스 오류가 발생했습니다.',
  'Server error while creating post.': '게시글 등록 중 서버 오류가 발생했습니다.',
  'Database error while deleting post.': '게시글 삭제 중 데이터베이스 오류가 발생했습니다.',
  'Server error while deleting post.': '게시글 삭제 중 서버 오류가 발생했습니다.',
  'Invalid response from nutrition public API.': '영양 정보 공공 API 응답 형식이 올바르지 않습니다.',
  'Nutrition public API returned error.': '영양 정보 공공 API에서 오류를 반환했습니다.',
  'Login failed.': '로그인에 실패했습니다.',
  'Signup failed.': '회원가입에 실패했습니다.',
  'Invalid ingredient response.': '재료 응답 형식이 올바르지 않습니다.',
  'Profile updated.': '프로필이 업데이트되었습니다.',
  'Profile loaded.': '프로필을 불러왔습니다.',
  'Food MBTI saved.': '음식 MBTI 결과가 저장되었습니다.',
  'Password updated.': '비밀번호가 변경되었습니다.',
  'Experience updated.': '경험치가 업데이트되었습니다.',
  'Ingredients loaded.': '재료 목록을 불러왔습니다.',
  'Ingredient created.': '재료가 추가되었습니다.',
  'Ingredient updated.': '재료가 수정되었습니다.',
  'Ingredient deleted.': '재료가 삭제되었습니다.',
  'Posts loaded.': '게시글을 불러왔습니다.',
  'Post created.': '게시글이 등록되었습니다.',
  'Post deleted.': '게시글이 삭제되었습니다.',
  'Signup success.': '회원가입이 완료되었습니다.',
  'Login success.': '로그인되었습니다.',
  'Login success!': '로그인되었습니다.',
  'Signup success!': '회원가입이 완료되었습니다.',
  'Logged out.': '로그아웃되었습니다.'
});

function isLikelyEnglishMessage(message) {
  return /[A-Za-z]/.test(message) && !/[가-힣]/.test(message);
}

function translateToastMessage(message, type = 'info') {
  const normalized = typeof message === 'string' ? message.trim() : '';
  if (!normalized) {
    return type === 'error'
      ? '오류가 발생했습니다. 잠시 후 다시 시도해주세요.'
      : '';
  }

  const translated = TOAST_MESSAGE_TRANSLATIONS[normalized];
  if (translated) {
    return translated;
  }

  if (normalized.startsWith('Failed to call nutrition public API.')) {
    return '영양 정보 공공 API 호출에 실패했습니다.';
  }

  if (normalized.startsWith('Nutrition public API error (')) {
    return '영양 정보 공공 API 오류가 발생했습니다.';
  }

  if (type === 'error' && isLikelyEnglishMessage(normalized)) {
    return '오류가 발생했습니다. 잠시 후 다시 시도해주세요.';
  }

  return normalized;
}

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

async function getJson(path) {
  const response = await fetch(`${API_BASE_URL}${path}`, {
    method: 'GET',
    credentials: 'include'
  });

  const data = await parseJsonSafe(response);
  return { response, data };
}

function buildMemberHeader() {
  const memberId = state.user?.memberId;
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
}

// Toast 알림
function showToast(message, type = 'info') {
  const toast = document.getElementById('toast');
  if (!toast) return;
  const translatedMessage = translateToastMessage(message, type);

  toast.className = `toast ${type} active`;
  toast.querySelector('.toast-message').textContent = translatedMessage;

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
    showToast('이메일과 비밀번호를 입력해주세요.', 'error');
    return false;
  }

  try {
    const { response, data } = await postJson('/api/auth/login', {
      email,
      password
    });

    if (!response.ok || !data || data.ok === false) {
      throw new Error(data?.message || '로그인에 실패했습니다.');
    }

    setAuthenticatedUser(data.member);

    showToast('로그인되었습니다.', 'success');
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
    showToast('모든 필드를 입력해주세요.', 'error');
    return false;
  }

  if (!agreeTerms) {
    showToast('약관에 동의해주세요.', 'error');
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

  try {
    const { response, data } = await postJson('/api/auth/signup', {
      name,
      email,
      password,
      confirmPassword,
      agreeTerms
    });

    if (!response.ok || !data || data.ok === false) {
      throw new Error(data?.message || '회원가입에 실패했습니다.');
    }

    setAuthenticatedUser(data.member);

    showToast('회원가입이 완료되었습니다.', 'success');
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

  state.user = null;
  showToast('로그아웃되었습니다.', 'success');

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
      return false;
    }

    setAuthenticatedUser(data.member);
    return true;
  } catch (_error) {
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

async function searchMemberByEmail(email) {
  const normalizedEmail = String(email || '').trim();
  if (!normalizedEmail) {
    throw new Error('이메일을 입력해주세요.');
  }

  const { response, data } = await getJson(
    `/api/friends/search?email=${encodeURIComponent(normalizedEmail)}`
  );

  if (!response.ok || !data || data.ok === false || !data.member) {
    throw new Error(data?.message || '사용자 검색에 실패했습니다.');
  }

  return data.member;
}

async function sendFriendRequest(email) {
  const normalizedEmail = String(email || '').trim();
  if (!normalizedEmail) {
    throw new Error('이메일을 입력해주세요.');
  }

  const { response, data } = await postJson('/api/friends/requests', {
    email: normalizedEmail
  });

  if (!response.ok || !data || data.ok === false) {
    throw new Error(data?.message || '친구 요청 전송에 실패했습니다.');
  }

  return data.request || null;
}

async function loadFriends() {
  const { response, data } = await getJson('/api/friends');

  if (!response.ok || !data || data.ok === false) {
    throw new Error(data?.message || '친구 목록을 불러오지 못했습니다.');
  }

  return Array.isArray(data.friends) ? data.friends : [];
}

async function loadIncomingFriendRequests() {
  const { response, data } = await getJson('/api/friends/requests/incoming');

  if (!response.ok || !data || data.ok === false) {
    throw new Error(data?.message || '받은 요청 목록을 불러오지 못했습니다.');
  }

  return Array.isArray(data.requests) ? data.requests : [];
}

async function loadOutgoingFriendRequests() {
  const { response, data } = await getJson('/api/friends/requests/outgoing');

  if (!response.ok || !data || data.ok === false) {
    throw new Error(data?.message || '보낸 요청 목록을 불러오지 못했습니다.');
  }

  return Array.isArray(data.requests) ? data.requests : [];
}

async function acceptFriendRequest(friendshipId) {
  const normalizedId = Number(friendshipId);
  if (!normalizedId) {
    throw new Error('잘못된 친구 요청 ID입니다.');
  }

  const { response, data } = await postJson(
    `/api/friends/requests/${encodeURIComponent(normalizedId)}/accept`,
    {}
  );

  if (!response.ok || !data || data.ok === false) {
    throw new Error(data?.message || '친구 요청 수락에 실패했습니다.');
  }

  return data.request || null;
}

async function rejectFriendRequest(friendshipId) {
  const normalizedId = Number(friendshipId);
  if (!normalizedId) {
    throw new Error('잘못된 친구 요청 ID입니다.');
  }

  const { response, data } = await postJson(
    `/api/friends/requests/${encodeURIComponent(normalizedId)}/reject`,
    {}
  );

  if (!response.ok || !data || data.ok === false) {
    throw new Error(data?.message || '친구 요청 거절에 실패했습니다.');
  }

  return data.request || null;
}

async function cancelFriendRequest(friendshipId) {
  const normalizedId = Number(friendshipId);
  if (!normalizedId) {
    throw new Error('잘못된 친구 요청 ID입니다.');
  }

  const { response, data } = await postJson(
    `/api/friends/requests/${encodeURIComponent(normalizedId)}/cancel`,
    {}
  );

  if (!response.ok || !data || data.ok === false) {
    throw new Error(data?.message || '친구 요청 취소에 실패했습니다.');
  }

  return data.request || null;
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
  state.ingredients = safeIngredients;
}

function getCachedIngredients() {
  return Array.isArray(state.ingredients) ? state.ingredients : [];
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
      throw new Error(body?.message || '재료 목록을 불러오지 못했습니다.');
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
    throw new Error(body?.message || '재료 등록에 실패했습니다.');
  }

  const created = normalizeIngredientForClient(body.ingredient);
  if (!created) {
    throw new Error('재료 응답 형식이 올바르지 않습니다.');
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
    throw new Error(body?.message || '재료 수정에 실패했습니다.');
  }

  const updated = normalizeIngredientForClient(body.ingredient);
  if (!updated) {
    throw new Error('재료 응답 형식이 올바르지 않습니다.');
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
    throw new Error(body?.message || '재료 삭제에 실패했습니다.');
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
    throw new Error(body?.message || '식품 검색에 실패했습니다.');
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

async function loadCommunityPosts(options = {}) {
  const normalizedOptions = typeof options === 'string'
    ? { filter: options }
    : (options || {});
  const shouldReturnDetailed = typeof options === 'object'
    && options !== null
    && Object.keys(normalizedOptions).length > 0;

  const filter = typeof normalizedOptions.filter === 'string' ? normalizedOptions.filter : 'all';
  const search = typeof normalizedOptions.search === 'string' ? normalizedOptions.search.trim() : '';
  const page = Number.isInteger(normalizedOptions.page) && normalizedOptions.page > 0
    ? normalizedOptions.page
    : 1;
  const size = Number.isInteger(normalizedOptions.size) && normalizedOptions.size > 0
    ? normalizedOptions.size
    : 9;

  const params = new URLSearchParams();
  params.set('type', filter || 'all');
  params.set('page', String(page));
  params.set('size', String(size));
  if (search) {
    params.set('search', search);
  }

  const response = await fetch(`${API_BASE_URL}/api/board/posts?${params.toString()}`, {
    method: 'GET',
    credentials: 'include'
  });

  const body = await parseJsonSafe(response);
  if (!response.ok || !body || body.ok === false) {
    throw new Error(body?.message || '게시글을 불러오지 못했습니다.');
  }

  const posts = Array.isArray(body.posts) ? body.posts : [];
  const pagination = body.pagination && typeof body.pagination === 'object'
    ? body.pagination
    : {};

  state.communityPosts = posts;
  state.communityPagination = {
    page: Number(pagination.page) > 0 ? Number(pagination.page) : 1,
    size: Number(pagination.size) > 0 ? Number(pagination.size) : size,
    totalItems: Number(pagination.totalItems) >= 0 ? Number(pagination.totalItems) : posts.length,
    totalPages: Number(pagination.totalPages) >= 0 ? Number(pagination.totalPages) : 0,
    hasPrev: Boolean(pagination.hasPrev),
    hasNext: Boolean(pagination.hasNext)
  };

  if (!shouldReturnDetailed) {
    return state.communityPosts;
  }

  return {
    posts: state.communityPosts,
    pagination: state.communityPagination
  };
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
    throw new Error(body?.message || '게시글 등록에 실패했습니다.');
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
    throw new Error(body?.message || '게시글 삭제에 실패했습니다.');
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

  showToast('레시피 추천 데이터는 아직 백엔드와 연동되지 않았습니다.', 'info');
  return [];
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
  const levelSource = state.user?.level ?? 1;
  const expSource = state.user?.exp ?? 0;
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
      throw new Error(data?.message || '경험치 업데이트에 실패했습니다.');
    }

    setAuthenticatedUser(data.member);
    const { level: currentLevel } = loadLevelData();
    updateLevelDisplay();

    const awardedExp = Number(data.awardedExp || 0);
    const previousLevel = Number(data.previousLevel || localLevelBefore);
    const newLevel = Number(data.newLevel || currentLevel);

    if (newLevel > previousLevel) {
      showToast(`레벨업! 레벨 ${previousLevel} -> 레벨 ${newLevel}\n${actionName}으로 레벨이 올랐습니다.`, 'success');
    } else if (awardedExp > 0) {
      showToast(`+${awardedExp} 경험치\n${actionName}`, 'success');
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

function normalizeFoodMBTIData(source) {
  if (!source || typeof source !== 'object') {
    return null;
  }

  const type = String(source.type || '').trim();
  const title = String(source.title || '').trim();
  if (!type || !title) {
    return null;
  }

  const description = String(source.description || '').trim();
  const traits = Array.isArray(source.traits)
    ? source.traits.map(item => String(item || '').trim()).filter(Boolean)
    : [];
  const recommendedFoods = Array.isArray(source.recommendedFoods)
    ? source.recommendedFoods.map(item => String(item || '').trim()).filter(Boolean)
    : [];

  const completedAtRaw = String(source.completedAt || source.date || '').trim();
  const completedAt = completedAtRaw || new Date().toISOString();

  return {
    type,
    title,
    description,
    traits,
    recommendedFoods,
    completedAt,
    date: completedAt
  };
}

function saveFoodMBTIToStorage(source) {
  const normalized = normalizeFoodMBTIData(source);
  if (!normalized) {
    return null;
  }
  storage.set('foodMBTI', normalized);
  return normalized;
}

async function saveFoodMBTIToServer(source, options = {}) {
  const normalized = normalizeFoodMBTIData(source);
  if (!normalized || !state.user?.loggedIn) {
    return false;
  }

  const { silent = true } = options;

  try {
    const { response, data } = await putJson('/api/members/me/food-mbti', {
      type: normalized.type,
      title: normalized.title,
      description: normalized.description,
      traits: normalized.traits,
      recommendedFoods: normalized.recommendedFoods,
      completedAt: normalized.completedAt
    });

    if (!response.ok || !data || data.ok === false) {
      throw new Error(data?.message || '음식 MBTI 저장에 실패했습니다.');
    }

    if (data.foodMbti) {
      saveFoodMBTIToStorage(data.foodMbti);
    }

    return true;
  } catch (error) {
    if (!silent) {
      showToast(error.message, 'error');
    }
    return false;
  }
}

async function loadMemberProfile(memberId) {
  const normalizedMemberId = Number(memberId);
  const isTargetMember =
    Number.isInteger(normalizedMemberId) && normalizedMemberId > 0;

  const path = isTargetMember
    ? `/api/members/${encodeURIComponent(normalizedMemberId)}/profile`
    : '/api/members/profile';
  const { response, data } = await getJson(path);

  if (!response.ok || !data || data.ok === false || !data.profile) {
    throw new Error(data?.message || '프로필을 불러오지 못했습니다.');
  }

  const profile = data.profile;

  if (profile.isMe) {
    if (profile.foodMbti) {
      saveFoodMBTIToStorage(profile.foodMbti);
    }
  }

  return profile;
}

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
    description: result.description,
    traits: result.traits,
    recommendedFoods: result.recommendedFoods,
    completedAt: new Date().toISOString()
  };

  saveFoodMBTIToStorage(mbtiData);
  void saveFoodMBTIToServer(mbtiData);
  void addExp('FOOD_MBTI_COMPLETE', '푸드 MBTI 테스트 완료');

  return result;
}

function loadFoodMBTI() {
  return normalizeFoodMBTIData(storage.get('foodMBTI'));
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
    const storedMbti = loadFoodMBTI();
    if (storedMbti) {
      void saveFoodMBTIToServer(storedMbti, { silent: true });
    }
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
  searchMemberByEmail,
  sendFriendRequest,
  loadFriends,
  loadIncomingFriendRequests,
  loadOutgoingFriendRequests,
  acceptFriendRequest,
  rejectFriendRequest,
  cancelFriendRequest,
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
  loadMemberProfile,
  saveFoodMBTI,
  saveFoodMBTIToServer,
  loadFoodMBTI,
  mbtiResults,
  storage,
  state
};
