<template>
  <div id="app">
    <b-navbar toggleable="md" type="dark" variant="info">
      <b-navbar-toggle target="nav_collapse"></b-navbar-toggle>

      <b-navbar-brand href="#">Portcullis</b-navbar-brand>
      <b-collapse is-nav id="nav_collapse">
        <b-navbar-nav>
          <b-nav-item href="#" v-for="s in services" :key="s" :to="s">
            {{ s }}
          </b-nav-item>
        </b-navbar-nav>

        <b-navbar-nav class="ml-auto">
          <b-button size="sm" v-b-modal.new-service>Add new</b-button>
        </b-navbar-nav>
      </b-collapse>
    </b-navbar>
    <b-container fluid class="fullbleed">
      <div v-if="loading">
        Loading...
      </div>
      <router-view></router-view>
    </b-container>

    <b-modal id="new-service" title="New service"
             @ok="newService" @shown="resetForms">
      <b-tabs v-model="forms.tabIndex">
        <b-tab title="Import list" active>
          <br>
          <b-form>
            <b-form-textarea v-model="forms.services"
                             placeholder="Services, one per line, name:port"
                             :rows="6">
            </b-form-textarea>
            <b-form-input v-model="forms.backendIp"
                          placeholder="Backend IP">
            </b-form-input>
          </b-form>
        </b-tab>
        <b-tab title="Create new service">
          <br>
          <b-form>
            <b-form-input v-model="forms.name"
                          placeholder="Service name">
            </b-form-input>
            <b-form-input v-model="forms.port"
                          placeholder="Service port">
            </b-form-input>
            <b-form-input v-model="forms.backendIp"
                          placeholder="Backend IP">
            </b-form-input>
          </b-form>
        </b-tab>
      </b-tabs>
    </b-modal>
  </div>
</template>

<script>
 import axios from 'axios';

 export default {
   name: 'app',
   data () {
     return {
       loading: true,
       services: [],
       forms: {
         tabIndex: 0,
         backendIp: '',
         services: '',
         name: '',
         port: '',
       }
     }
   },
   mounted () {
     this.updateServices()
   },
   methods: {
     updateServices () {
       let that = this
       axios.get('/api/services')
            .then(function (response) {
              that.services = response.data
              that.loading = false
            })
     },
     newService () {
       if (!this.forms.backendIp) return

       let that = this
       let handler = `from portcullis import Splicer

                      class Handler:
                          def __init__(self, ctx, client, backend):
                              self.ctx = ctx
                              self.client = client
                              self.backend = backend
                              self.splicer = Splicer(self.ctx, self.client, self.backend, self.end)
                              self.ctx.start_splicer(self.splicer)

                      def end(self):
                          pass`
       handler = handler.replace(/^ {21}/gm, '')

       let promises = []

       function addPost (name, port) {
         let config = `name = "${name}"
                       host = "localhost"
                       port = "${port}"
                       backlog = 128
                       handler_file = "$WORK_DIR/${name}.handler.py"
                       managed = True
                       protocol = "tcp"
                       backend_host = "${that.forms.backendIp}"
                       backend_port = "${port}"`
         config = config.replace(/^ +/gm, '')
         promises.push(axios.post('/api/services/' + name, {
           config: config,
           handler: handler
         }))
       }

       if (this.forms.services) {
         let services = this.forms.services.match(/[^\r\n]+/g)
         for (let s of services) {
           let [name, port] = s.split(':')
           addPost(name, port)
         }
       } else if (this.forms.name && this.forms.port) {
         addPost(this.forms.name, this.forms.port)
       } else {
         return
       }

       axios.all(promises)
            .then(axios.spread(function (acct, perms) {
              that.updateServices()
            }))
     },
     resetForms () {
       /* this.forms.services = ''
        * this.forms.name = ''
        * this.forms.port = '' */
       /* this.forms.backendIp = '' */
     }
   }
 }
</script>

<style>
 html, body {
   height: 100%;
 }
 #app {
   height: 90%;
 }
 .fullbleed {
   height: 100%;
 }
</style>
