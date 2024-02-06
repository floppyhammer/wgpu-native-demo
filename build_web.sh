# Set up emsdk environment.
source /home/aicg/emsdk/emsdk_env.sh
# Create a build-web directory in which we configured the project to be built
# with emscripten. You may use any regular cmake command line option here.
emcmake cmake -B build-web
cmake --build build-web
python3 -m http.server -d bin
# Go to http://0.0.0.0:8000/wgpu_native_demo.html
