import { state } from './modules/state.js';
import { storage } from './modules/storage.js';
import {
  showToast,
  openModal,
  closeModal,
  toggleNotificationPanel,
  closeNotificationPanel,
  toggleUserMenu,
  switchTab,
  formatDate,
  activateCurrentNav
} from './modules/ui.js';
import { login, signup, logout, checkAuth, requireAuth } from './modules/auth.js';
import {
  displayUserInfo,
  toggleProfileEdit,
  cancelProfileEdit,
  updateProfile,
  changePassword
} from './modules/profile.js';
import {
  loadIngredients,
  addIngredient,
  deleteIngredient,
  getDaysUntilExpiry,
  getExpiryBadgeClass,
  getExpiryText,
  checkExpiringItems
} from './modules/ingredients.js';
import { loadCommunityPosts, addCommunityPost, getTimeAgo } from './modules/community.js';
import { recommendRecipes } from './modules/recipes.js';
import { loadLevelData, getExpToNextLevel, addExp, updateLevelDisplay } from './modules/level.js';
import { saveFoodMBTI, loadFoodMBTI, mbtiResults } from './modules/food-mbti.js';

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