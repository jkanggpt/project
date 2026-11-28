#version 330
/* 모든 법선·수신점·광원·차폐 모양은 같은 월드 좌표입니다. */
in vec2 fragTexCoord;
out vec4 finalColor;
uniform sampler2D texture0;
uniform sampler2D normalBuffer;
uniform sampler2D surfaceBuffer;
uniform vec2 worldOrigin;
uniform vec2 worldSize;
uniform vec3 sunDirection;
uniform vec3 sunColor;
uniform vec3 ambientColor;
uniform vec4 lightPosition[4];
uniform vec4 lightColor[4];
uniform vec4 occluderCenter[64];
uniform vec4 occluderExtent[64];
uniform int occluderCount;
uniform int displayMode;
uniform float exposure;
uniform float torchEnabled;
uniform float interiorEnabled;
uniform float clockTime;

bool planeClip(vec3 normal, float bound, vec3 p, vec3 d, inout float first, inout float last) {
    float denominator = dot(normal, d);
    float distance = bound - dot(normal, p);
    if (abs(denominator) < 0.0000001) return distance >= 0.0;
    float t = distance / denominator;
    if (denominator < 0.0) first = max(first, t);
    else last = min(last, t);
    return first <= last;
}
bool blocked(vec3 p, vec3 d, float limit, vec4 a, vec4 b) {
    p -= a.xyz;
    float first = -100000.0;
    float last = limit;
    if (a.w > 0.5 && a.w < 1.5) {
        vec3 q = p / b.xyz;
        vec3 ray = d / b.xyz;
        float aa = dot(ray, ray);
        float bb = dot(q, ray);
        float cc = dot(q, q) - 1.0;
        float discriminant = bb * bb - aa * cc;
        if (discriminant < 0.0) return false;
        first = (-bb - sqrt(discriminant)) / aa;
        last = (-bb + sqrt(discriminant)) / aa;
    } else {
        if (!planeClip(vec3(1,0,0), b.x, p, d, first, last)) return false;
        if (!planeClip(vec3(-1,0,0), b.x, p, d, first, last)) return false;
        if (!planeClip(vec3(0,1,0), b.y, p, d, first, last)) return false;
        if (!planeClip(vec3(0,-1,0), b.y, p, d, first, last)) return false;
        if (a.w > 1.5) {
            float slope = b.z / b.x;
            if (!planeClip(vec3(0,0,-1), 1.0, p, d, first, last)) return false;
            if (!planeClip(vec3(slope,0,1), b.z, p, d, first, last)) return false;
            if (!planeClip(vec3(-slope,0,1), b.z, p, d, first, last)) return false;
        } else {
            if (!planeClip(vec3(0,0,-1), b.z, p, d, first, last)) return false;
            if (!planeClip(vec3(0,0,1), b.z, p, d, first, last)) return false;
        }
    }
    return last > max(first, 0.02) && first < limit && last > 0.02;
}
float visibility(vec3 p, vec3 normal, vec3 direction, float distance) {
    vec3 origin = p + normal * 0.48;
    for (int i = 0; i < 64; i++) {
        if (i >= occluderCount) break;
        if (blocked(origin, direction, distance - 0.2, occluderCenter[i], occluderExtent[i])) return 0.0;
    }
    return 1.0;
}
vec3 illuminate(vec3 p, vec3 n, vec3 color, vec3 direction, float visible, float roughness) {
    float diffuse = max(dot(n, direction), 0.0);
    vec3 view = normalize(vec3(0, 1, 1));
    vec3 halfway = normalize(direction + view);
    float specular = pow(max(dot(n, halfway), 0.0), mix(120.0, 12.0, roughness));
    specular *= mix(0.38, 0.025, roughness) * step(0.001, diffuse);
    return color * visible * (vec3(diffuse) + vec3(specular));
}
void main() {
    vec4 albedo = texture(texture0, fragTexCoord);
    if (albedo.a < 0.5) { finalColor = vec4(0.025,0.04,0.06,1); return; }
    vec3 n = normalize(texture(normalBuffer, fragTexCoord).rgb * 2.0 - 1.0);
    vec4 data = texture(surfaceBuffer, fragTexCoord);
    float height = data.r * 255.0 + data.g * (255.0 / 256.0);
    vec2 screen = worldOrigin + vec2(fragTexCoord.x, 1.0 - fragTexCoord.y) * worldSize;
    vec3 p = vec3(screen.x, screen.y + height, height);
    if (displayMode == 0) { finalColor = vec4(albedo.rgb,1); return; }
    if (displayMode == 3) { finalColor = vec4(n*.5+.5,1); return; }
    if (displayMode == 4) { finalColor = vec4(height/48.0, .15+height/64.0, .45-height/128.0, 1); return; }
    if (data.a < .15) n = normalize(n + vec3(.04*sin(p.x*.4+clockTime),.04*cos(p.y*.3+clockTime),0));
    vec3 base = pow(albedo.rgb, vec3(2.2));
    vec3 radiance = ambientColor * (.55 + .45*max(n.z,0.0));
    bool shadows = displayMode == 2 || displayMode == 5;
    vec3 sun = normalize(sunDirection);
    float sunVisible = shadows && dot(sunColor,vec3(1)) > .001 ? visibility(p,n,sun,512.0) : 1.0;
    radiance += illuminate(p,n,sunColor,sun,sunVisible,data.a);
    float occlusion = sunVisible;
    for (int i = 0; i < 4; i++) {
        vec3 delta = lightPosition[i].xyz - p;
        float distance = length(delta);
        float radius = lightPosition[i].w;
        if (radius <= 0.0 || distance >= radius || lightColor[i].a <= 0.0) continue;
        vec3 direction = delta / max(distance,.01);
        float attenuation = max(0.0,1.0-distance/radius);
        attenuation = attenuation * attenuation * lightColor[i].a;
        float visible = shadows ? visibility(p,n,direction,distance) : 1.0;
        radiance += illuminate(p,n,lightColor[i].rgb*attenuation,direction,visible,data.a);
        occlusion = min(occlusion, visible);
    }
    float emission = data.b > .8 ? torchEnabled*3.5 : interiorEnabled*2.4;
    vec3 result = base*radiance + base*data.b*emission;
    if (displayMode == 5) result = vec3(.10+.8*occlusion);
    else result = pow(vec3(1)-exp(-result*exposure),vec3(1.0/2.2));
    finalColor = vec4(clamp(result,0.0,1.0),1);
}
