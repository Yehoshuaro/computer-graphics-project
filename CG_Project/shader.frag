#version 330 core
out vec4 FragColor;
in vec2 uv;

uniform float iTime;
uniform vec2 iResolution;

float hash21(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}
float noise(vec3 p) {
    vec3 ip = floor(p);
    vec3 u = fract(p);
    u = u*u*(3.0-2.0*u);
    float res = mix(mix(mix(hash21(ip.xy + vec2(0.0,0.0)), hash21(ip.xy + vec2(1.0,0.0)), u.x),
                       mix(hash21(ip.xy + vec2(0.0,1.0)), hash21(ip.xy + vec2(1.0,1.0)), u.x), u.y),
                    0.0, 0.0);
    return res * 0.5 + 0.5*hash21(ip.xy + vec2(ip.z));
}

float sdSphere(vec3 p, float r) { return length(p) - r; }

float sdRBC(vec3 p, float radius, float thickness) {
    float r = length(p.xy);
    float conc = 0.6 * (1.0 - pow(r / radius, 2.0));
    float localZ = p.z - conc * thickness;
    vec2 q = vec2(r - radius, localZ);
    return length(q) - (thickness * 0.45);
}

float sdLeuko(vec3 p, float r) {
    return length(p) - r;
}

float smin(float a, float b, float k) {
    float h = clamp(0.5 + 0.5*(b-a)/k, 0.0, 1.0);
    return mix(b, a, h) - k*h*(1.0 - h);
}

float sceneSDF(vec3 p, out vec3 outColor) {
    float wall = abs(p.x) - 3.5;
    float d = 1e5;
    outColor = vec3(0.25, 0.02, 0.02);
    float speed = 1.6;
    float pulseBase = 0.12 * sin(iTime * 2.2) + 0.9;
    const int RBC_COUNT = 40;
    float k = 0.25;
    for (int i = 0; i < RBC_COUNT; ++i) {
        float fi = float(i);
        float laneSeed = hash21(vec2(fi, 7.3));
        float angle = laneSeed * 6.283185 + fi * 0.14;
        float radius = mix(0.6, 1.8, fract(sin(fi*12.9898)*43758.5453));
        float offset = mod((iTime * speed * (0.9 + 0.2*hash21(vec2(fi,2.1)))) + fi*0.6, 12.0) - 6.0;
        vec3 center;
        center.x = radius * cos(angle + iTime*0.6);
        center.y = radius * sin(angle + iTime*0.6);
        center.z = -offset + 0.3 * sin(iTime*1.5 + fi);
        vec3 pLocal = p - center;
        float rRBC = 0.5 + 0.12 * sin(fi*3.3 + iTime*1.3);
        float tRBC = 0.45 + 0.05 * sin(fi*2.1 + iTime*0.8);
        float sd = sdRBC(pLocal, rRBC, tRBC);
        d = smin(d, sd, k);
        vec3 c = vec3(0.9, 0.12 + 0.05*hash21(vec2(fi,3.2)), 0.08) * (0.6 + 0.6*pulseBase);
        if (sd < 0.12) {
            outColor = mix(outColor, c, smoothstep(0.12, -0.02, sd));
        }
    }
    const int L_COUNT = 6;
    for (int j = 0; j < L_COUNT; ++j) {
        float fj = float(j);
        float seed = hash21(vec2(fj,9.1));
        float ang = seed*6.2831 + iTime*0.25;
        float rad = 0.4 + 1.8*seed;
        vec3 center = vec3( rad * cos(ang), rad * sin(ang), -mod(iTime*0.5 + fj*2.3, 10.0) + 2.0);
        float sd = sdLeuko(p - center, 0.6 + 0.15*seed);
        d = smin(d, sd, 0.35);
        if (sd < 0.25) outColor = mix(outColor, vec3(0.95,0.95,0.9), 0.6);
    }
    float vessel = wall;
    d = smin(d, vessel, 0.6);
    return d;
}

vec3 calcNormal(vec3 p) {
    float eps = 0.0015;
    vec3 c;
    float dx = sceneSDF(p + vec3(eps,0,0), c) - sceneSDF(p - vec3(eps,0,0), c);
    float dy = sceneSDF(p + vec3(0,eps,0), c) - sceneSDF(p - vec3(0,eps,0), c);
    float dz = sceneSDF(p + vec3(0,0,eps), c) - sceneSDF(p - vec3(0,0,eps), c);
    return normalize(vec3(dx,dy,dz));
}

void main() {
    vec2 frag = uv * iResolution;
    vec2 p = (frag - 0.5 * iResolution) / iResolution.y;
    vec3 ro = vec3(0.0, 0.0, 1.2 * sin(iTime*0.4)*0.05 + 0.6);
    float focal = 1.0;
    vec3 rd = normalize(vec3(p.x, p.y, -focal));
    float t = 0.0;
    float maxDist = 24.0;
    vec3 col = vec3(0.02, 0.0, 0.0);
    bool hit = false;
    vec3 hitPos;
    vec3 tmpCol = col;
    for (int i = 0; i < 120; ++i) {
        vec3 pos = ro + rd * t;
        float dist = sceneSDF(pos, tmpCol);
        if (dist < 0.0015) { hit = true; hitPos = pos; col = tmpCol; break; }
        if (t > maxDist) break;
        t += clamp(dist * 0.85, 0.01, 0.6);
    }

    float heartbeat = sin(iTime * 2.2);
    float beatEnv = pow(max(heartbeat, 0.0), 6.0);
    float beatBase = 0.5 + 1.2 * beatEnv;
    float slowPulse = 0.85 + 0.15 * sin(iTime * 0.8);
    float pulseFactor = beatBase * slowPulse;

    vec3 finalColor = vec3(0.0);
    if (hit) {
        vec3 n = calcNormal(hitPos);
        vec3 lightPos = vec3(2.5, 1.8, 4.0 + 0.6*sin(iTime*1.3));
        vec3 L = normalize(lightPos - hitPos);
        vec3 V = normalize(-rd);
        float diff = max(dot(n, L), 0.0);
        float spec = pow(max(dot(reflect(-L, n), V), 0.0), 32.0);
        float rim = pow(1.0 - max(dot(n, V), 0.0), 3.0);
        vec3 ambient = 0.12 * col * (0.9 + 0.3 * beatEnv);
        vec3 diffuse = 0.9 * diff * col * (0.85 + 0.3 * beatEnv);
        vec3 specular = 0.45 * spec * vec3(1.0) * (0.8 + 0.6 * beatEnv);
        vec3 rimLight = 0.6 * rim * vec3(1.0,0.2,0.15) * 0.5 * (1.0 + 0.9 * beatEnv);
        finalColor = ambient + diffuse + specular + rimLight;
        float back = max(dot(-L, n), 0.0);
        finalColor += 0.25 * back * vec3(0.9,0.15,0.08) * 0.6 * (1.0 + 0.8 * beatEnv);
    } else {
        float glow = exp(-length(p) * 2.6);
        float ring = exp(-pow((length(p) - 0.0 - 0.12*beatEnv*5.0),2.0)*16.0);
        finalColor = vec3(0.01, 0.0, 0.0) + (glow * vec3(0.25, 0.02, 0.02) * pulseFactor) + (ring * vec3(0.5,0.08,0.06) * beatEnv * 1.5);
    }

    float g = (hash21(uv * iResolution.xy * 0.5 + iTime*0.1) - 0.5) * 0.02;
    finalColor += g;
    finalColor = clamp(finalColor, 0.0, 5.0);
    finalColor = pow(finalColor, vec3(0.4545));
    float vv = smoothstep(0.9, 0.3, length((uv-0.5)*vec2(iResolution.x/iResolution.y,1.0)));
    finalColor *= mix(1.0, 0.72, vv*0.8);
    FragColor = vec4(finalColor, 1.0);
}
