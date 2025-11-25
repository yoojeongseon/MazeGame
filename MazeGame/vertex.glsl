#version 120

void main() {
    // C코드의 gluLookAt, gluPerspective 설정을 그대로 적용하는 마법의 코드
    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
    
    // C코드의 glColor3f 색상을 그대로 전달
    gl_FrontColor = gl_Color;
}