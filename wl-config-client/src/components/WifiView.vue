<template>
  <WifiAuthForm
    :ssid="ssid"
    :pwd="pwd"
    :isConnected="isConnected"
    @update:ssid="emit('update:ssid', $event)"
    @update:pwd="emit('update:pwd', $event)"
    @connect="onConnectPressed"
  />
</template>

<script setup>
import WifiAuthForm from "../components/WifiAuthForm.vue";

defineProps({
  ssid: String,
  pwd: String,
  isConnected: Boolean,
  onConnectPressed: Function
});

const emit = defineEmits(["update:ssid", "update:pwd"]);
</script>