import axios from 'axios';

import { state, bus } from './globs.js'
window['_state'] = state
window['_bus'] = bus

bus.$on('reload-service', (name) => {
  state.tasks++
  axios.get('/api/service/' + name)
    .then((response) => {
      state.services[name] = response.data
      state.tasks--
    })
    .catch(function (error) {
      console.log('Could not GET service:', error)
      window.location.hash = '#/'
      state.tasks--
    })

})
bus.$on('reload-all', () => {
  state.tasks++
  axios.get('/api/services')
    .then((response) => {
      state.services = response.data
      bus.$emit('reload-all-done')
      state.tasks--
    })
})

bus.$on('patch-service', (name) => {
  state.tasks++
  axios.post('/api/service/' + name + '/config', {
    config: state.services[name].config,
    handler: state.services[name].handler,
  })
    .then((response) => {
      bus.$emit('reload-all')
      state.tasks--
    })
})
bus.$on('startstop-service', (name, running) => {
  state.tasks++
  axios.post('/api/service/' + name + '/running', {running: running})
    .then((response) => {
      bus.$emit('reload-service', name)
      state.tasks--
    })
})
bus.$on('patch-all', (services) => {
  state.tasks++
  axios.post('/api/services', services)
    .then((response) => {
      bus.$emit('reload-all')
      state.tasks--
    })
})

bus.$on('delete-service', (name) => {
  state.tasks++
  axios.delete('/api/service/' + name)
    .then((response) => {
      bus.$emit('reload-all')
      state.tasks--
    })
})
