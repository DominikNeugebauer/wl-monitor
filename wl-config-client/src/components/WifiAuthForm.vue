<template>
  <div class="center-container">
    <div class="wlan-form">
      <h1>WLAN verbinden</h1>
      <p class="subtitle">
        Bitte geben Sie den Namen Ihres WLAN-Netzwerks (SSID) sowie das dazugehörige Passwort ein, damit das Gerät eine Verbindung herstellen und auf das Internet zugreifen kann.
      </p>

      <label>Netzwerkname (SSID)</label>
      <input
        v-model="localSsid"
        type="text"
        placeholder="z. B. MeinWLAN"
      />

      <label>Passwort</label>
      <input
        v-model="localPwd"
        type="password"
        placeholder="WLAN-Passwort"
      />

      <button :disabled="!localPwd.length || !localSsid.length" @click="onConnectPressed">
        Verbinden
      </button>
    </div>
  </div>
</template>

<script setup>
import { ref, watch } from "vue";

const props = defineProps({
  ssid: String,
  pwd: String,
});

const emit = defineEmits(["update:ssid", "update:pwd", "connect"]);

const localSsid = ref(props.ssid || "");
const localPwd = ref(props.pwd || "");

watch(() => props.ssid, (val) => {
  localSsid.value = val || "";
});

watch(() => props.pwd, (val) => {
  localPwd.value = val || "";
});

watch(localSsid, (val) => emit("update:ssid", val));
watch(localPwd, (val) => emit("update:pwd", val));

function onConnectPressed() {
  if (!props.isConnected) {
    alert("Gerät ist noch nicht verbunden. Bitte warten...");
    return;
  }

  if (!localSsid.value || !localPwd.value) {
    alert("Bitte geben Sie WLAN-Name und Passwort ein.");
    return;
  }

  emit("connect", {
    ssid: localSsid.value,
    password: localPwd.value
  });
}
</script>

<style scoped>
.wlan-form {
  display: flex;
  flex-direction: column;
  gap: 12px;
  width: 320px;
  padding: 32px 28px;
  border-radius: 12px;
  background: white;
  box-shadow: 0 10px 25px rgba(0, 0, 0, 0.08);
}
</style>