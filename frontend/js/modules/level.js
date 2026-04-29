import { state } from './state.js';
import { storage } from './storage.js';
import { showToast } from './ui.js';

// 레벨 시스템
export function loadLevelData() {
  const level = storage.get('userLevel') || 1;
  const exp = storage.get('userExp') || 0;
  state.level = level;
  state.exp = exp;
  return { level, exp };
}

export function getExpToNextLevel(level) {
  return level * 100;
}

export function addExp(amount, actionName) {
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

export function updateLevelDisplay() {
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

