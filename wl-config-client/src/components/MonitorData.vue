<script setup>
import { ref, watch, onMounted, computed } from 'vue'

const props = defineProps({
  stop: { type: Object, required: true }
})

const monitorData = ref(null)
const isLoading = ref(false)
const error = ref(null)
const selectedOption = ref(null)

// All line and direction options from the API response
const availableLines = computed(() => {
  if (!monitorData.value || !monitorData.value.monitors) return []
  
  const options = []
  
  monitorData.value.monitors.forEach(monitor => {
    monitor.lines.forEach(line => {
      options.push({
        id: `${line.name}-${line.richtungsId}-${monitor.locationStop.properties.attributes.rbl}`,
        name: line.name,
        towards: line.towards,
        richtungsId: line.richtungsId,
        // RBL number (stop ID) used by the ESP32
        rbl: monitor.locationStop.properties.attributes.rbl
      })
    })
  })
  
  return options
})

// Direct Wiener Linien URL for the selected option
const finalApiUrl = computed(() => {
  if (!selectedOption.value) return ''
  const base = 'https://www.wienerlinien.at/ogd_realtime/monitor'
  return `${base}?stopId=${selectedOption.value.rbl}&line=${selectedOption.value.name}`
})

const fetchMonitorData = async () => {
  if (!props.stop) return
  isLoading.value = true
  error.value = null
  selectedOption.value = null 
  
  // Goes through the Vite proxy (/api to wienerlinien.at)
  const param = props.stop.DIVA ? `diva=${props.stop.DIVA}` : `stopId=${props.stop.StopID}`
  const url = `/api/ogd_realtime/monitor?${param}`

  try {
    const response = await fetch(url)
    if (!response.ok) throw new Error(`Status ${response.status}`)
    const json = await response.json()
    monitorData.value = json.data
  } catch (err) {
    error.value = "Fehler: " + err.message
  } finally {
    isLoading.value = false
  }
}

const copyToClipboard = (text) => {
  navigator.clipboard.writeText(text)
  alert("URL in Zwischenablage kopiert!")
}

watch(() => props.stop, fetchMonitorData, { deep: true })
onMounted(fetchMonitorData)
</script>

<template>
  <div class="monitor-container">
    <div class="header">
      <div class="title-group">
        <span class="station-label">Ausgewählte Station:</span>
        <h3>{{ stop.StopText }}</h3>
      </div>
      <button @click="fetchMonitorData" :disabled="isLoading" class="refresh-btn" title="Neu laden">
        {{ isLoading ? '...' : 'R' }}
      </button>
    </div>

    <div v-if="availableLines.length > 0" class="selection-area">
      <p class="instruction">Bitte wähle die gewünschte Linie und Richtung:</p>
      <div class="line-grid">
        <button 
          v-for="opt in availableLines" 
          :key="opt.id"
          class="line-button"
          :class="{ 'active': selectedOption?.id === opt.id }"
          @click="selectedOption = opt"
        >
          <span class="line-badge">{{ opt.name }}</span>
          <span class="direction-text">
            <strong>{{ opt.towards }}</strong>
            <small>Richtung {{ opt.richtungsId }} (RBL: {{ opt.rbl }})</small>
          </span>
        </button>
      </div>
    </div>

    <div v-if="selectedOption" class="config-section">
      <div class="config-card">
        <h4>ESP32 Konfiguration</h4>
        <div class="config-details">
          <div class="detail-item"><span>RBL / StopID:</span> <strong>{{ selectedOption.rbl }}</strong></div>
          <div class="detail-item"><span>Linie:</span> <strong>{{ selectedOption.name }}</strong></div>
          <div class="detail-item"><span>Richtung:</span> <strong>{{ selectedOption.richtungsId }}</strong></div>
        </div>

        <div class="endpoint-box">
          <label>Direkter API-Endpunkt (Postman/ESP32):</label>
          <div class="url-display">
            <code>{{ finalApiUrl }}</code>
            <button @click="copyToClipboard(finalApiUrl)" class="copy-icon">C</button>
          </div>
        </div>
      </div>
    </div>

    <div v-if="error" class="error-msg">{{ error }}</div>
    <div v-else-if="!isLoading && availableLines.length === 0" class="no-data">
      ⚠️ Keine aktiven Linien für diese Station gefunden.
    </div>
  </div>
</template>

<style scoped>
.monitor-container {
  margin-top: 20px;
  padding: 24px;
  background: #ffffff;
  border-radius: 16px;
  box-shadow: 0 10px 25px rgba(0,0,0,0.08);
  border: 1px solid #eaeaea;
  font-family: 'Inter', -apple-system, sans-serif;
}

.header {
  display: flex;
  justify-content: space-between;
  align-items: flex-start;
  margin-bottom: 24px;
}

.station-label {
  font-size: 11px;
  text-transform: uppercase;
  letter-spacing: 1px;
  color: #888;
}

h3 { margin: 4px 0 0 0; font-size: 20px; color: #1a1a1a; }

.instruction { font-size: 14px; color: #555; margin-bottom: 12px; }

.line-grid {
  display: flex;
  flex-direction: column;
  gap: 10px;
}

.line-button {
  display: flex;
  align-items: center;
  padding: 14px;
  border: 1px solid #e0e0e0;
  border-radius: 12px;
  background: #fff;
  cursor: pointer;
  transition: all 0.2s ease;
  text-align: left;
}

.line-button:hover {
  border-color: #007bff;
  background: #f8fbff;
}

.line-button.active {
  background: #007bff;
  border-color: #0056b3;
  color: white;
  box-shadow: 0 4px 12px rgba(0,123,255,0.2);
}

.line-badge {
  background: #f0f0f0;
  color: #333;
  padding: 4px 10px;
  border-radius: 6px;
  font-weight: 800;
  margin-right: 15px;
  min-width: 45px;
  text-align: center;
  font-size: 14px;
}

.active .line-badge { background: rgba(255,255,255,0.2); color: white; }

.direction-text { display: flex; flex-direction: column; line-height: 1.2; }
.direction-text small { font-size: 11px; opacity: 0.7; margin-top: 2px; }

/* Config Section */
.config-section {
  margin-top: 30px;
  animation: slideUp 0.3s ease-out;
}

.config-card {
  background: #fcfcfc;
  border: 1px solid #eee;
  padding: 20px;
  border-radius: 14px;
}

.config-card h4 { margin: 0 0 15px 0; font-size: 15px; color: #333; }

.config-details {
  display: flex;
  gap: 20px;
  margin-bottom: 20px;
  font-size: 13px;
}

.detail-item span { color: #777; margin-right: 5px; }

.endpoint-box {
  background: #fff3cd;
  padding: 15px;
  border-radius: 10px;
  border: 1px solid #ffeeba;
}

.endpoint-box label {
  display: block;
  font-size: 11px;
  font-weight: bold;
  color: #856404;
  margin-bottom: 8px;
  text-transform: uppercase;
}

.url-display {
  display: flex;
  background: white;
  padding: 10px;
  border-radius: 6px;
  border: 1px solid #ddd;
  align-items: center;
  gap: 10px;
}

code {
  font-family: 'Fira Code', monospace;
  font-size: 11px;
  color: #c7254e;
  word-break: break-all;
  flex: 1;
}

.copy-icon {
  background: none;
  border: none;
  cursor: pointer;
  font-size: 18px;
  padding: 0;
  transition: transform 0.1s;
}

.copy-icon:active { transform: scale(0.9); }

.error-msg { margin-top: 15px; color: #d93025; background: #ffeeee; padding: 12px; border-radius: 8px; font-size: 14px; }
.no-data { text-align: center; padding: 40px; color: #999; }

@keyframes slideUp {
  from { opacity: 0; transform: translateY(10px); }
  to { opacity: 1; transform: translateY(0); }
}

.refresh-btn {
  background: white;
  border: 1px solid #ddd;
  border-radius: 50%;
  width: 36px;
  height: 36px;
  display: flex;
  align-items: center;
  justify-content: center;
  cursor: pointer;
  box-shadow: 0 2px 4px rgba(0,0,0,0.05);
}
.refresh-btn:hover { background: #f5f5f5; }
</style>