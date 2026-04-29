import { state } from './state.js';
import { storage } from './storage.js';
import { showToast } from './ui.js';
import { addExp } from './level.js';

export function loadIngredients() {
  const ingredients = storage.get('ingredients') || [];
  state.ingredients = ingredients;
  return ingredients;
}

export function saveIngredients(ingredients) {
  storage.set('ingredients', ingredients);
  state.ingredients = ingredients;
}

export function addIngredient(ingredient) {
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

export function deleteIngredient(id) {
  const ingredients = loadIngredients();
  const filtered = ingredients.filter(item => item.id !== id);
  saveIngredients(filtered);
  showToast('재료가 삭제되었습니다.', 'success');
}

export function getDaysUntilExpiry(expiryDate) {
  const today = new Date();
  today.setHours(0, 0, 0, 0);
  const expiry = new Date(expiryDate);
  expiry.setHours(0, 0, 0, 0);
  const diffTime = expiry.getTime() - today.getTime();
  const diffDays = Math.ceil(diffTime / (1000 * 60 * 60 * 24));
  return diffDays;
}

export function getExpiryBadgeClass(expiryDate) {
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

export function getExpiryText(expiryDate) {
  const days = getDaysUntilExpiry(expiryDate);

  if (days < 0) {
    return '유통기한 만료';
  } else {
    return `D-${days}`;
  }
}

export function checkExpiringItems() {
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
