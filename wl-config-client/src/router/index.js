import { createRouter, createWebHashHistory } from "vue-router";
import WifiView from "../components/WifiView.vue";
import StopView from "../components/StopView.vue";

const routes = [
  {
    path: "/",
    redirect: "/stop"
  },
  {
    path: "/wifi",
    name: "wifi",
    component: WifiView
  },
  {
    path: "/stop",
    name: "stop",
    component: StopView
  }
];

const router = createRouter({
  history: createWebHashHistory(),
  routes
});

export default router;