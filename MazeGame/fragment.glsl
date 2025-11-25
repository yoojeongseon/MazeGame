#version 120

void main() {
    // 버텍스 쉐이더에서 받아온 색상(회색)을 그대로 출력
    gl_FragColor = gl_Color;
}