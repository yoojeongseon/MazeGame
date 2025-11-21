#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include <GL/glew.h>
#include <GL/freeglut.h>
#include <GL/glu.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ====== 1. 복잡한 미로 데이터 (1:벽, 0:길) ====== */
// 제안서: 2차원 배열로 미로 구조 생성 
#define MAP_SIZE 15
int maze_map[MAP_SIZE][MAP_SIZE] = {
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
    {1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1},
    {1, 0, 1, 0, 1, 0, 1, 1, 1, 1, 0, 1, 1, 0, 1},
    {1, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1},
    {1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1},
    {1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1},
    {1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 1, 1},
    {1, 0, 0, 0, 1, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1},
    {1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 0, 1},
    {1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 1, 0, 1},
    {1, 0, 1, 1, 1, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1},
    {1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1},
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1}, // 우측 하단이 출구 쪽
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}
};

/* ====== 구조체 및 전역 변수 ====== */
typedef struct { float x, y, z; } Vec3;
typedef struct { unsigned int a, b, c; } Tri;
typedef struct { Vec3* verts; size_t vcount; Tri* tris; size_t tcount; } Model;

static Model g_model;
static int g_win_w = 800, g_win_h = 800;
static GLuint g_programID = 0;

// 카메라 상태 (FPS 스타일)
static float cam_x = 1.5f; // 시작 위치 (미로 내부 좌표에 맞춤)
static float cam_y = 0.5f; // 눈 높이
static float cam_z = 1.5f;
static float cam_yaw = -90.0f;   // 좌우 회전 각도
static float cam_pitch = 0.0f;   // 상하 회전 각도

// 마우스 제어
static int is_mouse_captured = 1; // 마우스 커서 숨기기 여부
static int last_mouse_x = 400, last_mouse_y = 400;

// 키보드 상태
static int keyW = 0, keyA = 0, keyS = 0, keyD = 0;
static const float MOVE_SPEED = 2.0f;
static const float MOUSE_SENSITIVITY = 0.1f;

static int last_time_ms = 0;

/* ====== 유틸리티 및 OBJ 로더 ====== */
static void die(const char* msg) { fprintf(stderr, "%s\n", msg); exit(EXIT_FAILURE); }
static void* xrealloc(void* p, size_t sz) { void* t = realloc(p, sz); if (!t) die("Out of memory"); return t; }
static int parse_index_token(const char* tok) {
    char buf[64]; size_t k = 0;
    while (*tok && *tok != '/' && k + 1 < sizeof(buf)) buf[k++] = *tok++;
    buf[k] = 0; return atoi(buf);
}
char* read_file(const char* filename) {
    FILE* fp = fopen(filename, "rb");
    if (!fp) { fprintf(stderr, "File error: %s\n", filename); return NULL; }
    fseek(fp, 0, SEEK_END); long size = ftell(fp); fseek(fp, 0, SEEK_SET);
    char* buf = (char*)malloc(size + 1); fread(buf, 1, size, fp); buf[size] = 0; fclose(fp); return buf;
}
void check_compile_error(GLuint shader) {
    GLint success; glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) { char infoLog[512]; glGetShaderInfoLog(shader, 512, NULL, infoLog); fprintf(stderr, "Shader Error: %s\n", infoLog); }
}
GLuint load_shaders(const char* v_path, const char* f_path) {
    char* vs = read_file(v_path); char* fs = read_file(f_path);
    if (!vs || !fs) return 0;
    GLuint v = glCreateShader(GL_VERTEX_SHADER); glShaderSource(v, 1, (const char**)&vs, NULL); glCompileShader(v); check_compile_error(v);
    GLuint f = glCreateShader(GL_FRAGMENT_SHADER); glShaderSource(f, 1, (const char**)&fs, NULL); glCompileShader(f); check_compile_error(f);
    GLuint p = glCreateProgram(); glAttachShader(p, v); glAttachShader(p, f); glLinkProgram(p);
    free(vs); free(fs); glDeleteShader(v); glDeleteShader(f); return p;
}
static void load_obj(const char* path, Model* m) {
    FILE* fp = fopen(path, "r");
    if (!fp) die("Failed to open OBJ");
    size_t vcap = 256, tcap = 512, vcnt = 0, tcnt = 0;
    Vec3* verts = (Vec3*)calloc(vcap, sizeof(Vec3));
    Tri* tris = (Tri*)calloc(tcap, sizeof(Tri));
    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
        if (line[0] == 'v' && line[1] == ' ') {
            float x, y, z; sscanf(line + 1, "%f %f %f", &x, &y, &z);
            if (vcnt + 1 > vcap) { vcap *= 2; verts = (Vec3*)xrealloc(verts, vcap * sizeof(Vec3)); }
            verts[vcnt++] = (Vec3){ x, y, z };
        }
        else if (line[0] == 'f' && line[1] == ' ') {
            unsigned i0, i1, i2; int n = 0; char t[4][64];
            n = sscanf(line + 1, "%s %s %s %s", t[0], t[1], t[2], t[3]);
            if (n >= 3) {
                if (tcnt + 2 > tcap) { tcap *= 2; tris = (Tri*)xrealloc(tris, tcap * sizeof(Tri)); }
                tris[tcnt++] = (Tri){ (unsigned)parse_index_token(t[0]) - 1, (unsigned)parse_index_token(t[1]) - 1, (unsigned)parse_index_token(t[2]) - 1 };
                if (n == 4) tris[tcnt++] = (Tri){ (unsigned)parse_index_token(t[0]) - 1, (unsigned)parse_index_token(t[2]) - 1, (unsigned)parse_index_token(t[3]) - 1 };
            }
        }
    }
    fclose(fp); m->verts = verts; m->vcount = vcnt; m->tris = tris; m->tcount = tcnt;
}
static void normalize_model(Model* m) { /* 생략 없이 큐브 크기를 1x1x1로 맞춤 */
    // 큐브 모델을 (0,0,0) ~ (1,1,1) 크기로 정규화한다고 가정
    // 실제로는 로드된 모델에 따라 다르므로, 여기서는 간략히 스케일링만 적용
    // (기존 코드의 정규화 로직 유지)
    Vec3 minv = m->verts[0], maxv = m->verts[0];
    for (size_t i = 1; i < m->vcount; ++i) {
        if (m->verts[i].x < minv.x) minv.x = m->verts[i].x; if (m->verts[i].x > maxv.x) maxv.x = m->verts[i].x;
        if (m->verts[i].y < minv.y) minv.y = m->verts[i].y; if (m->verts[i].y > maxv.y) maxv.y = m->verts[i].y;
        if (m->verts[i].z < minv.z) minv.z = m->verts[i].z; if (m->verts[i].z > maxv.z) maxv.z = m->verts[i].z;
    }
    float maxd = maxv.x - minv.x;
    if (maxv.y - minv.y > maxd) maxd = maxv.y - minv.y;
    if (maxv.z - minv.z > maxd) maxd = maxv.z - minv.z;
    float scale = 1.0f / maxd;
    for (size_t i = 0; i < m->vcount; ++i) {
        m->verts[i].x = (m->verts[i].x - minv.x) * scale - 0.5f; // 중심을 0으로
        m->verts[i].y = (m->verts[i].y - minv.y) * scale - 0.5f;
        m->verts[i].z = (m->verts[i].z - minv.z) * scale - 0.5f;
    }
}
static void draw_model(const Model* m) {
    glBegin(GL_TRIANGLES);
    for (size_t i = 0; i < m->tcount; ++i) {
        glVertex3f(m->verts[m->tris[i].a].x, m->verts[m->tris[i].a].y, m->verts[m->tris[i].a].z);
        glVertex3f(m->verts[m->tris[i].b].x, m->verts[m->tris[i].b].y, m->verts[m->tris[i].b].z);
        glVertex3f(m->verts[m->tris[i].c].x, m->verts[m->tris[i].c].y, m->verts[m->tris[i].c].z);
    }
    glEnd();
}

/* ====== 충돌 처리 (Collision) ====== */
// 제안서: 플레이어가 벽을 통과하지 못하도록 구현 
int check_collision(float new_x, float new_z) {
    // 플레이어의 위치를 정수형 그리드 인덱스로 변환 (반올림)
    int grid_x = (int)(new_x + 0.5f);
    int grid_z = (int)(new_z + 0.5f);

    // 맵 범위를 벗어나는지 확인
    if (grid_x < 0 || grid_x >= MAP_SIZE || grid_z < 0 || grid_z >= MAP_SIZE) return 1; // 충돌

    // 벽(1)인지 확인
    if (maze_map[grid_z][grid_x] == 1) return 1; // 충돌

    return 0; // 이동 가능
}

/* ====== 디스플레이 (렌더링) ====== */
static void display(void) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    gluPerspective(45.0, (float)g_win_w / g_win_h, 0.1, 100.0);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();

    // 1. 카메라 방향 계산 (마우스 회전 적용) [cite: 7, 40]
    // Yaw(Y축 회전)와 Pitch(X축 회전)를 사용해 바라보는 방향 벡터 계산
    float dir_x = cos(cam_pitch * M_PI / 180.0f) * cos(cam_yaw * M_PI / 180.0f);
    float dir_y = sin(cam_pitch * M_PI / 180.0f);
    float dir_z = cos(cam_pitch * M_PI / 180.0f) * sin(cam_yaw * M_PI / 180.0f);

    // gluLookAt 설정 (현재 위치에서 계산된 방향을 바라봄)
    gluLookAt(cam_x, cam_y, cam_z,
        cam_x + dir_x, cam_y + dir_y, cam_z + dir_z,
        0.0, 1.0, 0.0);

    // 2. 미로 그리기 (반복문) [cite: 34]
    // 셰이더 적용
    if (g_programID != 0) glUseProgram(g_programID);

    for (int z = 0; z < MAP_SIZE; ++z) {
        for (int x = 0; x < MAP_SIZE; ++x) {
            if (maze_map[z][x] == 1) { // 벽이면 큐브 그리기
                glPushMatrix();
                glTranslatef((float)x, 0.0f, (float)z); // 그리드 좌표로 이동
                glColor3f(0.8f, 0.8f, 0.8f); // 벽 색상
                draw_model(&g_model);
                glPopMatrix();
            }
        }
    }

    // 바닥 그리기 (임시 평면)
    glBegin(GL_QUADS);
    glColor3f(0.3f, 0.3f, 0.3f); // 바닥 색상
    glVertex3f(-10.0f, -0.5f, -10.0f);
    glVertex3f(MAP_SIZE + 10.0f, -0.5f, -10.0f);
    glVertex3f(MAP_SIZE + 10.0f, -0.5f, MAP_SIZE + 10.0f);
    glVertex3f(-10.0f, -0.5f, MAP_SIZE + 10.0f);
    glEnd();

    glUseProgram(0);
    glutSwapBuffers();
}

/* ====== 마우스 입력 (시선 회전) ====== */
// 제안서: 마우스로 시점을 자유롭게 회전 
static void passive_motion(int x, int y) {
    if (!is_mouse_captured) return;

    int dx = x - g_win_w / 2;
    int dy = y - g_win_h / 2;

    // 마우스 중앙 강제 고정 (FPS 게임처럼)
    if (dx != 0 || dy != 0) {
        cam_yaw += dx * MOUSE_SENSITIVITY;
        cam_pitch -= dy * MOUSE_SENSITIVITY;

        // 상하 회전 제한 (고개 꺾임 방지)
        if (cam_pitch > 89.0f) cam_pitch = 89.0f;
        if (cam_pitch < -89.0f) cam_pitch = -89.0f;

        glutWarpPointer(g_win_w / 2, g_win_h / 2); // 커서를 다시 중앙으로
    }
}

/* ====== 키보드 입력 및 로직 업데이트 ====== */
static void keyboard(unsigned char key, int x, int y) {
    if (key == 27) exit(0); // ESC 종료 [cite: 11]
    if (key == 'w' || key == 'W') keyW = 1;
    if (key == 's' || key == 'S') keyS = 1;
    if (key == 'a' || key == 'A') keyA = 1;
    if (key == 'd' || key == 'D') keyD = 1;
    if (key == 'm') { // 'm'키로 마우스 커서 보이기/숨기기 토글
        is_mouse_captured = !is_mouse_captured;
        if (is_mouse_captured) glutSetCursor(GLUT_CURSOR_NONE);
        else glutSetCursor(GLUT_CURSOR_LEFT_ARROW);
    }
}
static void keyboard_up(unsigned char key, int x, int y) {
    if (key == 'w' || key == 'W') keyW = 0;
    if (key == 's' || key == 'S') keyS = 0;
    if (key == 'a' || key == 'A') keyA = 0;
    if (key == 'd' || key == 'D') keyD = 0;
}

static void idle_update(void) {
    int now = glutGet(GLUT_ELAPSED_TIME);
    if (last_time_ms == 0) last_time_ms = now;
    float dt = (now - last_time_ms) / 1000.0f;
    last_time_ms = now;

    // 이동 방향 계산 (카메라가 보는 방향 기준)
    // Yaw 값만 사용하여 이동 (하늘로 날지 않게)
    float rad = cam_yaw * M_PI / 180.0f;
    float front_x = cos(rad);
    float front_z = sin(rad);
    float right_x = -front_z; // 오른쪽 벡터
    float right_z = front_x;

    float dx = 0.0f, dz = 0.0f;
    float speed = MOVE_SPEED * dt;

    if (keyW) { dx += front_x * speed; dz += front_z * speed; }
    if (keyS) { dx -= front_x * speed; dz -= front_z * speed; }
    if (keyA) { dx -= right_x * speed; dz -= right_z * speed; }
    if (keyD) { dx += right_x * speed; dz += right_z * speed; }

    // 충돌 처리 후 이동 적용 [cite: 41]
    // X축 이동 시도
    if (!check_collision(cam_x + dx, cam_z)) {
        cam_x += dx;
    }
    // Z축 이동 시도
    if (!check_collision(cam_x, cam_z + dz)) {
        cam_z += dz;
    }

    glutPostRedisplay();
}

static void reshape(int w, int h) {
    g_win_w = w; g_win_h = h;
    glViewport(0, 0, w, h);
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
    glutInitWindowSize(800, 800);
    glutCreateWindow("3D Maze - FPS Mode");

    GLenum e = glewInit();
    if (e != GLEW_OK) die("glewInit failed");

    load_obj("cube.obj", &g_model);
    normalize_model(&g_model);

    // 셰이더 로드 (파일이 있어야 함)
    g_programID = load_shaders("vertex.glsl", "fragment.glsl");

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    // 마우스 커서 숨기기 (FPS 게임처럼)
    glutSetCursor(GLUT_CURSOR_NONE);
    glutWarpPointer(400, 400);

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutKeyboardUpFunc(keyboard_up);
    glutPassiveMotionFunc(passive_motion); // 마우스 움직임 감지 함수 등록
    glutIdleFunc(idle_update);

    glutMainLoop();
    return 0;
}