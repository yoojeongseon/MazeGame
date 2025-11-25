#version 120

varying float lightIntensity;

void main() {
    // 1. C언어의 gluLookAt(카메라) 설정을 받아와서 위치 계산
    vec3 pos = vec3(gl_ModelViewMatrix * gl_Vertex);
    vec3 norm = normalize(gl_NormalMatrix * gl_Normal);
    
    // 2. 조명(손전등) 효과 계산
    vec3 lightPos = vec3(0.0, 0.0, 0.0); // 카메라 위치에서 빛이 나감
    vec3 lightDir = normalize(lightPos - pos);
    
    // 3. 빛의 밝기 결정 (정면일수록 밝게)
    float diff = max(dot(norm, lightDir), 0.0);
    lightIntensity = 0.2 + diff;
    
    // 4. 최종 위치 확정 (이게 있어야 3D로 보임!)
    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
    
    // 5. 색상 전달
    gl_FrontColor = gl_Color;
}