<template>
  <v-container max-width="800">
    <v-card>
      <template #title>
        <div class="text-center">
          <span style="margin-right: 8px;">🔄</span>
          OTA Firmware Update
        </div>
      </template>
      
      <template #text>
        <v-alert v-if="updateStatus" :type="updateStatus.type" class="mb-4">
          {{ updateStatus.message }}
        </v-alert>

        <v-file-input
          v-model="selectedFile"
          label="Select firmware file (.bin)"
          accept=".bin"
          prepend-icon="📁"
          variant="outlined"
          :disabled="isUpdating"
          show-size
          truncate-length="15"
        />

        <v-card-text>
          <h4>Current Firmware Info:</h4>
          <v-list>
            <v-list-item>
              <v-list-item-title>Version:</v-list-item-title>
              <v-list-item-subtitle>{{ firmwareInfo.version || 'Unknown' }}</v-list-item-subtitle>
            </v-list-item>
            <v-list-item>
              <v-list-item-title>Build Date:</v-list-item-title>
              <v-list-item-subtitle>{{ firmwareInfo.buildDate || 'Unknown' }}</v-list-item-subtitle>
            </v-list-item>
            <v-list-item>
              <v-list-item-title>Free Space:</v-list-item-title>
              <v-list-item-subtitle>{{ firmwareInfo.freeSpace || 'Unknown' }}</v-list-item-subtitle>
            </v-list-item>
          </v-list>
        </v-card-text>

        <v-progress-linear
          v-if="isUpdating"
          :model-value="uploadProgress"
          color="primary"
          height="25"
          rounded
        >
          <template #default="{ value }">
            <strong>{{ Math.ceil(value) }}%</strong>
          </template>
        </v-progress-linear>

        <div class="d-flex justify-space-between mt-4">
          <v-btn
            color="primary"
            variant="outlined"
            @click="goBack"
            :disabled="isUpdating"
          >
            <span style="margin-right: 8px;">←</span>
            Back to Camera
          </v-btn>
          
          <v-btn
            color="primary"
            :disabled="!selectedFile || isUpdating"
            @click="startUpdate"
            :loading="isUpdating"
          >
            <span style="margin-right: 8px;">⬆</span>
            Start Update
          </v-btn>
        </div>
      </template>
    </v-card>
  </v-container>
</template>

<script lang="ts" setup>
import { ref, onMounted } from 'vue'
import { useRouter } from 'vue-router'

const router = useRouter()

// 修改：使用单个File对象而不是File数组
const selectedFile = ref<File | null>(null)
const isUpdating = ref(false)
const uploadProgress = ref(0)
const updateStatus = ref<{ type: 'success' | 'error' | 'info', message: string } | null>(null)

const firmwareInfo = ref({
  version: '',
  buildDate: '',
  freeSpace: ''
})

const goBack = () => {
  router.push('/')
}

const startUpdate = async () => {
  console.log('startUpdate called')
  console.log('selectedFile.value:', selectedFile.value)
  
  if (!selectedFile.value) {
    console.log('No file selected')
    updateStatus.value = { type: 'error', message: 'Please select a firmware file' }
    return
  }

  const file = selectedFile.value
  console.log('Selected file:', file.name, file.size)
  
  if (!file.name.endsWith('.bin')) {
    console.log('Invalid file type')
    updateStatus.value = { type: 'error', message: 'Please select a valid .bin firmware file' }
    return
  }

  console.log('Starting update process...')
  isUpdating.value = true
  uploadProgress.value = 0
  updateStatus.value = { type: 'info', message: 'Starting firmware update...' }

  try {
    console.log('Reading file as ArrayBuffer...')
    const arrayBuffer = await file.arrayBuffer()
    console.log('File size:', arrayBuffer.byteLength)
    
    console.log('Sending request to /api/ota/update...')
    const response = await fetch('/api/ota/update', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/octet-stream',
        'Content-Length': arrayBuffer.byteLength.toString()
      },
      body: arrayBuffer
    })

    console.log('Response received:', response.status)

    if (!response.ok) {
      throw new Error(`Update failed: ${response.statusText}`)
    }

    // 模拟上传进度
    const interval = setInterval(() => {
      if (uploadProgress.value < 90) {
        uploadProgress.value += Math.random() * 10
      }
    }, 200)

    // 等待更新完成
    await new Promise(resolve => setTimeout(resolve, 3000))
    
    clearInterval(interval)
    uploadProgress.value = 100
    
    updateStatus.value = { 
      type: 'success', 
      message: 'Firmware update completed successfully! The device will restart in 5 seconds.' 
    }

    // 5秒后跳转回主页
    setTimeout(() => {
      router.push('/')
    }, 5000)

  } catch (error) {
    console.error('Update error:', error)
    updateStatus.value = { 
      type: 'error', 
      message: `Update failed: ${error instanceof Error ? error.message : 'Unknown error'}` 
    }
  } finally {
    isUpdating.value = false
  }
}

const fetchFirmwareInfo = async () => {
  try {
    const response = await fetch('/api/ota/info')
    if (response.ok) {
      const data = await response.json()
      firmwareInfo.value = data
    }
  } catch (error) {
    console.error('Failed to fetch firmware info:', error)
  }
}

onMounted(() => {
  fetchFirmwareInfo()
})
</script>

<style scoped>
.text-center {
  text-align: center;
}
</style>
