<template>
  <div class="fullbleed">
    <b-row class="fullbleed">
      <b-col cols="6">
        Config
        <AceEditor v-model="state.servicesMap[name].config" lang="python" theme="solarized_light"></AceEditor>
      </b-col>
      <b-col cols="6">
        Handler
        <AceEditor v-model="state.servicesMap[name].handler" lang="python" theme="solarized_light"></AceEditor>
      </b-col>
    </b-row>
  </div>
</template>

<script>
 import { state, bus } from './globs.js'

 import * as AceEditor from 'vue2-ace-editor'
 import 'brace/ext/language_tools'
 import 'brace/mode/python'
 import 'brace/theme/solarized_light'

 export default {
   name: 'service',
   data () {
     return {
       state: state,
       config: '',
       handler: '',
       proxying: true
     }
   },
   beforeRouteEnter (to, from, next) {
     next(vm => {
       vm.getServiceInfo()
     })
   },
   computed: {
     name () {
       return this.$route.params.service
     }
   },
   watch: {
     '$route' (to, from) {
       this.getServiceInfo()
     }
   },
   methods: {
     getServiceInfo() {
       bus.$emit('load-service', this.name)
     }
   },
   components: { AceEditor }
}
</script>
