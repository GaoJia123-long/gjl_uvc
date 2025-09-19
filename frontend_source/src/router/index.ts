import { createRouter, createWebHistory } from 'vue-router'
import index from '@/pages/index.vue'
import ota from '@/pages/ota.vue'

const router = createRouter({
  history: createWebHistory(import.meta.env.BASE_URL),
  routes: [
    { path: '/', component: index },
    { path: '/ota', component: ota }
  ],
})

export default router
