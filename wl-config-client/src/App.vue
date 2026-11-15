<script setup>
import { ref, onMounted, provide } from "vue";
import { RouterView, RouterLink } from "vue-router";
import { Vue3Lottie } from 'vue3-lottie'
import MyAnimation from './assets/RP_Train.json'

let socket = null;
const isConnected = ref(false);

const ssid = ref("WLAN1-2AQTJH");
const pwd = ref("eEb49RmRTM5608fG");

onMounted(() => {
  socket = new WebSocket("ws://192.168.4.1:81/ws");
  console.log("Connecting to ESP32");

  socket.onopen = function () {
    console.log("Connected to ESP32");
    isConnected.value = true;
  };

  socket.onmessage = function (event) {
    console.log("Received: " + event.data);
    const json = JSON.parse(event.data);
  };

  socket.onclose = function () {
    console.log("Connection closed");
    isConnected.value = false;
  };
});

function onConnectPressed({ ssid, password }) {
  if (!socket || socket.readyState !== WebSocket.OPEN) {
    alert("Not connected to ESP32. Please wait...");
    return;
  }

  socket.send(
    JSON.stringify({
      request: "CONNECT",
      payload: {
        ssid,
        password
      }
    })
  );
}
</script>

<template>
  <div v-if="!isConnected">
    <!-- TODO: reconnect on timeout -->
    <div class="center-container">
      <div class="container-loading">
        <Vue3Lottie 
          :animationData="MyAnimation" 
          :height="300" 
          :width="300" 
        />
        <span class="loading-text">
          Connecting...
        </span>
      </div>
    </div>
  </div>
    <div v-else>
      <nav style="display: flex; gap: 12px; margin-bottom: 16px;">
      <RouterLink to="/stop">Stop auswählen</RouterLink>
      <RouterLink to="/wifi">Neues WLAN einstellen</RouterLink>
    </nav>

    <RouterView
      :ssid="ssid"
      :pwd="pwd"
      :isConnected="isConnected"
      :onConnectPressed="onConnectPressed"
      @update:ssid="ssid = $event"
      @update:pwd="pwd = $event"
    />
  </div>
</template>

<style scoped>
  .container-loading {
    display: flex;
    flex-direction: column;
    align-items: center;
  }
  .loading-text {
    transform: translateY(-50px);
    color: rgb(46, 46, 46);
  }
</style>