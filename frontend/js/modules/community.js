import { state } from './state.js';
import { storage } from './storage.js';
import { addExp } from './level.js';

export function loadCommunityPosts() {
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

export function saveCommunityPosts(posts) {
  storage.set('communityPosts', posts);
  state.communityPosts = posts;
}

export function addCommunityPost(post) {
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

export function getTimeAgo(dateString) {
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
