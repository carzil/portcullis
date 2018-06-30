import axios from 'axios';

import { state, bus } from './globs.js'
window['_state'] = state
window['_bus'] = bus

bus.$on('load-services', () => {
  state.tasks++
  axios.get('/api/services')
    .then((response) => {
      state.services = response.data
      state.tasks--
      bus.$emit('loaded-services')
    })
})
bus.$on('update-service', (service) => {
  state.tasks++
  axios.post('/api/services/' + service.name, service)
    .then((response) => {
      state.tasks--
    })
})
bus.$on('update-services', (services) => {
  state.tasks++
  let promises = []
  for (let service of services) {
    promises.push(axios.post('/api/services/' + service.name, service))
  }
  axios.all(promises)
    .then(axios.spread((acct, perms) => {
      state.tasks--
      bus.$emit('load-services')
    }))
})
bus.$on('delete-service', (name) => {
  state.tasks++
  axios.delete('/api/services/' + name)
    .then((response) => {
      state.tasks--
      bus.$emit('load-services')
    })
})
bus.$on('load-service', (name) => {
  axios.get('/api/services/' + this.name)
    .then((response) => {
      this.config = response.data.config
      this.handler = response.data.handler
      this.proxying = response.data.proxying
      this.$emit('loaded')
    })
})
