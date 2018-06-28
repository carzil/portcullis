# How to build Portcullis
```
mkdir build
cd build
cmake ..
make
```

# How to run Portcullis
```
portcullis <path-to-config>
```

# How to run Portcullis UI
```
# dev
cd admin
FLASK_DEBUG=1 flask run
cd ../ui
npm run dev

# prod
cd ui
npm run build
cd ../admin
flask run -h 0.0.0.0
```
