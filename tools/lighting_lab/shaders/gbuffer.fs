#version 330
/* 원화의 알파로 잘라 세 화면 버퍼의 경계를 정확히 맞춥니다. */
in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;
uniform sampler2D texture0;
uniform sampler2D normalAtlas;
uniform sampler2D surfaceAtlas;
uniform int bufferPass;
uniform float normalFlip;
void main() {
    vec4 albedo = texture(texture0, fragTexCoord);
    if (albedo.a < 0.5) discard;
    if (bufferPass == 0) finalColor = albedo;
    else if (bufferPass == 1) {
        vec3 normal = texture(normalAtlas, fragTexCoord).rgb * 2.0 - 1.0;
        normal.x *= normalFlip;
        finalColor = vec4(normal * 0.5 + 0.5, 1.0);
    } else finalColor = texture(surfaceAtlas, fragTexCoord);
}
