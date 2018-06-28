<template>
  <div class="fullbleed">
    <b-row class="fullbleed">
      <b-col cols="2">
        <b-list-group>
          <b-list-group-item button @click="save('prestable')">
            Save to prestable
          </b-list-group-item>
          <b-list-group-item button @click="save('prod')">
            Save to prod
          </b-list-group-item>
          <b-list-group-item button @click="toggleProxying">
            {{ proxying ? 'Disable' : 'Enable' }} proxying
          </b-list-group-item>
          <b-list-group-item button @click="undo">
            Undo
          </b-list-group-item>
        </b-list-group>
      </b-col>
      <b-col cols="5">
        Config
        <AceEditor v-model="config" lang="python" theme="solarized_light"></AceEditor>
      </b-col>
      <b-col cols="5">
        Handler
        <AceEditor v-model="handler" lang="python" theme="solarized_light"></AceEditor>
      </b-col>
    </b-row>
  </div>
</template>

<script>
 import axios from 'axios';

 import * as AceEditor from 'vue2-ace-editor'
 import 'brace/ext/language_tools'
 import 'brace/mode/python'
 import 'brace/theme/solarized_light'

 export default {
   name: 'service',
   data () {
     return {
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
       this.$emit('loading')
       let that = this
       axios.get('/api/services/' + this.name)
            .then((response) => {
              that.config = response.data.config
              that.handler = response.data.handler
              that.$emit('loaded')
            }, (error) => {
              that.$router.push('/')
            })
     },
     save (target) {
       this.loading = true
       let that = this
       axios.post('/api/services/' + this.name, {
         config: this.config,
         handler: this.handler
       })
            .then((response) => {
              that.$emit('loaded')
            })
     },
     toggleProxying () {
       this.proxying ^= true
     },
     undo () {
       console.log('undo')
     }
   },
   components: { AceEditor }
}
</script>
