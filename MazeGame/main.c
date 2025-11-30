#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <time.h>

#include <GL/glew.h>
#include <GL/freeglut.h>
#include <GL/glu.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ====== 1. 미로 데이터 ====== */
#define MAP_SIZE 20 
#define PLAYER_RADIUS 0.35f

int maze_map[MAP_SIZE][MAP_SIZE] = {
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
    {1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1},
    {1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 1, 0, 1, 0, 1, 0, 1},
    {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 1},
    {1, 0, 1, 0, 1, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1},
    {1, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 1, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 1},
    {1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1},
    {1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 1},
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1},
    {1, 0, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1},
    {1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1},
    {1, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1},
    {1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1},
    {1, 0, 1, 0, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 0, 1},
    {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1},
    {1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 1, 1, 0, 1, 1, 1, 1, 0, 1},
    {1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}
};

typedef struct { float x, y, z; } Vec3;
typedef struct { unsigned int a, b, c; } Tri;
typedef struct { Vec3* verts; size_t vcount; Tri* tris; size_t tcount; } Model;

static Model g_model;
static int g_win_w = 800, g_win_h = 800;
static GLuint g_programID = 0;

static float cam_x = 1.5f;
static float cam_y = 0.5f;
static float cam_z = 1.5f;
static float cam_yaw = -90.0f;
static float cam_pitch = 0.0f;

typedef struct {
    float x, z;
    float speed;
} Enemy;

/* 적 시작 위치 */
static Enemy g_enemy = { 1.5f, 3.5f, 1.8f };
static float g_enemy_wait_timer = 3.0f;

static float g_torch_flicker = 1.0f;

static int is_mouse_captured = 1;
static int keyW = 0, keyA = 0, keyS = 0, keyD = 0;
static const float MOVE_SPEED = 2.0f;
static const float MOUSE_SENSITIVITY = 0.1f;
static int last_time_ms = 0;

typedef struct { float x, z; int active; } PowerUp;
static PowerUp g_powerups[3] = {
    { 3.5f,  1.5f, 1 },
    { 15.5f, 1.5f, 1 },
    { 1.5f, 13.5f, 1 }
};

static int g_can_pass_wall = 0;
static float g_ghost_timer = 0.0f;

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

static void normalize_model(Model* m) {
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
        m->verts[i].x = (m->verts[i].x - minv.x) * scale - 0.5f;
        m->verts[i].y = (m->verts[i].y - minv.y) * scale - 0.5f;
        m->verts[i].z = (m->verts[i].z - minv.z) * scale - 0.5f;
    }
}

void compute_normal(Vec3 v1, Vec3 v2, Vec3 v3) {
    float ux = v2.x - v1.x; float uy = v2.y - v1.y; float uz = v2.z - v1.z;
    float vx = v3.x - v1.x; float vy = v3.y - v1.y; float vz = v3.z - v1.z;
    float nx = uy * vz - uz * vy;
    float ny = uz * vx - ux * vz;
    float nz = ux * vy - uy * vx;
    float len = sqrt(nx * nx + ny * ny + nz * nz);
    if (len > 0.0f) glNormal3f(nx / len, ny / len, nz / len);
}

static void draw_model(const Model* m) {
    glBegin(GL_TRIANGLES);
    for (size_t i = 0; i < m->tcount; ++i) {
        Vec3 v1 = m->verts[m->tris[i].a];
        Vec3 v2 = m->verts[m->tris[i].b];
        Vec3 v3 = m->verts[m->tris[i].c];
        compute_normal(v1, v2, v3);
        glVertex3f(v1.x, v1.y, v1.z);
        glVertex3f(v2.x, v2.y, v2.z);
        glVertex3f(v3.x, v3.y, v3.z);
    }
    glEnd();
}

static int classify_cell(int grid_x, int grid_z) {
    if (grid_x < 0 || grid_x >= MAP_SIZE || grid_z < 0 || grid_z >= MAP_SIZE) return 2;
    if (maze_map[grid_z][grid_x] == 1) return 1;
    return 0;
}

int check_collision_ex(float new_x, float new_z, float r, int is_ghost) {
    static const float dirs[9][2] = {
        { 0.0f,   0.0f }, { 1.0f,   0.0f }, {-1.0f,   0.0f },
        { 0.0f,   1.0f }, { 0.0f,  -1.0f }, { 0.7f,   0.7f },
        { 0.7f,  -0.7f }, {-0.7f,   0.7f }, {-0.7f,  -0.7f }
    };

    for (int i = 0; i < 9; ++i) {
        float px = new_x + dirs[i][0] * r;
        float pz = new_z + dirs[i][1] * r;
        int grid_x = (int)px;
        int grid_z = (int)pz;
        int cell = classify_cell(grid_x, grid_z);

        if (cell == 2) return 1;
        if (cell == 1 && !is_ghost) return 1;
    }
    return 0;
}

int check_enemy_collision(float new_x, float new_z) {
    int grid_x = (int)(new_x + 0.5f);
    int grid_z = (int)(new_z + 0.5f);
    if (classify_cell(grid_x, grid_z) == 1) return 1;
    return 0;
}

int check_player_collision(float new_x, float new_z) {
    return check_collision_ex(new_x, new_z, 0.35f, g_can_pass_wall);
}

static void draw_minimap(void) {
    int mini_w = g_win_w / 4;
    int mini_h = g_win_h / 4;
    int mini_x = g_win_w - mini_w - 10;
    int mini_y = g_win_h - mini_h - 10;

    glViewport(mini_x, mini_y, mini_w, mini_h);
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    glOrtho(0.0, (double)MAP_SIZE, 0.0, (double)MAP_SIZE, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();

    glDisable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE); glUseProgram(0);

    glBegin(GL_QUADS); glColor3f(0.05f, 0.05f, 0.05f);
    glVertex2f(0.0f, 0.0f); glVertex2f((float)MAP_SIZE, 0.0f);
    glVertex2f((float)MAP_SIZE, (float)MAP_SIZE); glVertex2f(0.0f, (float)MAP_SIZE);
    glEnd();

    for (int z = 0; z < MAP_SIZE; ++z) {
        for (int x = 0; x < MAP_SIZE; ++x) {
            if (maze_map[z][x] == 1) {
                glBegin(GL_QUADS); glColor3f(0.8f, 0.8f, 0.8f);
                glVertex2f((float)x, (float)z); glVertex2f((float)x + 1, (float)z);
                glVertex2f((float)x + 1, (float)z + 1); glVertex2f((float)x, (float)z + 1);
                glEnd();
            }
        }
    }

    for (int i = 0; i < 3; ++i) {
        if (g_powerups[i].active) {
            glBegin(GL_QUADS); glColor3f(1.0f, 1.0f, 0.0f);
            float cx = g_powerups[i].x, cz = g_powerups[i].z, s = 0.3f;
            glVertex2f(cx - s, cz - s); glVertex2f(cx + s, cz - s);
            glVertex2f(cx + s, cz + s); glVertex2f(cx - s, cz + s);
            glEnd();
        }
    }

    glBegin(GL_QUADS);
    if (g_enemy_wait_timer > 0.0f) glColor3f(0.5f, 0.5f, 0.5f);
    else glColor3f(1.0f, 0.0f, 0.0f);

    float ex = g_enemy.x, ez = g_enemy.z, es = 0.4f;
    glVertex2f(ex - es, ez - es); glVertex2f(ex + es, ez - es);
    glVertex2f(ex + es, ez + es); glVertex2f(ex - es, ez + es);
    glEnd();

    glBegin(GL_TRIANGLES); glColor3f(1.0f, 0.2f, 0.2f);
    float px = cam_x, pz = cam_z, r = 0.3f;
    glVertex2f(px, pz + r); glVertex2f(px - r * 0.6f, pz - r); glVertex2f(px + r * 0.6f, pz - r);
    glEnd();

    glLineWidth(2.0f); glBegin(GL_LINE_LOOP); glColor3f(1.0f, 1.0f, 1.0f);
    glVertex2f(0.0f, 0.0f); glVertex2f((float)MAP_SIZE, 0.0f);
    glVertex2f((float)MAP_SIZE, (float)MAP_SIZE); glVertex2f(0.0f, (float)MAP_SIZE);
    glEnd(); glLineWidth(1.0f);

    glEnable(GL_DEPTH_TEST); glEnable(GL_CULL_FACE);
}

static void display(void) {
    glViewport(0, 0, g_win_w, g_win_h);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    gluPerspective(45.0, (float)g_win_w / g_win_h, 0.1, 100.0);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();

    float dir_x = cos(cam_pitch * M_PI / 180.0f) * cos(cam_yaw * M_PI / 180.0f);
    float dir_y = sin(cam_pitch * M_PI / 180.0f);
    float dir_z = cos(cam_pitch * M_PI / 180.0f) * sin(cam_yaw * M_PI / 180.0f);

    gluLookAt(cam_x, cam_y, cam_z, cam_x + dir_x, cam_y + dir_y, cam_z + dir_z, 0.0, 1.0, 0.0);

    if (g_programID != 0) glUseProgram(g_programID);

    for (int z = 0; z < MAP_SIZE; ++z) {
        for (int x = 0; x < MAP_SIZE; ++x) {
            if (maze_map[z][x] == 1) {
                glPushMatrix();
                glTranslatef((float)x + 0.5f, 0.0f, (float)z + 0.5f);
                glColor3f(0.6f * g_torch_flicker, 0.6f * g_torch_flicker, 0.6f * g_torch_flicker);
                draw_model(&g_model);
                glPopMatrix();
            }
        }
    }

    for (int i = 0; i < 3; ++i) {
        if (g_powerups[i].active) {
            glPushMatrix();
            glTranslatef(g_powerups[i].x, 0.3f, g_powerups[i].z);
            glScalef(0.3f, 0.3f, 0.3f);
            glColor3f(1.0f * g_torch_flicker, 1.0f * g_torch_flicker, 0.2f);
            draw_model(&g_model);
            glPopMatrix();
        }
    }

    glPushMatrix();
    glTranslatef(g_enemy.x, 0.3f, g_enemy.z);
    glScalef(0.6f, 0.6f, 0.6f);
    if (g_enemy_wait_timer > 0.0f) glColor3f(0.5f, 0.5f, 0.5f);
    else glColor3f(1.0f, 0.0f, 0.0f);
    draw_model(&g_model);
    glPopMatrix();

    glBegin(GL_QUADS); glNormal3f(0.0f, 1.0f, 0.0f);
    for (int z = -5; z < MAP_SIZE + 5; ++z) {
        for (int x = -5; x < MAP_SIZE + 5; ++x) {
            float c1 = 0.2f * g_torch_flicker;
            float c2 = 0.15f * g_torch_flicker;
            if ((x + z) % 2 == 0) glColor3f(c1, c1, c1); else glColor3f(c2, c2, c2);
            glVertex3f(x - 0.5f, -0.5f, z - 0.5f); glVertex3f(x + 0.5f, -0.5f, z - 0.5f);
            glVertex3f(x + 0.5f, -0.5f, z + 0.5f); glVertex3f(x - 0.5f, -0.5f, z + 0.5f);
        }
    }
    glEnd();

    if (g_programID != 0) glUseProgram(0);
    draw_minimap();
    glutSwapBuffers();
}

static void passive_motion(int x, int y) {
    if (!is_mouse_captured) return;
    int dx = x - g_win_w / 2;
    int dy = y - g_win_h / 2;
    if (dx != 0 || dy != 0) {
        cam_yaw += dx * MOUSE_SENSITIVITY;
        cam_pitch -= dy * MOUSE_SENSITIVITY;
        if (cam_pitch > 89.0f) cam_pitch = 89.0f;
        if (cam_pitch < -89.0f) cam_pitch = -89.0f;
        glutWarpPointer(g_win_w / 2, g_win_h / 2);
    }
}

static void keyboard(unsigned char key, int x, int y) {
    if (key == 27) exit(0);
    if (key == 'w' || key == 'W') keyW = 1;
    if (key == 's' || key == 'S') keyS = 1;
    if (key == 'a' || key == 'A') keyA = 1;
    if (key == 'd' || key == 'D') keyD = 1;
    if (key == 'm') {
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

void update_enemy(float dt) {
    float dx = cam_x - g_enemy.x;
    float dz = cam_z - g_enemy.z;
    float dist = sqrt(dx * dx + dz * dz);

    if (dist < 0.6f) {
        // [수정] 무적 상태(벽통과 중)일 때는 게임오버 안 됨
        if (g_can_pass_wall == 0) {
            printf("CAUGHT BY ENEMY! GAME OVER.\n");
            exit(0);
        }
    }

    if (dist > 0.1f) {
        dx /= dist;
        dz /= dist;
        float move_dist = g_enemy.speed * dt;

        float next_x = g_enemy.x + dx * move_dist;
        float next_z = g_enemy.z + dz * move_dist;

        if (!check_enemy_collision(next_x, g_enemy.z)) g_enemy.x = next_x;
        if (!check_enemy_collision(g_enemy.x, next_z)) g_enemy.z = next_z;
    }
}

static void idle_update(void) {
    int now = glutGet(GLUT_ELAPSED_TIME);
    if (last_time_ms == 0) last_time_ms = now;
    float dt = (now - last_time_ms) / 1000.0f;
    last_time_ms = now;
    if (dt > 0.05f) dt = 0.05f;

    g_torch_flicker = 0.8f + ((rand() % 30) / 100.0f);

    if (g_ghost_timer > 0.0f) {
        g_ghost_timer -= dt;
        if (g_ghost_timer <= 0.0f) {
            g_ghost_timer = 0.0f;
            g_can_pass_wall = 0;
        }
    }

    if (g_enemy_wait_timer > 0.0f) {
        int prev = (int)g_enemy_wait_timer;
        g_enemy_wait_timer -= dt;
        int curr = (int)g_enemy_wait_timer;
        if (prev != curr && curr >= 0) printf("%d...\n", curr + 1);
    }
    else {
        update_enemy(dt);
    }

    float rad = cam_yaw * M_PI / 180.0f;
    float fx = cos(rad), fz = sin(rad);
    float rx = -fz, rz = fx;
    float dx = 0.0f, dz = 0.0f, speed = MOVE_SPEED * dt;

    if (keyW) { dx += fx * speed; dz += fz * speed; }
    if (keyS) { dx -= fx * speed; dz -= fz * speed; }
    if (keyA) { dx -= rx * speed; dz -= rz * speed; }
    if (keyD) { dx += rx * speed; dz += rz * speed; }

    if (!check_player_collision(cam_x + dx, cam_z)) cam_x += dx;
    if (!check_player_collision(cam_x, cam_z + dz)) cam_z += dz;

    for (int i = 0; i < 3; ++i) {
        if (!g_powerups[i].active) continue;
        float vx = cam_x - g_powerups[i].x, vz = cam_z - g_powerups[i].z;
        if (vx * vx + vz * vz < 0.16f) {
            g_powerups[i].active = 0;
            g_can_pass_wall = 1;
            g_ghost_timer = 5.0f;
        }
    }

    glutPostRedisplay();
}

static void reshape(int w, int h) {
    g_win_w = w; g_win_h = h;
    glViewport(0, 0, w, h);
}

static void init_gl(void) {
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    srand(time(NULL));
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
    glutInitWindowSize(800, 800);
    glutCreateWindow("3D Maze - Invincible Ghost");

    GLenum e = glewInit();
    if (e != GLEW_OK) die("glewInit failed");

    load_obj("cube.obj", &g_model);
    normalize_model(&g_model);
    g_programID = load_shaders("vertex.glsl", "fragment.glsl");
    init_gl();

    glutSetCursor(GLUT_CURSOR_NONE);
    glutWarpPointer(400, 400);

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutKeyboardUpFunc(keyboard_up);
    glutPassiveMotionFunc(passive_motion);
    glutIdleFunc(idle_update);

    glutMainLoop();
    return 0;
}