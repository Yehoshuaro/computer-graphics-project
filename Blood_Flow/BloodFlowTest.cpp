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

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1,311.7))) * 43758.5453); }

float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f*f*(3.0 - 2.0*f);
    return mix(mix(hash(i + vec2(0,0)), hash(i + vec2(1,0)), u.x),
               mix(hash(i + vec2(0,1)), hash(i + vec2(1,1)), u.x), u.y);
}

float fbm(vec2 p) {
    float v = 0.0;
    float a = 0.5;
    for (int i = 0; i < 5; i++) {
        v += a * noise(p);
        p *= 2.0;
        a *= 0.5;
    }
    return v;
}

float sdSphere(vec3 p, vec3 c, float r) { return length(p - c) - r; }

float smoothUnion(float d1, float d2, float k) {
    float h = clamp(0.5 + 0.5*(d2 - d1)/k, 0.0, 1.0);
    return mix(d2, d1, h) - k*h*(1.0 - h);
}

float sceneSDF(vec3 p, out vec3 col) {
    vec3 c[5];
    float r[5];
    for (int i = 0; i < 5; i++) {
        float t = iTime * (0.7 + 0.1*i);
        c[i] = vec3(
            sin(t + i)*1.2,
            cos(t*0.9 + i*1.3)*1.2,
            sin(t*0.8 + i*2.0)*0.8
        );
        r[i] = 0.55 + 0.15*sin(iTime*1.2 + i);
    }
    float d = 999.0;
    float k = 0.6;
    for (int i = 0; i < 5; i++) {
        d = smoothUnion(d, sdSphere(p, c[i], r[i]), k);
    }
    col = vec3(0.5, 0.05, 0.08);
    return d;
}

vec3 calcNormal(vec3 p) {
    float e = 0.001;
    vec3 col;
    vec3 n;
    n.x = sceneSDF(p + vec3(e,0,0), col) - sceneSDF(p - vec3(e,0,0), col);
    n.y = sceneSDF(p + vec3(0,e,0), col) - sceneSDF(p - vec3(0,e,0), col);
    n.z = sceneSDF(p + vec3(0,0,e), col) - sceneSDF(p - vec3(0,0,e), col);
    return normalize(n);
}

vec3 bloodstream(vec2 uv, float t) {
    vec2 p = uv * 2.0;
    p.x += t * 0.3;
    float n = fbm(p * 2.5 + vec2(0.0, t * 0.3));
    float pulse = 0.65 + 0.35*sin(t*3.5);
    vec3 dark = vec3(0.05, 0.0, 0.01);
    vec3 mid = vec3(0.2, 0.02, 0.04);
    vec3 light = vec3(0.65, 0.1, 0.12);
    vec3 base = mix(dark, mid, pulse);
    vec3 glow = mix(base, light, smoothstep(0.5, 0.7, n));
    return mix(base, glow, 0.4);
}

vec3 cartoonShade(vec3 n, vec3 l, vec3 baseColor) {
    float diff = dot(n, l);
    float s = smoothstep(0.0, 0.6, diff);
    s = floor(s * 4.0) / 4.0;
    vec3 c = baseColor * (0.4 + 0.6*s);
    return c;
}

vec3 bloodCells(vec2 uv, float t) {
    vec3 result = vec3(0.0);
    for (int i = 0; i < 12; i++) {
        vec2 cellPos = vec2(
            fract(sin(i*21.7 + t*0.25)*43758.5),
            fract(cos(i*17.3 + t*0.28)*27345.2)
        );
        cellPos = cellPos * 2.0 - 1.0;
        cellPos.x -= mod(t*0.25 + i*0.1, 2.0) - 1.0;
        float d = length(uv - cellPos);
        float intensity = smoothstep(0.15, 0.0, d);
        vec3 col = mix(vec3(0.9,0.15,0.18), vec3(0.4,0.05,0.08), fbm(uv*3.0 + t));
        if (i % 3 == 0) col = vec3(0.9, 0.9, 0.95); // лейкоцит
        if (i % 5 == 0) col = vec3(0.8, 0.4, 0.3); // тромбоцит
        result += intensity * col * 0.5;
    }
    return result;
}

void main() {
    vec2 fragCoord = uv * iResolution;
    vec2 p = (fragCoord - 0.5 * iResolution) / iResolution.y;
    vec3 ro = vec3(0.0, 0.0, 4.0);
    vec3 rd = normalize(vec3(p.x, p.y, -1.2));

    float t = 0.0;
    float maxD = 10.0;
    bool hit = false;
    vec3 pos;
    vec3 color;

    for (int i = 0; i < 120; i++) {
        pos = ro + rd * t;
        vec3 col;
        float d = sceneSDF(pos, col);
        if (d < 0.002) { hit = true; break; }
        if (t > maxD) break;
        t += max(0.01, d * 0.8);
    }

    if (hit) {
        vec3 n = calcNormal(pos);
        vec3 light = normalize(vec3(1.5, 1.2, 2.0));
        vec3 col;
        sceneSDF(pos, col);
        vec3 shaded = cartoonShade(n, light, col);
        float pulse = 0.5 + 0.5*sin(iTime * 4.0);
        shaded *= 0.7 + 0.3*pulse;
        color = shaded;
    } else {
        color = bloodstream(p, iTime) + bloodCells(p, iTime) * 0.9;
    }

    color = mix(color, vec3(0.12, 0.01, 0.02), 0.3);
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
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    GLFWwindow* window = glfwCreateWindow(mode->width, mode->height, "Bloodstream Simulation", monitor, NULL);
    glfwMakeContextCurrent(window);
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
