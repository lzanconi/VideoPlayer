#version 330 core
out vec4 FragColor;
in vec2 TexCoord;
uniform sampler2D yTexture;
uniform sampler2D uvTexture;
uniform float uAlpha;

void main() {
    float y = texture(yTexture, vec2(TexCoord.x, 1.0 - TexCoord.y)).r;
    vec2 uv = texture(uvTexture, vec2(TexCoord.x, 1.0 - TexCoord.y)).rg - vec2(0.5, 0.5);
    
    // BT.709 conversion
    float r = y + 1.5748 * uv.y;
    float g = y - 0.1873 * uv.x - 0.4681 * uv.y;
    float b = y + 1.8556 * uv.x;
    
    FragColor = vec4(r, g, b, uAlpha);
}