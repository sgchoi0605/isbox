import { state } from './state.js';
import { storage } from './storage.js';
import { showToast } from './ui.js';


export function displayUserInfo() {
  const userNameEl = document.getElementById('userName');
  const userEmailEl = document.getElementById('userEmail');

  if (state.user) {
    if (userNameEl) userNameEl.textContent = state.user.name;
    if (userEmailEl) userEmailEl.textContent = state.user.email;
  }
}



export function toggleProfileEdit() {
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

export function cancelProfileEdit() {
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

export function updateProfile() {
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

export function changePassword() {
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
