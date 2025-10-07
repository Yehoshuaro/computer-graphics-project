#define _USE_MATH_DEFINES
#include <cmath>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>

const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

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

float sdSphere(vec3 p, vec3 c, float r) {
    return length(p - c) - r;
}

float smoothUnion(float d1, float d2, float k) {
    float h = clamp(0.5 + 0.5*(d2 - d1)/k, 0.0, 1.0);
    return mix(d2, d1, h) - k*h*(1.0 - h);
}

vec3 estimateNormal(vec3 p, vec3 c[4], float r[4]) {
    float eps = 0.0008;
    float dx = 0.0;
    float dy = 0.0;
    float dz = 0.0;
    vec3 e = vec3(eps, 0.0, 0.0);
    vec3 cp[4];
    for (int i=0;i<4;i++) cp[i]=c[i];
    float d0 = 1000.0;
    for (int i=0;i<4;i++) d0 = smoothUnion(d0, sdSphere(p, cp[i], r[i]), 0.4);
    dx = smoothUnion(d0, d0, 0.4) - (
        smoothUnion(smoothUnion(sdSphere(p-e.x, cp[0], r[0]), sdSphere(p-e.x, cp[1], r[1]), 0.4),
                    smoothUnion(sdSphere(p-e.x, cp[2], r[2]), sdSphere(p-e.x, cp[3], r[3]), 0.4), 0.4));
    dy = smoothUnion(d0, d0, 0.4) - (
        smoothUnion(smoothUnion(sdSphere(p-e.y, cp[0], r[0]), sdSphere(p-e.y, cp[1], r[1]), 0.4),
                    smoothUnion(sdSphere(p-e.y, cp[2], r[2]), sdSphere(p-e.y, cp[3], r[3]), 0.4), 0.4));
    dz = smoothUnion(d0, d0, 0.4) - (
        smoothUnion(smoothUnion(sdSphere(p-e.z, cp[0], r[0]), sdSphere(p-e.z, cp[1], r[1]), 0.4),
                    smoothUnion(sdSphere(p-e.z, cp[2], r[2]), sdSphere(p-e.z, cp[3], r[3]), 0.4), 0.4));
    return normalize(vec3(dx, dy, dz));
}

float sceneSDF(vec3 p, out vec3 col) {
    vec3 centers[4];
    float radii[4];
    centers[0] = vec3(-0.6 + 0.6*sin(iTime*0.9), 0.0, 0.0);
    centers[1] = vec3(0.6 + 0.6*sin(iTime*1.1 + 1.2), 0.0, 0.0);
    centers[2] = vec3(0.0, 0.6*cos(iTime*0.7), 0.0);
    centers[3] = vec3(0.0, -0.6*cos(iTime*0.9 + 0.5), 0.0);
    radii[0] = 0.6;
    radii[1] = 0.6;
    radii[2] = 0.45;
    radii[3] = 0.45;
    float d = 1000.0;
    float k = 0.6;
    for (int i=0;i<4;i++) {
        d = smoothUnion(d, sdSphere(p, centers[i], radii[i]), k);
    }
    col = vec3(0.3 + 0.2*sin(iTime), 0.5 + 0.3*cos(iTime*0.7), 0.9);
    return d;
}

vec3 calcNormal(vec3 p) {
    float eps = 0.0008;
    vec3 col;
    float dx = sceneSDF(vec3(p.x + eps, p.y, p.z), col) - sceneSDF(vec3(p.x - eps, p.y, p.z), col);
    float dy = sceneSDF(vec3(p.x, p.y + eps, p.z), col) - sceneSDF(vec3(p.x, p.y - eps, p.z), col);
    float dz = sceneSDF(vec3(p.x, p.y, p.z + eps), col) - sceneSDF(vec3(p.x, p.y, p.z - eps), col);
    return normalize(vec3(dx, dy, dz));
}

void main() {
    vec2 fragCoord = uv * iResolution;
    vec2 p = (fragCoord - 0.5 * iResolution) / iResolution.y;
    vec3 ro = vec3(0.0, 0.0, 3.0);
    float fov = 45.0;
    vec3 rd = normalize(vec3(p.x * tan(radians(fov) * 0.5), p.y * tan(radians(fov) * 0.5), -1.0));
    rd = normalize(rd);
    float t = 0.0;
    float maxDist = 20.0;
    vec3 color = vec3(0.0);
    bool hit = false;
    vec3 hitPos;
    for (int i = 0; i < 120; ++i) {
        vec3 pos = ro + rd * t;
        vec3 coltmp;
        float dist = sceneSDF(pos, coltmp);
        if (dist < 0.001) {
            hit = true;
            hitPos = pos;
            break;
        }
        if (t > maxDist) break;
        t += max(0.02, dist * 0.6);
    }
    if (hit) {
        vec3 n = calcNormal(hitPos);
        vec3 lightPos = vec3(1.2, 1.0, 2.0);
        vec3 lightColor = vec3(1.0);
        vec3 viewPos = ro;
        vec3 ambient = 0.2 * lightColor;
        vec3 lightDir = normalize(lightPos - hitPos);
        float diff = max(dot(n, lightDir), 0.0);
        vec3 diffuse = diff * lightColor;
        vec3 viewDir = normalize(viewPos - hitPos);
        vec3 reflectDir = reflect(-lightDir, n);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
        vec3 specular = 0.5 * spec * lightColor;
        vec3 baseCol;
        sceneSDF(hitPos, baseCol);
        color = (ambient + diffuse + specular) * baseCol;
    } else {
        color = vec3(0.02, 0.03, 0.06);
    }
    FragColor = vec4(sqrt(color), 1.0);
}
)";

unsigned int compile(const char* vsrc, const char* fsrc) {
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
    if (!glfwInit()) return -1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Spheres Merging Prototype", NULL, NULL);
    if (!window) {
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        glfwTerminate();
        return -1;
    }
    float quad[] = {
        -1.0f, -1.0f,
         1.0f, -1.0f,
        -1.0f,  1.0f,
         1.0f,  1.0f
    };
    unsigned int VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    unsigned int shaderProgram = compile(vertexShaderSource, fragmentShaderSource);
    while (!glfwWindowShouldClose(window)) {
        float time = (float)glfwGetTime();
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        glViewport(0, 0, width, height);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(shaderProgram);
        glUniform1f(glGetUniformLocation(shaderProgram, "iTime"), time);
        glUniform2f(glGetUniformLocation(shaderProgram, "iResolution"), (float)width, (float)height);
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glfwSwapBuffers(window);
        glfwPollEvents();
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(window, true);
    }
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(shaderProgram);
    glfwTerminate();
    return 0;
}
