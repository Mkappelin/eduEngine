#ifndef Game_hpp
#define Game_hpp
#pragma once

#include <entt/fwd.hpp>
#include "GameBase.h"
#include "RenderableMesh.hpp"
#include "ForwardRenderer.hpp"
#include "ShapeRenderer.hpp"

/// @brief A Game may hold, update and render 3D geometry and GUI elements
class Game : public eeng::GameBase
{
public:
    /// @brief For game resource initialization
    /// @return 
    bool init() override;

    /// @brief General update method that is called each frame
    /// @param time Total time elapsed in seconds
    /// @param deltaTime Time elapsed since the last frame
    /// @param input Input from mouse, keyboard and controllers
    void update(
        float time,
        float deltaTime,
        InputManagerPtr input) override;

    /// @brief For rendering of game contents
    /// @param time Total time elapsed in seconds
    /// @param screenWidth Current width of the window in pixels
    /// @param screenHeight Current height of the window in pixels
    void render(
        float time,
        int windowWidth,
        int windowHeight) override;

    /// @brief For destruction of game resources
    void destroy() override;

private:
    /// @brief For rendering of GUI elements
    void renderUI();

    // Renderer for rendering imported animated or non-animated models
    eeng::ForwardRendererPtr forwardRenderer;

    // Immediate-mode renderer for basic 2D or 3D primitives
    ShapeRendererPtr shapeRenderer;

    // Entity registry - to use in labs
    std::shared_ptr<entt::registry> entity_registry;

    // Matrices for view, projection and viewport
    struct Matrices
    {
        glm::mat4 V;
        glm::mat4 P;
        glm::mat4 VP;
        glm::ivec2 windowSize;
    } matrices;

    // Basic third-person camera
    struct Camera
    {
        glm::vec3 lookAt = glm_aux::vec3_000;   // Point of interest
        glm::vec3 up = glm_aux::vec3_010;       // Local up-vector
        float distance = 15.0f;                 // Distance to point-of-interest
        float sensitivity = 0.005f;             // Mouse sensitivity
        const float nearPlane = 1.0f;           // Rendering near plane
        const float farPlane = 500.0f;          // Rendering far plane

        // Position and view angles (computed when camera is updated)
        float yaw = 0.0f;                       // Horizontal angle (radians)
        float pitch = -glm::pi<float>() / 8;    // Vertical angle (radians)
        glm::vec3 pos;                          // Camera position

        // Previous mouse position
        glm::ivec2 mouse_xy_prev{ -1, -1 };
    } camera;

    // Light properties
    struct PointLight
    {
        glm::vec3 pos;
        glm::vec3 color{ 1.0f, 1.0f, 0.8f };
    } pointlight;

    // (Placeholder) Player data
    struct Player
    {
        glm::vec3 pos = glm_aux::vec3_000;
        float velocity{ 6.0f };

        // Local vectors & view ray (computed when camera/player is updated)
        glm::vec3 fwd, right;
        glm_aux::Ray viewRay;
    } player;

    // Game meshes
    std::shared_ptr<eeng::RenderableMesh> grassMesh, horseMesh, characterMesh, foxMesh, marcoMesh;

    // Game entity transformations
    glm::mat4 characterWorldMatrix1, characterWorldMatrix2, characterWorldMatrix3;
    glm::mat4 grassWorldMatrix, horseWorldMatrix;

    // Game entity AABBs (for collision detection or visualization)
    eeng::AABB character_aabb1, character_aabb2, character_aabb3, horse_aabb, grass_aabb;

    // Placeholder animation state
    int characterAnimIndex = -1;
    float characterAnimSpeed = 1.0f;

    // Stats
    int drawcallCount = 0;

    /// @brief Placeholder system for updating the camera position based on inputs
    /// @param input Input from mouse, keyboard and controllers
    void updateCamera(
        InputManagerPtr input);

    /// @brief Placeholder system for updating the 'player' based on inputs
    /// @param deltaTime 
    void updatePlayer(
        float deltaTime,
        InputManagerPtr input);



    struct Tfm
    {
        glm::vec3 position, rotation, scale;
    };

    struct Velocity 
    {
        glm::vec3 velocity;
    };
    struct MeshComponent 
    {
        std::shared_ptr<eeng::RenderableMesh> mesh;
    };
    struct NPCController 
    {
        std::vector<glm::vec3> waypoints;
		int currentWaypoint = 0;
		float speed = 2.0f;
        bool canTrade;
        bool canRepair;
        bool hostile;
    };
    struct PlayerController 
    {
        glm::vec3 direction = glm::vec3(0.0f);
    };

    struct AnimState {
        int currentState = 0;
        int previousState = 0;
        float blendTimer = 0.0f;
    };

    bool useBlendingFSM = true;
    float debugBlendFactor = 1.0f;
    bool useDebugBlend = false;
    bool showBoneGizmos = false;


    void MovingSystem(Tfm& t, Velocity& v, float dt);
    void PlayerControllerSystem(Game::PlayerController& pc, Game::Velocity& v, const InputManagerPtr& input, const Camera& camera);
    void NPCControllerSystem(NPCController& npcc, Tfm& tfm, Velocity& vel);
    void RenderSystem(eeng::ForwardRendererPtr& forwardRenderer, Tfm& tfm, MeshComponent& entityMesh);
    void FSM(MeshComponent& mesh, const Velocity& vel, float dt);
	void FSMWithBlend(MeshComponent& mesh, Velocity& v, AnimState& anim, float deltaTime, float time);

    //Refactoring things

	//Init
    void initEntities();
    void initNPCEntity();
	void initPlayerEntity();
    void initFoxEntity();

    void initMeshes();

    void initWorldTransforms();
	void initRenderers();

    //Update
    void updateMovement(float deltaTime);
    void updateCameraTarget();
    void updateNPCs();
    void updateCharacterFSMs(float deltaTime, float time);
    void updateWorldTransforms(float time);
    void updatePlayerRayIntersections();
    void handlePicking(InputManagerPtr input, float time);

    //Rendering
    void renderPlayerUI();
    void renderAnimationUI();
    void renderNPCUI();
    void renderGameInfoUI();

    void renderEntities();
	void renderMesh(float time);
	void beginRenderingPass();
	void endRenderingPass();
	void renderDebugBoneGizmos();
	void renderDebugShapes();
	void updateViewProjectionMatrices(int windowWidth, int windowHeight);
};

#endif