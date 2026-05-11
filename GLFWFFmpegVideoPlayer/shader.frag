#version 330 core
out vec4 FragColor;
in vec2 TexCoord;
uniform sampler2D yTexture;
uniform sampler2D uvTexture;
uniform float uAlpha;
uniform int uRotated; // Ensure this is declared

void main() {
    vec2 finalCoords;
    
    if (uRotated == 1) {
        // 180-degree rotation: Flip X and use raw Y
        // (This cancels out the standard 1.0 - Y flip)
        finalCoords = vec2(1.0 - TexCoord.x, TexCoord.y);
    } else {
        // Standard orientation: Use your original vertical flip
        finalCoords = vec2(TexCoord.x, 1.0 - TexCoord.y);
    }

    float y = texture(yTexture, finalCoords).r;
    vec2 uv = texture(uvTexture, finalCoords).rg - vec2(0.5, 0.5);

    // BT.709 conversion
    float r = y + 1.5748 * uv.y;
    float g = y - 0.1873 * uv.x - 0.4681 * uv.y;
    float b = y + 1.8556 * uv.x;

    FragColor = vec4(r, g, b, uAlpha);
}