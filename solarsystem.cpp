#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

#define WIDTH 1100
#define HEIGHT 750
#define PI 3.1415926

// ---------- Camera ----------
glm::vec3 cameraPos   = glm::vec3(0.0f, 0.0f, 80.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 cameraUp    = glm::vec3(0.0f, 1.0f, 0.0f);

float yaw   = -90.0f;
float pitch = 0.0f;
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// ---------- Input Handling ----------
void processInput(int &running, float deltaTime) {
    const float baseSpeed = 2.5f;
    const Uint8* state = SDL_GetKeyboardState(NULL);
    float speed = baseSpeed;
    if (state[SDL_SCANCODE_LSHIFT]) speed = 10.0f; // sprint
    float cameraSpeed = speed * deltaTime;

    // Forward / Back / Left / Right
    if (state[SDL_SCANCODE_W]) cameraPos += cameraSpeed * cameraFront;
    if (state[SDL_SCANCODE_S]) cameraPos -= cameraSpeed * cameraFront;
    if (state[SDL_SCANCODE_A]) cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
    if (state[SDL_SCANCODE_D]) cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;

    // Discrete up/down movement
    if (state[SDL_SCANCODE_UP])   cameraPos.y += 1.0f; // move up
    if (state[SDL_SCANCODE_DOWN]) cameraPos.y -= 1.0f; // move down

    if (state[SDL_SCANCODE_ESCAPE]) running = 0;
}

/* ================= PLANET ================= */
typedef struct {
    float orbitR;       // distance from sun
    float size;
    float angle;        // orbit angle around sun
    float rotation;     // rotation around its own axis
    float orbitSpeed;   // orbit speed
    float rotationSpeed; // spin speed
    float r,g,b;
    GLuint texture;
} Planet;

Planet planets[] = {
    {18,  1.2, 0, 0, 2.0, 1.0, 0.8,0.8,0.7,0},   // Mercury
    {22, 1.5, 0, 0, 1.5, 0.9, 1.0,0.6,0.2,0},   // Venus
    {27, 1.65,0, 0, 1.2, 1.0, 1,0.3,0.1,0},     // Earth
    {32+7, 1.8, 0, 0, 1.0, 0.8, 0.9,0.6,0.3,0},   // Mars
    {44+7, 3.75,0, 0, 0.5, 0.5, 0.9,0.8,0.5,0},   // Jupiter (extra gap before this)
    {58+10, 3.0, 0, 0, 0.4, 0.3, 0.5,0.8,1,0},     // Saturn (room for rings)
    {72+14, 2.7, 0, 0, 0.3, 0.25,0.5,0.8,1,0},     // Uranus
    {100, 2.5, 0, 0, 0.25,0.2, 0.3,0.5,1,0}      // Neptune
};

int planetCount = sizeof(planets)/sizeof(planets[0]);
float moonAngle = 0.0f; 
//textures
GLuint sunTexture = 0;//sun
GLuint moonTexture = 0;//Moon
GLuint backgroundTex=0;//Background
GLuint earthDayTex = 0;//Day earth
GLuint earthNightTex = 0;//Night
GLuint saturnRingTex = 0;
GLuint uranusRingTex = 0;

/* ================= ASTEROIDS ================= */
struct Asteroid {
    float angle;
    float radius;
    float height;
    float size;
    float speed;
    float shade;
};

std::vector<Asteroid> innerBelt;
std::vector<Asteroid> outerBelt;

GLUquadric *quad;

/* ================= OPENGL HELPERS ================= */
void setupLighting() {
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);

    GLfloat ambient[]  = {0.2f,0.2f,0.2f,1.0f};
    GLfloat diffuse[]  = {1.4f,1.3f,1.1f,1.0f};
    GLfloat specular[] = {1.0f,1.0f,1.0f,1.0f};
    GLfloat pos[]      = {0.0f,0.0f,0.0f,1.0f};

    glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, specular);
    glLightfv(GL_LIGHT0, GL_POSITION, pos);

    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE);
    glMaterialfv(GL_FRONT, GL_SPECULAR, specular);
    glMaterialf(GL_FRONT, GL_SHININESS, 50.0f);
}

//==================Textures==================================
GLuint loadTexture(const char* filename) {
    SDL_Surface* img = IMG_Load(filename);
    if (!img) {
        SDL_Log("Failed to load texture %s: %s", filename, IMG_GetError());
        return 0;
    }

    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    GLint mode = (img->format->BytesPerPixel == 4) ? GL_RGBA : GL_RGB;

    glTexImage2D(GL_TEXTURE_2D, 0, mode,
                 img->w, img->h, 0,
                 mode, GL_UNSIGNED_BYTE, img->pixels);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    SDL_FreeSurface(img);
    return texture;
}
//==================DRAWING===================================
void drawSphere(float r, GLuint texture=0){
    if(texture){
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D,texture);
    } else glDisable(GL_TEXTURE_2D);
    gluSphere(quad,r,32,32);
    glDisable(GL_TEXTURE_2D);
}

void drawOrbit(float r){
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glLineWidth(0.2f);
    glBegin(GL_LINE_LOOP);
    int segments = 360; // more segments = smoother
    for(int i=0;i<=segments;i++){
        float a = i * 2 * PI / segments;
        glVertex3f(cos(a)*r,0,sin(a)*r);
    }
    glEnd();
    glDisable(GL_LINE_SMOOTH);
}

void initBelt(std::vector<Asteroid>& belt, float inner, float outer, int count) {
    for(int i=0; i<count; i++) {
        Asteroid a;
        a.angle  = ((float)rand()/RAND_MAX) * 360.0f;
        a.radius = inner + ((float)rand()/RAND_MAX) * (outer - inner);
        a.height = ((float)rand()/RAND_MAX - 0.5f) * 1.2f;   // belt thickness
        a.size   = 0.03f + ((float)rand()/RAND_MAX) * 0.08f;
        a.speed  = 0.01f + ((float)rand()/RAND_MAX) * 0.15f;
        a.shade  = 0.4f + ((float)rand()/RAND_MAX)*0.4f;
        belt.push_back(a);
    }
}

void drawBelt(std::vector<Asteroid>& belt) {
    for(auto& a : belt) {
        a.angle += a.speed;

        float rad = a.angle * PI / 180.0f;
        float x = cos(rad) * a.radius;
        float z = sin(rad) * a.radius;

        glPushMatrix();
        glTranslatef(x, a.height, z);

        float shade = 0.5f + ((float)rand()/RAND_MAX) * 0.4f;
        glColor4f(a.shade, a.shade, a.shade,0.85f);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);


        drawSphere(a.size, 0);
        glPopMatrix();
        glDisable(GL_BLEND);
    }
}

void drawRing(float innerR, float outerR, GLuint texture)
{
    glPushMatrix();

    glDisable(GL_LIGHTING);

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texture);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glBegin(GL_TRIANGLE_STRIP);
    for (int i = 0; i <= 360; i++)
    {
        float rad = i * PI / 180.0f;
        float cx = cos(rad);
        float sz = sin(rad);

        // inner
        glTexCoord2f(0, i / 360.0f);
        glVertex3f(cx * innerR, 0, sz * innerR);

        // outer
        glTexCoord2f(1, i / 360.0f);
        glVertex3f(cx * outerR, 0, sz * outerR);
    }
    glEnd();

    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);

    glPopMatrix();
}


void drawPlanet(Planet *p){
    glPushMatrix();

    // Orbit around Sun
    glRotatef(p->angle,0,1,0);
    glTranslatef(p->orbitR,0,0);

    // Spin on its own axis
    glRotatef(p->rotation,0,1,0);

    if(p->texture) glColor3f(1,1,1);
    else glColor3f(p->r,p->g,p->b);

    drawSphere(p->size, p->texture);

    glPopMatrix();
}


void drawEarthMoon(){
    glPushMatrix();

    // Earth orbit
    glRotatef(planets[2].angle,0,1,0);
    glTranslatef(planets[2].orbitR,0,0);
    glRotatef(planets[2].rotation,0,1,0);
    drawSphere(planets[2].size, earthDayTex);

    // Night lights
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(1,1,1,0.35f);
    drawSphere(planets[2].size * 1.001f, earthNightTex);
    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);

    // ----- Moon orbit -----
    glRotatef(moonAngle, 0, 1, 0);              // Moon orbit around Earth
    glTranslatef(planets[2].size + 2.0, 0, 0); // distance from Earth
    glRotatef(moonAngle*12, 0, 1, 0);          // Moon spin on own axis
    drawSphere(0.5, moonTexture);

    glPopMatrix();
}

void drawBackground(){
    // Disable depth test so background doesn't block planets
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, WIDTH, 0, HEIGHT);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, backgroundTex);
    glColor3f(1,1,1);

    glBegin(GL_QUADS);
        glTexCoord2f(0,0); glVertex2f(0,0);
        glTexCoord2f(1,0); glVertex2f(WIDTH,0);
        glTexCoord2f(1,1); glVertex2f(WIDTH,HEIGHT);
        glTexCoord2f(0,1); glVertex2f(0,HEIGHT);
    glEnd();

    glDisable(GL_TEXTURE_2D);

    // Restore matrices
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);

    glEnable(GL_LIGHTING);
    glEnable(GL_DEPTH_TEST);
}
/* ================= DRAW SCENE(Planets+belts+sun) ================= */
void drawScene(){
    // Sun
    glPushMatrix();
    glDisable(GL_LIGHTING);
    drawSphere(6, sunTexture);
    glEnable(GL_LIGHTING);
    glPopMatrix();

    // Draw orbits
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

    glPushAttrib(GL_ENABLE_BIT);
    glDisable(GL_LIGHTING);
    glColor4f(0.8f, 0.8f, 1.0f, 0.6f);

    for(int i = 0; i < planetCount; i++) {
        drawOrbit(planets[i].orbitR);
    }

    glPopAttrib();

    glDisable(GL_LINE_SMOOTH);
    glDisable(GL_BLEND);

    // Planets (except Earth)
    for(int i=0;i<planetCount;i++){
        if(i != 2) drawPlanet(&planets[i]);
    }

    // Earth + Moon
    drawEarthMoon();

    // Asteroid Belts
    glColor3f(0.6,0.6,0.6);
    drawBelt(innerBelt);

    glColor3f(0.7,0.7,0.9);
    drawBelt(outerBelt);
    //Saturn ring
   glPushMatrix();
   glRotatef(planets[5].angle, 0, 1, 0);
   glTranslatef(planets[5].orbitR, 0, 0);
   glRotatef(27, 1, 0, 0);  
   drawRing(4.0f, 7.0f, saturnRingTex);
   glPopMatrix();

    //Uranus ring
    glPushMatrix();
    glRotatef(planets[6].angle, 0, 1, 0);
    glTranslatef(planets[6].orbitR, 0, 0);
    glRotatef(98, 1, 0, 0);  
    drawRing(3.0f, 5.0f, uranusRingTex);
    glPopMatrix();
}

/* ================= UPDATE ================= */
void updatePlanets(float deltaTime)
{
    for(int i = 0; i < planetCount; i++)
    {
        // Orbit around Sun
        planets[i].angle += planets[i].orbitSpeed * 0.5;

        // Spin around its own axis
        planets[i].rotation += planets[i].rotationSpeed * deltaTime * 50;
        if(planets[i].rotation > 360.0f) planets[i].rotation -= 360.0f;
    }
    moonAngle += 20.0f * deltaTime; // 20 deg/sec orbit speed
    if (moonAngle > 360.0f) moonAngle -= 360.0f;
}

/* ================= MAIN ================= */
int main(){
    SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER);
    TTF_Init();
    SDL_Window *win=SDL_CreateWindow("Solar System",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,WIDTH,HEIGHT,
        SDL_WINDOW_OPENGL);
    SDL_GLContext ctx=SDL_GL_CreateContext(win);
    TTF_Font *font=TTF_OpenFont("C:/Windows/Fonts/Candara.ttf",32);

    glEnable(GL_DEPTH_TEST);
    setupLighting();
    quad=gluNewQuadric();
    gluQuadricTexture(quad, GL_TRUE);
    gluQuadricNormals(quad, GLU_SMOOTH);
    gluQuadricDrawStyle(quad, GLU_FILL);

    backgroundTex= loadTexture("Textures/space.bmp");
    earthDayTex   = loadTexture("Textures/earth.bmp");
    earthNightTex = loadTexture("Textures/earth_night.bmp");
    saturnRingTex = loadTexture("Textures/saturnring.png");
    uranusRingTex = loadTexture("Textures/uranusring.png");

    // Load planet textures 
    planets[0].texture = loadTexture("Textures/mercury.bmp");
    planets[1].texture = loadTexture("Textures/venus.bmp");
    planets[3].texture = loadTexture("Textures/mars.bmp");
    planets[4].texture = loadTexture("Textures/jupiter.bmp");
    planets[5].texture = loadTexture("Textures/saturn.bmp");
    planets[6].texture = loadTexture("Textures/uranus.bmp");
    planets[7].texture = loadTexture("Textures/neptune.bmp");

    //Load Sun and moon texture
    sunTexture = loadTexture("Textures/sun.bmp");     
    moonTexture = loadTexture("Textures/moon.bmp");

    //Asteroid belt
    float marsOrbit    = planets[3].orbitR;
    float jupiterOrbit = planets[4].orbitR;
    // Place belt exactly between Mars and Jupiter
    float innerBeltCenter = (marsOrbit + jupiterOrbit) * 0.5f;
    float neptuneOrbit = planets[7].orbitR;
    // Place it beyond Neptune
    float outerBeltCenter = neptuneOrbit + 10.0f;
    initBelt(innerBelt, innerBeltCenter-3, innerBeltCenter+3, 500);
    initBelt(outerBelt, outerBeltCenter, outerBeltCenter+10, 500);

    SDL_Event event;
    int running=1;
    float earthAngle=0;

    // Lock the mouse to window and hide cursor
    SDL_SetRelativeMouseMode(SDL_TRUE);

    while(running){
        // Frame timing
        float currentFrame = SDL_GetTicks() / 1000.0f;
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // Handle events (mouse look + quit)
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running =0;
            }
            else if (event.type == SDL_MOUSEMOTION) {
                float xoffset = event.motion.xrel;
                float yoffset = -event.motion.yrel;

                float sensitivity = 0.1f;
                xoffset *= sensitivity;
                yoffset *= sensitivity;

                yaw   += xoffset;
                pitch += yoffset;

                if (pitch > 89.0f) pitch = 89.0f;
                if (pitch < -89.0f) pitch = -89.0f;

                glm::vec3 front;
                front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
                front.y = sin(glm::radians(pitch));
                front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
                cameraFront = glm::normalize(front);
            }
        }
        //___________Keyboard Handler_____________
        processInput(running, deltaTime);
        //___________Palnet Rotation_______________
        updatePlanets(deltaTime);

        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

        glDepthMask(GL_FALSE);
        drawBackground();
        glDepthMask(GL_TRUE);

        // ---- Proper FPS Camera ----
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        gluPerspective(70.0, (float)WIDTH/HEIGHT, 0.1, 2000.0);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        gluLookAt(
        cameraPos.x, cameraPos.y, cameraPos.z,
        cameraPos.x + cameraFront.x,
        cameraPos.y + cameraFront.y,
        cameraPos.z + cameraFront.z,
        cameraUp.x, cameraUp.y, cameraUp.z
        );
        // ---- Update Sun light position every frame ----
        GLfloat lightPos[] = {0.0f, 0.0f, 0.0f, 1.0f};
        glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
 
        drawScene();

        SDL_GL_SwapWindow(win);
    }
    gluDeleteQuadric(quad);
    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(win);
    TTF_Quit();
    SDL_Quit();
    return 0;
}