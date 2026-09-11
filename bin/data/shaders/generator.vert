#version 150

// Vertex shader dùng chung cho MỌI generator. Nó không làm gì ngoài đưa
// hình chữ nhật toàn khung ra đúng chỗ — toàn bộ hình ảnh sinh ra ở
// fragment shader.

uniform mat4 modelViewProjectionMatrix;

in vec4 position;
in vec2 texcoord;

out vec2 vTexCoord;

void main() {
    vTexCoord   = texcoord;
    gl_Position = modelViewProjectionMatrix * position;
}
