#define _USE_MATH_DEFINES
#include <cmath>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <chrono>

const char* vertexShaderSource = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
out vec2 uv;
void main() {
    uv = aPos * 0.5 + 0.5;
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)";

const char* fragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;
in vec2 uv;
uniform float iTime;
uniform vec2 iResolution;

// distance field sphere
float sdSphere(vec3 p, vec3 c, float r) {
    return length(p - c) - r;
}

// smooth union
float smoothUnion(float d1, float d2, float k) {
    float h = clamp(0.5 + 0.5*(d2 - d1)/k, 0.0, 1.0);
    return mix(d2, d1, h) - k*h*(1.0 - h);
}

float sceneSDF(vec3 p, out vec3 col) {
    const int count = 6;
    vec3 centers[count];
    float radii[count];
    
    for (int i = 0; i < count; i++) {
        float t = iTime * (0.6 + 0.2*i);
        centers[i] = vec3(
            1.5 * sin(t + i),
            1.5 * cos(t * 0.8 + i * 1.3),
            0.5 * sin(t * 0.7 + i * 2.0)
        );
        radii[i] = 0.5 + 0.15 * sin(iTime + i);
    }

    float d = 999.0;
    float k = 0.35; // меньше → более разделённые сферы
    for (int i = 0; i < count; i++) {
        d = smoothUnion(d, sdSphere(p, centers[i], radii[i]), k);
    }

    col = vec3(0.25 + 0.25*sin(iTime), 0.6 + 0.3*cos(iTime*0.5), 1.0);
    return d;
}

vec3 calcNormal(vec3 p) {
    float eps = 0.0005;
    vec3 col;
    float dx = sceneSDF(vec3(p.x + eps, p.y, p.z), col) - sceneSDF(vec3(p.x - eps, p.y, p.z), col);
    float dy = sceneSDF(vec3(p.x, p.y + eps, p.z), col) - sceneSDF(vec3(p.x, p.y - eps, p.z), col);
    float dz = sceneSDF(vec3(p.x, p.y, p.z + eps), col) - sceneSDF(vec3(p.x, p.y, p.z - eps), col);
    return normalize(vec3(dx, dy, dz));
}

void main() {
    vec2 fragCoord = uv * iResolution;
    vec2 p = (fragCoord - 0.5 * iResolution) / iResolution.y;
    vec3 ro = vec3(0.0, 0.0, 5.0); // камера чуть дальше
    vec3 rd = normalize(vec3(p.x, p.y, -1.0));
    
    float t = 0.0;
    float maxDist = 20.0;
    vec3 color = vec3(0.0);
    bool hit = false;
    vec3 hitPos;

    for (int i = 0; i < 180; ++i) {
        vec3 pos = ro + rd * t;
        vec3 coltmp;
        float dist = sceneSDF(pos, coltmp);
        if (dist < 0.001) { hit = true; hitPos = pos; break; }
        if (t > maxDist) break;
        t += max(0.01, dist * 0.7);
    }

    if (hit) {
        vec3 n = calcNormal(hitPos);
        vec3 lightPos = vec3(3.0, 2.0, 4.0);
        vec3 lightColor = vec3(1.0);
        vec3 viewPos = ro;
        vec3 ambient = 0.2 * lightColor;
        vec3 lightDir = normalize(lightPos - hitPos);
        float diff = max(dot(n, lightDir), 0.0);
        vec3 diffuse = diff * lightColor;
        vec3 viewDir = normalize(viewPos - hitPos);
        vec3 reflectDir = reflect(-lightDir, n);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), 64.0);
        vec3 specular = 0.4 * spec * lightColor;
        vec3 baseCol;
        sceneSDF(hitPos, baseCol);
        color = (ambient + diffuse + specular) * baseCol;
    } else {
        color = vec3(0.02, 0.03, 0.06);
    }

    FragColor = vec4(pow(color, vec3(0.4545)), 1.0);
}
)";

unsigned int compileShader(const char* vsrc, const char* fsrc) {
    unsigned int vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vsrc, NULL);
    glCompileShader(vs);
    unsigned int fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fsrc, NULL);
    glCompileShader(fs);
    unsigned int prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    const int SCR_WIDTH = 1600;
    const int SCR_HEIGHT = 900;
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Spheres Merging Visualization", NULL, NULL);
    glfwMakeContextCurrent(window);
    glfwMaximizeWindow(window);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) return -1;

    unsigned int shader = compileShader(vertexShaderSource, fragmentShaderSource);

    float quad[] = { -1, -1, 1, -1, -1, 1, 1, 1 };
    unsigned int VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    auto start = std::chrono::high_resolution_clock::now();

    while (!glfwWindowShouldClose(window)) {
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(shader);

        auto now = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float>(now - start).count();

        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        glUniform1f(glGetUniformLocation(shader, "iTime"), time);
        glUniform2f(glGetUniformLocation(shader, "iResolution"), (float)width, (float)height);

        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    glfwTerminate();
    return 0;
}
