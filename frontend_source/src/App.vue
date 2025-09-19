<template>
 <v-app>
    <v-app-bar color="primary" dark>
      <v-app-bar-title>ESP32 Camera System</v-app-bar-title>
      
      <v-spacer></v-spacer>
      
      <v-btn
        :to="'/'"
        variant="text"
        :class="{ 'v-btn--active': $route.path === '/' }"
      >
        <v-icon left>mdi-video</v-icon>
        Camera
      </v-btn>
      
      <v-btn
        :to="'/ota'"
        variant="text"
        :class="{ 'v-btn--active': $route.path === '/ota' }"
      >
        <v-icon left>mdi-update</v-icon>
        OTA Update
      </v-btn>
    </v-app-bar>
    
    <v-main>
      <router-view />
    </v-main>
  </v-app>
</template>

<script lang="ts" setup>
// App.vue 只需要处理应用级别的逻辑
// 具体的页面逻辑应该在各自的页面组件中
</script>

<style scoped>
/* 应用级别的样式 */
</style>
