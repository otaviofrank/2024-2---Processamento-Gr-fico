Integrantes: Otávio Menezes Frank


Compilado no linux: 

g++ -o game main.cpp Dependencies/GLAD/src/glad.c \
    -I./Dependencies/GLAD/include \
    -I./Dependencies/glfw/include \
    -I./Dependencies/glm \
    -I./Dependencies/stb_image \
    -L./Dependencies/glfw/lib \
    -lglfw -ldl -lGL

