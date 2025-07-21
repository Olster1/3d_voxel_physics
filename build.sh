clear
clear
# -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -g
g++ -std=c++11 -o Minecraft ./platform_layer.cpp -Wno-deprecated-declarations -Wno-writable-strings -Wno-c++11-compat-deprecated-writable-strings -rpath /Library/Frameworks -F/Library/Frameworks -framework OpenGL -framework SDL2