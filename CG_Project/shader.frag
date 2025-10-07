#version 330 core
out vec4 FragColor;
in vec3 Normal;
void main() {
    float intensity = dot(normalize(Normal), normalize(vec3(0.3, 0.5, 0.8)));
    FragColor = vec4(0.4 + 0.6 * intensity, 0.4 + 0.6 * intensity, 1.0, 1.0);
}
