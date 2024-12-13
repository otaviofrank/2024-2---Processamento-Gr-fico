#include <iostream>
#include <string>
#include <assert.h>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <ctime>

// Incluindo as bibliotecas externas necessárias
#include <glad/glad.h>
#include <GLFW/glfw3.h>

// Incluindo GLM para manipulação de matrizes e vetores
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

using namespace std;
using namespace glm;

// ---------- Declarações de estruturas e enums ----------

// Estrutura de dados para sprites
struct Sprite {
    GLfloat VAO;     // id do buffer de geometria (Vertex Array Object)
    GLfloat texID;   // id da textura
    vec3 pos;        // posição do sprite na cena
    vec3 dimensions; // dimensões do sprite
    float angle;     // ângulo de rotação

    // Parâmetros para animação (spritesheet)
    int nAnimations;
    int nFrames;
    int iAnimation;
    int iFrame;
    float ds, dt; // incrementos de textura (s,t) para cada frame/animação

    // Parâmetro para movimentação
    float vel;

    // Para colisão AABB (Axis-Aligned Bounding Box)
    vec2 PMax, PMin;
};

// Estados do sprite do personagem
enum sprites_states {
    IDLE = 1,
    MOVING_LEFT,
    MOVING_RIGHT
};

// ---------- Declarações de funções e variáveis globais ----------

// Função de callback do teclado
void key_callback(GLFWwindow *window, int key, int scancode, int action, int mode);

// Funções auxiliares de setup
int setupShader();
Sprite initializeSprite(GLuint texID, vec3 dimensions, vec3 position,
                        int nAnimations = 1, int nFrames = 1, float vel = 1.5f, float angle = 0.0f);
GLuint loadTexture(string filePath, int &width, int &height);

// Funções de desenho e atualização
void drawSprite(GLuint shaderID, Sprite &sprite);
void updateSprite(GLuint shaderID, Sprite &sprite);
void moveSprite(Sprite &sprite);

// Funções relacionadas a itens
void updateItems(Sprite &sprite);
void spawnItem(Sprite &sprite);
void calculateAABB(Sprite &sprite);
bool checkCollision(Sprite one, Sprite two);

// Funções de interface (HUD)
void printHUD(int score, float hp, float shield); // [NOVO: SHIELD] adicionamos "shield" no print

// Dimensões da janela
const GLuint WIDTH = 800, HEIGHT = 600;

// Variáveis globais
float FPS = 12.0f;
float lastTime = 0;
bool keys[1024];
GLuint itemsTexIDs[4];
float velItems = 1.5f;
float lastSpawnX = 400.0;

float hp = 100.0f;
float shield = 0.0f;

int main() {
    srand((unsigned)time(0)); // Semente para números aleatórios

    // Inicialização da GLFW
    if(!glfwInit()) {
        cerr << "Falha ao inicializar a GLFW." << endl;
        return -1;
    }

    // Criação da janela GLFW
    GLFWwindow *window = glfwCreateWindow(WIDTH, HEIGHT, "SHOGUNATO", nullptr, nullptr);
    if(!window) {
        cerr << "Falha ao criar a janela GLFW." << endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);

    // Registra a função de callback de teclado
    glfwSetKeyCallback(window, key_callback);

    // GLAD: carrega os ponteiros de função da OpenGL
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        cout << "Falha ao inicializar GLAD" << endl;
        return -1;
    }

    // Obtendo informações da GPU e versão OpenGL
    cout << "Renderer: " << glGetString(GL_RENDERER) << endl;
    cout << "OpenGL version: " << glGetString(GL_VERSION) << endl;

    for (int i = 0; i < 1024; i++)
        keys[i] = false;

    int screenWidth, screenHeight;
    glfwGetFramebufferSize(window, &screenWidth, &screenHeight);
    glViewport(0, 0, screenWidth, screenHeight);

    GLuint shaderID = setupShader();

    Sprite background, character;
    vector<Sprite> items;
    int score = 0;
    bool gameover = false;
    int imgWidth, imgHeight, texID;

    // Carrega textura do personagem
    texID = loadTexture("../Textures/Characters/Female 23-1.png", imgWidth, imgHeight);
    character = initializeSprite(texID, vec3(imgWidth * 3, imgHeight * 3, 1.0), vec3(400, 100, 0), 4, 3);

    // Carrega textura do background
    texID = loadTexture("../Textures/Backgrounds/shogunhouse.png", imgWidth, imgHeight);
    background = initializeSprite(texID, vec3(imgWidth * 0.4, imgHeight * 0.4, 1.0), vec3(400, 300, 0));

    // Carrega texturas dos itens
    itemsTexIDs[0] = loadTexture("../Textures/Items/Icon30.png", imgWidth, imgHeight);
    itemsTexIDs[1] = loadTexture("../Textures/Items/Icon26.png", imgWidth, imgHeight);
    itemsTexIDs[2] = loadTexture("../Textures/Items/Icon42.png", imgWidth, imgHeight);
    itemsTexIDs[3] = loadTexture("../Textures/Items/shield.png", imgWidth, imgHeight);

    for (int i = 0; i < 4; i++) {
        Sprite newItem = initializeSprite(0, vec3(imgWidth * 1.5, imgHeight * 1.5, 1.0), vec3(0, 0, 0));
        spawnItem(newItem);
        items.push_back(newItem);
    }

    glUseProgram(shaderID);
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(glGetUniformLocation(shaderID, "texBuff"), 0);

    mat4 projection = ortho(0.0, 800.0, 0.0, 600.0, -1.0, 1.0);
    glUniformMatrix4fv(glGetUniformLocation(shaderID, "projection"), 1, GL_FALSE, value_ptr(projection));

    mat4 model = mat4(1);
    glUniformMatrix4fv(glGetUniformLocation(shaderID, "model"), 1, GL_FALSE, value_ptr(model));

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_ALWAYS);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    character.iAnimation = IDLE;
    printHUD(score, hp, shield); // [NOVO: SHIELD]

    while (!glfwWindowShouldClose(window) && !gameover) {
        glfwPollEvents();
        glClearColor(193/255.0f, 229/255.0f, 245/255.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUniform2f(glGetUniformLocation(shaderID, "offsetTex"), 0.0, 0.0);

        // Calcula AABB do personagem
        calculateAABB(character);

        // Checa colisões dos itens com o personagem
        for (int i = 0; i < (int)items.size(); i++) {
            calculateAABB(items[i]);
            if (checkCollision(character, items[i])) {
                // Verifica qual item foi coletado
                if (items[i].texID == itemsTexIDs[2]) {
                    // Item especial dá +5 pontos
                    score += 5;
                } else if (items[i].texID == itemsTexIDs[3]) {
                    // Item shield
                    shield = 100.0f;  
                } else {
                    // Itens comuns dão +1 ponto
                    score += 1;
                }

                velItems += 0.1f;
                printHUD(score, hp, shield);

                spawnItem(items[i]);
            }
        }

        drawSprite(shaderID, background);

        moveSprite(character);
        updateSprite(shaderID, character);
        drawSprite(shaderID, character);

        glUniform2f(glGetUniformLocation(shaderID, "offsetTex"), 0.0, 0.0);
        for (int i = 0; i < (int)items.size(); i++) {
            drawSprite(shaderID, items[i]);
            updateItems(items[i]);
        }

        if (hp <= 0.0f) {
            gameover = true;
            cout << "GAME OVER!" << endl;
        }

        glBindVertexArray(0);
        glfwSwapBuffers(window);

        printHUD(score, hp, shield);
    }

    if (!glfwWindowShouldClose(window))
        glfwSetWindowShouldClose(window, GL_TRUE);

    glfwTerminate();
    return 0;
}

// Função de callback do teclado
void key_callback(GLFWwindow *window, int key, int scancode, int action, int mode) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GL_TRUE);

    if (action == GLFW_PRESS)
        keys[key] = true;
    else if (action == GLFW_RELEASE)
        keys[key] = false;
}

int setupShader() {
    const GLchar *vertexShaderSource = R"(
        #version 400
        layout (location = 0) in vec3 position;
        layout (location = 1) in vec2 texc;
        uniform mat4 projection;
        uniform mat4 model;
        out vec2 texCoord;
        void main() {
            gl_Position = projection * model * vec4(position.x, position.y, position.z, 1.0);
            texCoord = vec2(texc.s, 1.0-texc.t);
        }
    )";

    const GLchar *fragmentShaderSource = R"(
        #version 400
        in vec2 texCoord;
        uniform sampler2D texBuff;
        uniform vec2 offsetTex;
        out vec4 color;
        void main() {
            color = texture(texBuff, texCoord + offsetTex);
        }
    )";

    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);

    GLint success;
    GLchar infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        cerr << "ERRO::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << endl;
    }

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);

    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        cerr << "ERRO::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << endl;
    }

    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        cerr << "ERRO::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return shaderProgram;
}

Sprite initializeSprite(GLuint texID, vec3 dimensions, vec3 position,
                        int nAnimations, int nFrames, float vel, float angle) {
    Sprite sprite;
    sprite.texID = texID;
    sprite.dimensions.x = dimensions.x / nFrames;
    sprite.dimensions.y = dimensions.y / nAnimations;
    sprite.pos = position;
    sprite.nAnimations = nAnimations;
    sprite.nFrames = nFrames;
    sprite.angle = angle;
    sprite.iFrame = 0;
    sprite.iAnimation = 0;
    sprite.vel = vel;
    sprite.ds = 1.0f / (float)nFrames;
    sprite.dt = 1.0f / (float)nAnimations;

    GLfloat vertices[] = {
        //   x      y     z    s           t
        -0.5f,  0.5f, 0.0f,   0.0f,       sprite.dt,
        -0.5f, -0.5f, 0.0f,   0.0f,       0.0f,
         0.5f,  0.5f, 0.0f,   sprite.ds,  sprite.dt,

        -0.5f, -0.5f, 0.0f,   0.0f,       0.0f,
         0.5f,  0.5f, 0.0f,   sprite.ds,  sprite.dt,
         0.5f, -0.5f, 0.0f,   sprite.ds,  0.0f
    };

    GLuint VBO, VAO;
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    // Atributos de posição
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (GLvoid *)0);
    glEnableVertexAttribArray(0);

    // Atributos de textura
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (GLvoid *)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    sprite.VAO = VAO;
    return sprite;
}

void drawSprite(GLuint shaderID, Sprite &sprite) {
    glBindVertexArray(sprite.VAO);
    glBindTexture(GL_TEXTURE_2D, sprite.texID);

    mat4 model = mat4(1.0f);
    model = translate(model, sprite.pos);
    model = rotate(model, radians(sprite.angle), vec3(0.0f, 0.0f, 1.0f));
    model = scale(model, sprite.dimensions);

    glUniformMatrix4fv(glGetUniformLocation(shaderID, "model"), 1, GL_FALSE, value_ptr(model));
    glDrawArrays(GL_TRIANGLES, 0, 6);

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void updateSprite(GLuint shaderID, Sprite &sprite) {
    float now = glfwGetTime();
    float dt = now - lastTime;
    if (dt >= 1.0f / FPS) {
        sprite.iFrame = (sprite.iFrame + 1) % sprite.nFrames;
        lastTime = now;
    }

    vec2 offsetTex;
    offsetTex.s = sprite.iFrame * sprite.ds;
    offsetTex.t = sprite.iAnimation * sprite.dt;
    glUniform2f(glGetUniformLocation(shaderID, "offsetTex"), offsetTex.s, offsetTex.t);
}

GLuint loadTexture(string filePath, int &width, int &height) {
    GLuint texID;
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    int nrChannels;
    unsigned char *data = stbi_load(filePath.c_str(), &width, &height, &nrChannels, 0);

    if (data) {
        if (nrChannels == 3) {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
        } else {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        }
        glGenerateMipmap(GL_TEXTURE_2D);
    } else {
        cerr << "Falha ao carregar a textura: " << filePath << endl;
    }

    stbi_image_free(data);
    glBindTexture(GL_TEXTURE_2D, 0);

    return texID;
}

void moveSprite(Sprite &sprite)
{
    float speedMultiplier = 2.0f; // fator de multiplicação da velocidade
    if (keys[GLFW_KEY_A] || keys[GLFW_KEY_LEFT])
    {
        sprite.pos.x -= sprite.vel * speedMultiplier;
        sprite.iAnimation = MOVING_LEFT;
    }

    if (keys[GLFW_KEY_D] || keys[GLFW_KEY_RIGHT])
    {
        sprite.pos.x += sprite.vel * speedMultiplier;
        sprite.iAnimation = MOVING_RIGHT;
    }

    if (!keys[GLFW_KEY_A] && !keys[GLFW_KEY_D] && !keys[GLFW_KEY_LEFT] && !keys[GLFW_KEY_RIGHT])
    {
        sprite.iAnimation = IDLE;
    }
}


void spawnItem(Sprite &sprite) {
    int max = (int)lastSpawnX + 250;
    if (max > 790) max = 790;
    int min = (int)lastSpawnX - 250;
    if (min < 10) min = 10;

    sprite.pos.x = (float)(rand() % (max - min + 1) + min);
    lastSpawnX = sprite.pos.x;
    sprite.pos.y = (float)(rand() % (3000 - 650 + 1) + 650);

    // [NOVO: SHIELD] Agora inclui o item shield no sorteio
    int randomItem = rand() % 4; 
    sprite.texID = itemsTexIDs[randomItem];

    sprite.vel = velItems;

    int n = rand() % 3;
    if (n == 1) {
        sprite.vel = sprite.vel + sprite.vel * 0.1f;
    } else if (n == 2) {
        sprite.vel = sprite.vel - sprite.vel * 0.1f;
    }
}

void updateItems(Sprite &sprite) {
    if (sprite.pos.y > 100) {
        sprite.pos.y -= sprite.vel;
    } else {
        // O item não foi pego, causa dano
        float damage = (float)(rand() % 16 + 5); // valor entre 5 e 20

        // [NOVO: SHIELD] Primeiro descontar do shield
        if (shield > 0.0f) {
            float absorbed = (damage <= shield) ? damage : shield;
            shield -= absorbed;
            damage -= absorbed; // dano restante vai para o HP, se houver
        }

        if (damage > 0.0f) {
            hp -= damage;
        }

        cout << "Item nao coletado! Dano causado total: " << (int)(damage + (damage == 0 ? 0 : 0)) 
             << "%. HP atual: " << hp << "%, Shield atual: " << shield << "%" << endl;

        spawnItem(sprite);
    }
}

void calculateAABB(Sprite &sprite) {
    sprite.PMin.x = sprite.pos.x - sprite.dimensions.x / 2.0f;
    sprite.PMin.y = sprite.pos.y - sprite.dimensions.y / 2.0f;

    sprite.PMax.x = sprite.pos.x + sprite.dimensions.x / 2.0f;
    sprite.PMax.y = sprite.pos.y + sprite.dimensions.y / 2.0f;
}

bool checkCollision(Sprite one, Sprite two) {
    bool collisionX = one.PMax.x >= two.PMin.x && two.PMax.x >= one.PMin.x;
    bool collisionY = one.PMax.y >= two.PMin.y && two.PMax.y >= one.PMin.y;
    return collisionX && collisionY;
}

void printHUD(int score, float hp, float shield) {
#if defined(_WIN32) || defined(_WIN64)
    system("cls");
#else
    system("clear");
#endif

    // Sequências ANSI para cores
    const char* RESET = "\033[0m";
    const char* RED = "\033[31m";
    const char* YELLOW = "\033[33m";
    const char* BLUE = "\033[34m";

    if (hp < 0.0f) hp = 0.0f;
    if (hp > 100.0f) hp = 100.0f;
    if (shield < 0.0f) shield = 0.0f;
    if (shield > 100.0f) shield = 100.0f;

    // Tamanho da "barra"
    int barLength = 20;
    int filledLengthHP = (int)((hp / 100.0f) * barLength);
    int filledLengthShield = (int)((shield / 100.0f) * barLength);

    // Construção da barra de HP com corações
    std::string healthBar;
    for (int i = 0; i < filledLengthHP; i++)
        healthBar += "♥";
    for (int i = filledLengthHP; i < barLength; i++)
        healthBar += ".";

    // Construção da barra do shield com escudos
    std::string shieldBar;
    for (int i = 0; i < filledLengthShield; i++)
        shieldBar += "🛡";
    for (int i = filledLengthShield; i < barLength; i++)
        shieldBar += ".";

    // Impressão com cores e símbolos
    cout << YELLOW << "Score: " << score << RESET << "\n";
    cout << RED << "HP:     [" << healthBar << "] " << (int)hp << "%" << RESET << "\n";
    cout << BLUE << "SHIELD: [" << shieldBar << "] " << (int)shield << "%" << RESET << "\n";
}
