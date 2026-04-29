import { state } from './state.js';
import { storage } from './storage.js';
import { showToast } from './ui.js';

// 사용자 인증
export function login(email, password) {
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

export function signup(name, email, password, confirmPassword) {
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

export function logout() {
  storage.remove('user');
  state.user = null;
  showToast('로그아웃되었습니다.', 'success');

  setTimeout(() => {
    window.location.href = 'index.html';
  }, 1000);
}

export function checkAuth() {
  const user = storage.get('user');
  if (user && user.loggedIn) {
    state.user = user;
    return true;
  }
  return false;
}

export function requireAuth() {
  if (!checkAuth()) {
    window.location.href = 'index.html';
    return false;
  }
  return true;
}
