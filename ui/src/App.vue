<template>
  <div id="app">
    <div v-if="loading" id="loader">
      Loading...
    </div>
    <b-navbar toggleable="md" type="dark" variant="info">
      <b-navbar-toggle target="nav_collapse"></b-navbar-toggle>

      <b-navbar-brand href="#">Portcullis</b-navbar-brand>
      <b-collapse is-nav id="nav_collapse">
        <b-navbar-nav>
          <b-nav-item href="#" v-for="s in services" :key="s.name" :to="s.name">
            {{ s.name }} <b-badge pill :variant="s.proxying ? 'success' : 'danger'">&nbsp;</b-badge>
          </b-nav-item>
        </b-navbar-nav>

        <b-navbar-nav class="ml-auto">
          <div v-if="serviceName in state.servicesMap">
            <b-button size="sm" class="mr-1" @click="saveService" variant="success">
              Save
            </b-button>
            <b-button size="sm" class="mr-1" @click="toggleProxying">
              {{ selectedService.proxying ? 'Disable' : 'Enable' }}
            </b-button>
            <b-button size="sm" class="mr-1" @click="deleteService" variant="danger">
              Delete
            </b-button>
          </div>
          <b-button size="sm" class="mr-1" v-b-modal.new-service>
            Add new
          </b-button>
        </b-navbar-nav>
      </b-collapse>
    </b-navbar>
    <b-container fluid class="fullbleed">
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
 import { state, bus } from './globs.js'

 export default {
   name: 'app',
   data () {
     return {
       state: state,
       forms: {
         tabIndex: 0,
         backendIp: '',
         services: '',
         name: '',
         port: '',
       }
     }
   },
   computed: {
     loading () {
       return this.state.loading
     },
     services () {
       return this.state.services
     },
     serviceName () {
       return this.$route.params.service
     },
     selectedService () {
       return this.state.servicesMap[this.serviceName]
     }
   },
   mounted () {
     bus.$on('loaded-services', () => {
       if (!this.serviceName in state.servicesMap) {
         this.$router.push('/')
       }
     })
     bus.$emit('load-services')
   },
   methods: {
     newService () {
       let handler = `from portcullis import Splicer

                      class Handler:
                          def __init__(self, ctx, client, backend):
                              self.ctx = ctx
                              self.client = client
                              self.backend = backend
                              self.splicer = Splicer(self.ctx, self.client, self.backend, self.end)
                              self.ctx.start_splicer(self.splicer)

                          def end(self):
                              pass
                      `
       handler = handler.replace(/^ {21}/gm, '')

       let promises = []

       let getServiceObj = (name, port) => ({
         name: name,
         config: `name = "${name}"
                  host = "localhost"
                  port = "${port}"
                  backlog = 128
                  handler_file = "$WORK_DIR/${name}.handler.py"
                  managed = True
                  protocol = "tcp"
                  backend_host = "${this.forms.backendIp}"
                  backend_port = "${port}"
                  `.replace(/^ +/gm, ''),
         handler: handler
       })

       if (this.forms.tabIndex == 0) {
         let services = this.forms.services.match(/[^\r\n]+/g)
         let res = []
         for (let s of services) {
           let [name, port] = s.split(':')
           res.push(getServiceObj(name, port))
         }
         bus.$emit('update-services', res)
       } else if (this.forms.tabIndex == 1) {
         addPost(this.forms.name, this.forms.port)
         bus.$emit('update-service', getServiceObj(name, port))
       }
     },
     resetForms () {
       /* this.forms.services = ''
        * this.forms.name = ''
        * this.forms.port = '' */
       /* this.forms.backendIp = '' */
     },
     saveService () {
       bus.$emit('update-service', {
         name: this.serviceName,
         config: this.config,
         handler: this.handler
       })
     },
     toggleProxying () {
       bus.$emit('update-service', {
         name: this.serviceName,
         proxying: !this.state.servicesMap[this.serviceName].proxying
       })
     },
     deleteService () {
       if (confirm(`Are you sure you want to delete service ${this.serviceName}?!!1`)) {
         bus.$emit('delete-service', this.serviceName)
         this.$router.push('/')
       }
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
 #loader {
   position: fixed;
   width: 100%;
   height: 100%;
   top: 0;
   left: 0;
   right: 0;
   bottom: 0;
   background-color: rgba(0,0,0,0.5);
   z-index: 100;

   display: -webkit-flexbox;
   display: -ms-flexbox;
   display: -webkit-flex;
   display: flex;
   -webkit-flex-align: center;
   -ms-flex-align: center;
   -webkit-align-items: center;
   align-items: center;
   justify-content: center;

   font-size: 40px;
   color: #eee;
 }
</style>
