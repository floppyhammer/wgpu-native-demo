emcmake cmake -B build-web
cmake --build build-web
python3 -m http.server -d bin
# Go to http://localhost:8000/wgpu_native_demo.html
