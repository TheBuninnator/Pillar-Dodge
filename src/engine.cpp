#include "engine.h"
#include <iostream>

enum state {start, play, over};
state screen;

const color skyBlue(77/255.0, 213/255.0, 240/255.0);
const color grassGreen(26/255.0, 176/255.0, 56/255.0);
const color darkGreen(27/255.0, 81/255.0, 45/255.0);
const color white(1, 1, 1);
const color brickRed(201/255.0, 20/255.0, 20/255.0);
const color darkBlue(1/255.0, 110/255.0, 214/255.0);
const color purple(119/255.0, 11/255.0, 224/255.0);
const color black(0, 0, 0);
const color magenta(1, 0, 1);
const color orange(1, 163/255.0, 22/255.0);
const color cyan (0, 1, 1);
const color gray (125/255.0, 128/255.0, 133/255.0);

// Button colors
const color originalFill = {1, 0, 0, 1};
const color hoverFill{0.75, 0, 0, 1};
const color pressFill{0.5, 0, 0, 1};

// The Score and difficulty
int score;
double moveSpeed;

Engine::Engine() : keys() {
    this->initWindow();
    this->initShaders();
    this->initShapes();
}

Engine::~Engine() {}

unsigned int Engine::initWindow(bool debug) {
    // glfw: initialize and configure
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_FALSE);
#endif
    glfwWindowHint(GLFW_RESIZABLE, false);

    window = glfwCreateWindow(width, height, "engine", nullptr, nullptr);
    glfwMakeContextCurrent(window);

    // glad: load all OpenGL function pointers
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        cout << "Failed to initialize GLAD" << endl;
        return -1;
    }

    // OpenGL configuration
    glViewport(0, 0, width, height);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glfwSwapInterval(1);

    return 0;
}

void Engine::initShaders() {
    // load shader manager
    shaderManager = make_unique<ShaderManager>();

    // Load shader into shader manager and retrieve it
    shapeShader = this->shaderManager->loadShader("../res/shaders/shape.vert", "../res/shaders/shape.frag",  nullptr, "shape");

    // Configure text shader and renderer
    textShader = shaderManager->loadShader("../res/shaders/text.vert", "../res/shaders/text.frag", nullptr, "text");
    fontRenderer = make_unique<FontRenderer>(shaderManager->getShader("text"), "../res/fonts/MxPlus_IBM_BIOS.ttf", 24);

    // Set uniforms
    textShader.setVector2f("vertex", vec4(100, 100, .5, .5));
    shapeShader.use();
    shapeShader.setMatrix4("projection", this->PROJECTION);
}

void Engine::initShapes() {
    // Initialize the user to be a 10x10 white block
    //       centered at (0, 0)
    user = make_unique<Rect>(shapeShader, vec2(0, 0), vec2(10, 10), white);

    // Init Buttons
    easyButton = make_unique<Rect>(shapeShader, vec2{width/3,height/2}, vec2{100, 50}, color{1, 0, 0, 1});
    mediumButton = make_unique<Rect>(shapeShader, vec2{width/2,height/2}, vec2{100, 50}, color{1, 0, 0, 1});
    hardButton = make_unique<Rect>(shapeShader, vec2{width/1.5,height/2}, vec2{100, 50}, color{1, 0, 0, 1});

    // Init grass
    grass = make_unique<Rect>(shapeShader, vec2(width/2, 50), vec2(width, height / 3), grassGreen);

    // Init mountains
    mountains.push_back(make_unique<Triangle>(shapeShader, vec2(width/4, 300), vec2(width, 400), darkGreen));
    mountains.push_back(make_unique<Triangle>(shapeShader, vec2(2*width/3, 300), vec2(width, 500), darkGreen));

    // Init Cloud
    clouds.push_back(Cloud(shapeShader, vec2(200, 500)));
    clouds.push_back(Cloud(shapeShader, vec2(400, 520)));
    clouds.push_back(Cloud(shapeShader, vec2(325, 480)));

    // Init buildings from closest to furthest
    // There will be two buildings, one coming from the ground up and another from the top down
    int totalBuildingWidth = 0;
    vec2 buildingSize;
    while (totalBuildingWidth < width + 50) {
        // Set the height of the bottom pillar to a random value between 40 and 540
        buildingSize.y = rand() % 500 + 40;
        // Building width set to 25
        buildingSize.x = 25;
        buildings1.push_back(make_unique<Rect>(shapeShader,
                                               vec2(totalBuildingWidth + (buildingSize.x / 2.0) + 800,
                                                    ((buildingSize.y / 2.0))),
                                               buildingSize, brickRed));
        // Calculate the old building size to determine where the upper pillar should go
        int oldBuildSize = buildingSize.y;
        buildingSize.y += 600;
        buildings2.push_back(make_unique<Rect>(shapeShader,
                                        vec2(totalBuildingWidth + (buildingSize.x / 2.0) + 800,
                                            (((buildingSize.y) / 2.0) + (oldBuildSize + 40))), // Leave a 40 pixel gap
                                            buildingSize, brickRed));
        totalBuildingWidth += buildingSize.x + 300;
    }


}

void Engine::processInput() {
    glfwPollEvents();

    // Set keys to true if pressed, false if released
    for (int key = 0; key < 1024; ++key) {
        if (glfwGetKey(window, key) == GLFW_PRESS)
            keys[key] = true;
        else if (glfwGetKey(window, key) == GLFW_RELEASE)
            keys[key] = false;
    }

    // Close window if escape key is pressed
    if (keys[GLFW_KEY_ESCAPE])
        glfwSetWindowShouldClose(window, true);

    // Mouse position saved to check for collisions
    glfwGetCursorPos(window, &MouseX, &MouseY);

    // If we're in the over screen and the user presses r, change screen to start
    // Hint: The index is GLFW_KEY_R
    if (keys[GLFW_KEY_R] && screen == over) {
        score = 0;
        buildings1.clear();
        buildings2.clear();
        this->initShapes();
        screen = start;
    }

    // Mouse position is inverted because the origin of the window is in the top left corner
    MouseY = height - MouseY; // Invert y-axis of mouse position
    // make the user move with the mouse
    user->setPosX(MouseX);
    user->setPosY(MouseY);

    // Start buttons
    bool buttonOverlapsMouse = easyButton->isOverlapping(*user);
    bool mousePressed = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;

    // When in play screen, if the user hovers or clicks on the button then change the spawnButton's color
    // Hint: look at the color objects declared at the top of this file
    if (mousePressed && buttonOverlapsMouse) {
        easyButton->setColor(pressFill);
    }
    else if (buttonOverlapsMouse) {
        //std::cout << "I'm overlapping!" << std::endl;
        easyButton->setColor(hoverFill);
    }
    else {
        easyButton->setColor(originalFill);
    }

    if (mousePressedLastFrame && !mousePressed && buttonOverlapsMouse && screen == start) {
        moveSpeed = 2.0;
        screen = play;
    }
    // Medium Button
    buttonOverlapsMouse = mediumButton->isOverlapping(*user);
    mousePressed = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;

    // When in play screen, if the user hovers or clicks on the button then change the spawnButton's color
    // Hint: look at the color objects declared at the top of this file
    if (mousePressed && buttonOverlapsMouse) {
        mediumButton->setColor(pressFill);
    }
    else if (buttonOverlapsMouse) {
        //std::cout << "I'm overlapping!" << std::endl;
        mediumButton->setColor(hoverFill);
    }
    else {
        mediumButton->setColor(originalFill);
    }

    if (mousePressedLastFrame && !mousePressed && buttonOverlapsMouse && screen == start) {
        moveSpeed = 5.0;
        screen = play;
    }
    // Hard Button
    buttonOverlapsMouse = hardButton->isOverlapping(*user);
    mousePressed = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;

    // When in play screen, if the user hovers or clicks on the button then change the spawnButton's color
    // Hint: look at the color objects declared at the top of this file
    if (mousePressed && buttonOverlapsMouse) {
        hardButton->setColor(pressFill);
    }
    else if (buttonOverlapsMouse) {
        //std::cout << "I'm overlapping!" << std::endl;
        hardButton->setColor(hoverFill);
    }
    else {
        hardButton->setColor(originalFill);
    }

    if (mousePressedLastFrame && !mousePressed && buttonOverlapsMouse && screen == start) {
        moveSpeed = 10.0;
        screen = play;
    }

    // Save mousePressed for next frame
    mousePressedLastFrame = mousePressed;

    // Make sure the user cannot exceed the screen
    if (user->getTop() >= height) {
        user->setPosY(height);
    }
    if (user->getBottom() <= 0) {
        user->setPosY(0);
    }
    if (user->getLeft() <= 0) {
        user->setPosX(0);
    }
    if (user->getRight() >= width) {
        user->setPosX(width);
    }
    // End the game if they crash a pillar
    if (screen == play) {
        for (const unique_ptr<Rect>& r : buildings1) {
            if (r->isOverlapping(*user)) {
                screen = over;
            } else {
                r->setColor(gray);
            }
        }

        // Update the colors of buildings2
        for (const unique_ptr<Rect>& r : buildings2) {
            if (r->isOverlapping(*user)) {
                screen = over;
            } else {
                r->setColor(gray);
            }
        }
    }

}

void Engine::update() {
    // Calculate delta time
    float currentFrame = glfwGetTime();
    deltaTime = currentFrame - lastFrame;
    lastFrame = currentFrame;

    if (screen == play) {
        // Update clouds
        for (Cloud& c : clouds) {
            c.moveXWithinBounds(-1, width);
        }


        // Update buildings
        for (int i = 0; i < buildings1.size(); ++i) {
            // Move all the red buildings to the left
            buildings1[i]->moveX(-moveSpeed);
            // If a building has moved off the screen
            if (buildings1[i]->getPosX() < -(buildings1[i]->getSize().x/2)) {
                score++;
                cout << "Score: " << score << endl;
                // Set it to the right of the screen so that it passes through again
                int buildingOnLeft = (buildings1[i] == buildings1[0]) ? buildings1.size()-1 : i - 1;
                buildings1[i]->setPosX(buildings1[buildingOnLeft]->getPosX() + buildings1[buildingOnLeft]->getSize().x/2 + buildings1[i]->getSize().x/2 + 300);
            }
        }

        // The top buildings move in sync
        for (int i = 0; i < buildings2.size(); ++i) {
            // Move all the buildings to the left
            buildings2[i]->moveX(-moveSpeed);
            // If a building has moved off the screen
            if (buildings2[i]->getPosX() < -(buildings2[i]->getSize().x/2)) {
                // Set it to the right of the screen so that it passes through again
                int buildingOnLeft = (buildings2[i] == buildings2[0]) ? buildings2.size()-1 : i - 1;
                buildings2[i]->setPosX(buildings2[buildingOnLeft]->getPosX() + buildings2[buildingOnLeft]->getSize().x/2 + buildings2[i]->getSize().x/2 + 300);
            }
        }
    }
}

void Engine::render() {
    glClearColor(skyBlue.red,skyBlue.green, skyBlue.blue, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Set shader to draw shapes
    shapeShader.use();

    // Render differently depending on screen
    switch (screen) {
        case start: {
            string title = "Welcome to Jake's Pillar Dodge";
            string instructions = "Select your difficulty";
            // (12 * message.length()) is the offset to center text.
            // 12 pixels is the width of each character scaled by 1.
            // NOTE: This line changes the shader being used to the font shader.
            //  If you want to draw shapes again after drawing text,
            //  you'll need to call shapeShader.use() again first.
            this->fontRenderer->renderText(title, width/2 - (12 * title.length()), height/1.3, projection, 1, vec3{1, 1, 1});
            this->fontRenderer->renderText(instructions, width/2 - (12 * instructions.length()), height/3, projection, 1, vec3{1, 1, 1});

            // Drawing the spawn button
            shapeShader.use();
            easyButton->setUniforms();
            easyButton->draw();
            mediumButton->setUniforms();
            mediumButton->draw();
            hardButton->setUniforms();
            hardButton->draw();

            // Drawing the Spawn button text
            fontRenderer->renderText("Easy", easyButton->getPos().x - 30, easyButton->getPos().y - 5, projection, 0.5, vec3{1, 1, 1});
            fontRenderer->renderText("Medium", mediumButton->getPos().x - 30, mediumButton->getPos().y - 5, projection, 0.5, vec3{1, 1, 1});
            fontRenderer->renderText("Hard", hardButton->getPos().x - 30, hardButton->getPos().y - 5, projection, 0.5, vec3{1, 1, 1});

            // Draw my little guy
            shapeShader.use();
            user->setUniforms();
            user->draw();
            break;
        }
        case play: {
            for (const unique_ptr<Triangle>& m : mountains) {
                m->setUniforms();
                m->draw();
            }

            for (Cloud& c : clouds) {
                c.setUniformsAndDraw();
            }

            grass->setUniforms();
            grass->draw();

            // Drawing the pillars
            for (const unique_ptr<Rect>& r : buildings2) {
                r->setUniforms();
                r->draw();
            }
            for (const unique_ptr<Rect>& r : buildings1) {
                r->setUniforms();
                r->draw();
            }

            user->setUniforms();
            user->draw();

            string scoreText = "Score: " + std::to_string(score);
            this->fontRenderer->renderText(scoreText, width/1.2 - (12 * scoreText.length()), height/1.1, projection, 1, vec3{1, 1, 1});

            break;
        }
        case over: {
            string message = "GAME OVER! :(";
            // Display the message on the screen
            fontRenderer->renderText(message, width/2 - (12 * message.length()), height/1.8, projection, 1, vec3{1, 1, 1});
            string scoreText = "Your score was " + std::to_string(score);
            this->fontRenderer->renderText(scoreText, width/2 - (12 * scoreText.length()), height/2.2, projection, 1, vec3{1, 1, 1});
            string reset = "Press R to reset";
            this->fontRenderer->renderText(reset, width/2 - (12 * reset.length()), height/2.8, projection, 1, vec3{1, 1, 1});

            break;
        }
    }


    glfwSwapBuffers(window);
}

bool Engine::shouldClose() {
    return glfwWindowShouldClose(window);
}