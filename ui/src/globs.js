var state = {
  services: {},

  tasks: 0,
  get loading () {
    return this.tasks > 0
  }
}

import Vue from 'vue'
var bus = new Vue()

export { state, bus }
