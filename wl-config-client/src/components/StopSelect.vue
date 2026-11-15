<script setup>
import { ref, computed, onMounted, onUnmounted } from 'vue'
import MonitorData from './MonitorData.vue'
import stops from '../data/wl-stops.js'

const searchQuery = ref('')
const isDropdownOpen = ref(false)
const selectedStop = ref(null)
const containerRef = ref(null)

const filteredStops = computed(() => {
  const query = searchQuery.value.toLowerCase().trim()
  
  if (!query) {
    return stops.filter(s => s.StopText)
  }
  
  return stops.filter(stop => 
    stop.StopText && stop.StopText.toLowerCase().includes(query)
  )
})

const selectStop = (stop) => {
  selectedStop.value = stop
  searchQuery.value = stop.StopText
  isDropdownOpen.value = false
}

const clearSelection = () => {
  selectedStop.value = null
  searchQuery.value = ''
  isDropdownOpen.value = false
}

const handleClickOutside = (event) => {
  if (containerRef.value && !containerRef.value.contains(event.target)) {
    isDropdownOpen.value = false
  }
}

onMounted(() => document.addEventListener('click', handleClickOutside))
onUnmounted(() => document.removeEventListener('click', handleClickOutside))
</script>

<template>
  <div class="autocomplete-wrapper" ref="containerRef">
    <label for="stop-search" class="label">Haltestelle auswählen:</label>
    
    <div class="input-container">
      <input 
        id="stop-search"
        type="text" 
        v-model="searchQuery" 
        @focus="isDropdownOpen = true"
        placeholder="Suchen..."
        autocomplete="off"
      />
      
      <button 
        v-if="searchQuery" 
        class="clear-btn" 
        @click="clearSelection"
        type="button"
        title="Löschen"
      >
        &times;
      </button>

      <ul v-if="isDropdownOpen" class="dropdown">
        <li 
          v-for="stop in filteredStops" 
          :key="stop.StopID" 
          @click="selectStop(stop)"
          :class="{ 'active': selectedStop?.StopID === stop.StopID }"
        >
          {{ stop.StopText }}
        </li>
        <li v-if="filteredStops.length === 0" class="no-data">
          Keine Ergebnisse
        </li>
      </ul>
    </div>

    <div v-if="selectedStop" class="json-dump-container">
      <div class="dump-header">
        <span>Raw Data:</span>
      </div>
      <pre class="json-output">{{ JSON.stringify(selectedStop, null, 2) }}</pre>
    </div>
  </div>

  <div v-if="selectedStop">
    <MonitorData :stop="selectedStop" />
  </div>
</template>

<style scoped>
.autocomplete-wrapper {
  position: relative;
  width: 100%;
  max-width: 500px;
  margin: 0 auto;
  font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
}

.label {
  display: block;
  margin-bottom: 6px;
  font-size: 14px;
  font-weight: 500;
  color: #444;
}

.input-container {
  position: relative;
  width: 100%;
}

input {
  width: 100%;
  padding: 10px 35px 10px 12px;
  font-size: 15px;
  border: 1px solid #ccc;
  border-radius: 6px;
  box-sizing: border-box;
  background-color: #fff;
}

input:focus {
  outline: none;
  border-color: #007bff;
  box-shadow: 0 0 0 2px rgba(0,123,255,0.1);
}

.clear-btn {
  position: absolute;
  right: 8px;
  top: 50%;
  transform: translateY(-50%);
  background: #f0f0f0;
  border: none;
  width: 20px;
  height: 20px;
  border-radius: 50%;
  cursor: pointer;
  display: flex;
  align-items: center;
  justify-content: center;
  color: #666;
  font-size: 16px;
  padding: 0;
}

.clear-btn:hover {
  background: #e0e0e0;
  color: #000;
}

.dropdown {
  position: absolute;
  top: 100%; 
  left: 0;
  width: 100%;
  max-height: 250px;
  overflow-y: auto;
  background: white;
  border: 1px solid #ccc;
  border-top: none;
  border-radius: 0 0 6px 6px;
  z-index: 1000;
  margin: 0;
  padding: 0;
  list-style: none;
  box-shadow: 0 4px 12px rgba(0,0,0,0.1);
}

/* Suggestion text */
.dropdown li {
  padding: 10px 15px;
  cursor: pointer;
  font-size: 14px;
  color: #333;
  border-bottom: 1px solid #f9f9f9;
}

.dropdown li:last-child {
  border-bottom: none;
}

.dropdown li:hover {
  background-color: #f0f7ff;
}

.dropdown li.active {
  background-color: #e7f3ff;
  font-weight: 600;
}

.no-data {
  padding: 15px;
  text-align: center;
  color: #999;
  font-size: 14px;
}

/* JSON dump */
.json-dump-container {
  margin-top: 15px;
  background: #1e1e1e;
  color: #9cdcfe;
  border-radius: 6px;
  padding: 12px;
  font-size: 12px;
}

.dump-header {
  color: #888;
  margin-bottom: 8px;
  font-size: 11px;
  text-transform: uppercase;
  letter-spacing: 0.5px;
}

.json-output {
  margin: 0;
  white-space: pre-wrap;
  word-wrap: break-word;
  font-family: 'Consolas', 'Monaco', monospace;
}
</style>