#version 120

varying float lightIntensity;

void main() {
    // 원래 색상에 밝기를 곱해서 입체감 표현
    vec4 color = gl_Color;
    gl_FragColor = vec4(color.rgb * lightIntensity, color.a);
}