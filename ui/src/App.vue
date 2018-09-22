<template>
  <div id="app">
    <div v-if="loading" id="loader">
      Loading...
    </div>
    <b-navbar toggleable="md" type="light" variant="warning">
      <b-navbar-toggle target="nav_collapse"></b-navbar-toggle>

      <b-navbar-brand href="#">Portcullis</b-navbar-brand>
      <b-collapse is-nav id="nav_collapse">
        <b-navbar-nav>
          <b-nav-item href="#" v-for="s in services" :key="s.name" :to="s.name">
            {{ s.name }} <b-badge pill :variant="s.proxying ? 'success' : 'danger'">&nbsp;</b-badge>
          </b-nav-item>
          <div class="ml-3">
            <b-button size="m" class="mr-1" v-b-modal.new-service>
              Add new
            </b-button>
            <template v-if="serviceName in state.services">
              <b-button size="m" class="mr-1" @click="saveService" variant="success">
                Save
              </b-button>
              <b-button size="m" class="mr-1" @click="toggleProxying">
                {{ selectedService.proxying ? 'Stop' : 'Start' }}
              </b-button>
              <b-button size="m" class="mr-1" @click="deleteService" variant="danger">
                Delete
              </b-button>
            </template>
          </div>
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
       return this.state.services[this.serviceName]
     }
   },
   mounted () {
     bus.$on('reload-all-done', () => {
       if (!this.serviceName in state.services) {
         this.$router.push('/')
       }
     })
     bus.$emit('reload-all')
   },
   methods: {
     newService () {
       let getServiceObj = (name, port) => ({
         name: name,
         config: `
           host = ""
           port = "${port}"
           backlog = 128
           handler_file = "$WORK_DIR/${name}.handler.py"
           protocol = "tcp"
           backend_ip = "${this.forms.backendIp}"
           backend_ipv6 = ""
           backend_port = "${port}"
           coroutine_stack_size = 4096 * 100
         `.replace(/^ +/gm, ''),
         handler: `
           from portcullis.core import TcpHandle, resolve_v4
           from portcullis.http import HttpHandle

           backend_addr = resolve_v4("tcp://${this.forms.backendIp}:${port}")

           def handler(ctx, clt):
               client = HttpHandle(clt)
               backend = HttpHandle(TcpHandle.connect(ctx, backend_addr))

               request = client.read_request()
               backend.write_request(request)
               client.transfer_body(backend, request)

               ua = request.headers["User-Agent"]
               if ua is not None:
                   request.headers["User-Agent"] = "portcullis"

               response = backend.read_response()
               response.headers["Server"] = "portcullis"

               client.write_response(response)
               backend.transfer_body(client, response)
        `.replace(/^ {10}/gm, '')
       })

       if (this.forms.tabIndex == 0) {
         let services = this.forms.services.match(/[^\r\n]+/g)
         let res = {}
         for (let s of services) {
           let [name, port] = s.split(':')
           res[name] = getServiceObj(name, port)
         }
         bus.$emit('patch-all', res)
       } else if (this.forms.tabIndex == 1) {
         let [name, port] = [this.forms.name, this.forms.port]
         bus.$emit('patch-service', getServiceObj(name, port))
       }
     },
     resetForms () {
       /* this.forms.services = ''
        * this.forms.name = ''
        * this.forms.port = '' */
       /* this.forms.backendIp = '' */
     },
     saveService () {
       bus.$emit('patch-service', this.serviceName)
     },
     toggleProxying () {
       bus.$emit(
         'startstop-service',
         this.serviceName,
         !this.state.services[this.serviceName].proxying,
       )
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
