// Toast 알림
export function showToast(message, type = 'info') {
  const toast = document.getElementById('toast');
  if (!toast) return;

  toast.className = `toast ${type} active`;
  toast.querySelector('.toast-message').textContent = message;

  setTimeout(() => {
    toast.classList.remove('active');
  }, 3000);
}

// 모달 제어
export function openModal(modalId) {
  const modal = document.getElementById(modalId);
  if (modal) {
    modal.classList.add('active');
    document.body.style.overflow = 'hidden';
  }
}

export function closeModal(modalId) {
  const modal = document.getElementById(modalId);
  if (modal) {
    modal.classList.remove('active');
    document.body.style.overflow = 'auto';
  }
}

// 알림 패널 토글
export function toggleNotificationPanel() {
  const panel = document.getElementById('notificationPanel');
  const overlay = document.getElementById('notificationOverlay');

  if (panel && overlay) {
    panel.classList.toggle('active');
    overlay.classList.toggle('active');
  }
}

export function closeNotificationPanel() {
  const panel = document.getElementById('notificationPanel');
  const overlay = document.getElementById('notificationOverlay');

  if (panel && overlay) {
    panel.classList.remove('active');
    overlay.classList.remove('active');
  }
}

export function toggleUserMenu() {
  const dropdown = document.getElementById('userMenuDropdown');
  if (dropdown) {
    dropdown.classList.toggle('active');
  }
}


export function switchTab(tabName) {
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

export function formatDate(dateString) {
  const date = new Date(dateString);
  return date.toLocaleDateString('ko-KR', {
    year: 'numeric',
    month: 'long',
    day: 'numeric'
  });
}



export function activateCurrentNav() {
  const currentPage = window.location.pathname.split('/').pop().replace('.html', '');
  const navLinks = document.querySelectorAll('.nav-link');

  navLinks.forEach(link => {
    if (link.dataset.page === currentPage) {
      link.classList.add('active');
    }
  });
