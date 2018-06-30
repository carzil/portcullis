var state = {
  services: [],
  get servicesMap () {
    let res = {}
    for (let s of this.services)
      res[s.name] = s
    return res
  },

  tasks: 0,
  get loading () {
    return this.tasks > 0
  }
}

import Vue from 'vue'
var bus = new Vue()

export { state, bus }
