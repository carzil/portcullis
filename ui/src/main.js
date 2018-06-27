import Vue from 'vue'
import VueRouter from 'vue-router'
import BootstrapVue from 'bootstrap-vue'
import { Navbar, Layout } from 'bootstrap-vue/es/components';

import 'bootstrap/dist/css/bootstrap.css'
import 'bootstrap-vue/dist/bootstrap-vue.css'

import App from './App.vue'
import Service from './Service.vue'

Vue.use(VueRouter)
Vue.use(BootstrapVue);
Vue.use(Navbar);
Vue.use(Layout);

const Home = { template: '<p>Please select service on top</p>' }

const routes = [
  { path: '/', component: Home },
  { path: '/:service', component: Service },
]

const router = new VueRouter({
  routes
})


const app = new Vue({
  el: '#app',
  router,
  template: '<App/>',
  components: { App }
})
