
#include <entt/entt.hpp>
#include "glmcommon.hpp"
#include "imgui.h"
#include "Log.hpp"
#include "Game.hpp"
#include <SDL.h>


bool Game::init()
{
   
	initRenderers();
    
    entity_registry = std::make_shared<entt::registry>();
   
	initMeshes();
	initEntities();
	initWorldTransforms();

    return true;
}

void Game::update(
    float time,
    float deltaTime,
    InputManagerPtr input)
{
    updateCamera(input);
    updatePlayer(deltaTime, input);

    updateMovement(deltaTime);
    updateCameraTarget();
    updateNPCs();
    updateCharacterFSMs(deltaTime, time);
    updateWorldTransforms(time);
    updatePlayerRayIntersections();
    handlePicking(input, time);
}

void Game::render(
    float time,
    int windowWidth,
    int windowHeight)
{
    renderUI();

	updateViewProjectionMatrices(windowWidth, windowHeight);

    beginRenderingPass();
	renderEntities();
    renderMesh(time);
	renderDebugBoneGizmos();
    endRenderingPass();

    renderDebugShapes();
}

void Game::renderUI()
{
    renderPlayerUI();
    renderAnimationUI();
    renderNPCUI();
    renderGameInfoUI();
}

void Game::destroy()
{

}

void Game::updateCamera(
    InputManagerPtr input)
{
    // Fetch mouse and compute movement since last frame
    auto mouse = input->GetMouseState();
    glm::ivec2 mouse_xy{ mouse.x, mouse.y };
    glm::ivec2 mouse_xy_diff{ 0, 0 };
    if (mouse.leftButton && camera.mouse_xy_prev.x >= 0)
        mouse_xy_diff = camera.mouse_xy_prev - mouse_xy;
    camera.mouse_xy_prev = mouse_xy;

    // Update camera rotation from mouse movement
    camera.yaw += mouse_xy_diff.x * camera.sensitivity;
    camera.pitch += mouse_xy_diff.y * camera.sensitivity;
    camera.pitch = glm::clamp(camera.pitch, -glm::radians(89.0f), 0.0f);

    // Update camera position
    const glm::vec4 rotatedPos = glm_aux::R(camera.yaw, camera.pitch) * glm::vec4(0.0f, 0.0f, camera.distance, 1.0f);
    camera.pos = camera.lookAt + glm::vec3(rotatedPos);
}

void Game::updatePlayer(float deltaTime, InputManagerPtr input)
{
    // Fetch the player controller and velocity components (this could be part of an entity system if using one)
    auto view = entity_registry->view<PlayerController, Velocity>();

    for (auto entity : view)
    {
        auto& pc = view.get<PlayerController>(entity);
        auto& v = view.get<Velocity>(entity);

        // Update the movement direction based on player input
        PlayerControllerSystem(pc, v, input, camera);

        // You can apply any additional logic here, such as position updates or animations

        // Update player's forward view ray (this could be useful for AI, camera systems, etc.)
        player.viewRay = glm_aux::Ray{ player.pos + glm::vec3(0.0f, 2.0f, 0.0f), player.fwd };
    }
}


void Game::MovingSystem(Game::Tfm& tfm, Game::Velocity& v, float deltaTime)
{
    tfm.position += v.velocity * deltaTime;
}
void Game::PlayerControllerSystem(Game::PlayerController& pc, Game::Velocity& v, const InputManagerPtr& input, const Camera& camera)
{
    using Key = eeng::InputManager::Key;

    bool W = input->IsKeyPressed(Key::W);
    bool A = input->IsKeyPressed(Key::A);
    bool S = input->IsKeyPressed(Key::S);
    bool D = input->IsKeyPressed(Key::D);

    glm::vec3 fwd = glm_aux::R(camera.yaw, glm_aux::vec3_010) * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f);
    glm::vec3 right = glm::cross(fwd, glm_aux::vec3_010);

    glm::vec3 moveDir =
        fwd * ((W ? 1.0f : 0.0f) + (S ? -1.0f : 0.0f)) +
        right * ((A ? -1.0f : 0.0f) + (D ? 1.0f : 0.0f));

    if (glm::length(moveDir) > 0.0f) {
        moveDir = glm::normalize(moveDir) * player.velocity; // or just hardcode to 5.0f for now
    }

    pc.direction = moveDir;
    v.velocity = moveDir;

    eeng::Log("IsKeyPressed W: %d, A: %d, S: %d, D: %d", W, A, S, D);
    eeng::Log("Resulting moveDir: %s", glm_aux::to_string(moveDir).c_str());

    if (input->IsKeyPressed(Key::G))
    {
        showBoneGizmos = !showBoneGizmos;
        eeng::Log("Bone gizmos: %s", showBoneGizmos ? "ON" : "OFF");
    }
}
void Game::RenderSystem(eeng::ForwardRendererPtr& forwardRenderer, Tfm& tfm, MeshComponent& entityMesh)
{
    glm::mat4 objWorldMatrix = glm_aux::TRS(
        tfm.position,
        tfm.rotation.y, { 0, 1, 0 },
        tfm.scale);


	forwardRenderer->renderMesh(entityMesh.mesh, objWorldMatrix);
}
void Game::NPCControllerSystem(NPCController& npcc, Tfm& tfm, Velocity& v)
{
    if (npcc.waypoints.empty()) return;

    glm::vec3 target = npcc.waypoints[npcc.currentWaypoint];
    glm::vec3 toTarget = target - tfm.position;

    float distance = glm::length(toTarget);

    if (distance < 0.5f) // close enough to switch
    {
        npcc.currentWaypoint = (npcc.currentWaypoint + 1) % npcc.waypoints.size();
        return;
    }

    glm::vec3 direction = glm::normalize(toTarget);
    v.velocity = direction * npcc.speed;
}
void Game::FSM(MeshComponent& mesh, const Velocity& v, float dt)
{
    enum State
    {
        IDLE,
        WALKING,
        RUNNING
    };

    State currentState;
    float speed = glm::length(v.velocity);

    if (speed == 0)
    {
        currentState = IDLE;
    }
    else if (speed <= 7)
    {
        currentState = WALKING; 
    }
    else
    {
        currentState = RUNNING; 
    }

    switch (currentState)
    {
    case IDLE:
		mesh.mesh->animate(1, dt * characterAnimSpeed);
		break;
	case WALKING:
        mesh.mesh->animate(2, dt * characterAnimSpeed);
        break;
    case RUNNING:
        mesh.mesh->animate(3, dt * characterAnimSpeed);
        break;
    default:
        
        break;
    }
}
void Game::FSMWithBlend(MeshComponent& mesh, Velocity& v, AnimState& anim, float deltaTime, float time)
{
    enum State { IDLE, WALKING, RUNNING };

    float speed = glm::length(v.velocity);
    int newState;

    if (speed == 0.0f)
        newState = IDLE;
    else if (speed <= 7.0f)
        newState = WALKING;
    else
        newState = RUNNING;

    if (newState != anim.currentState)
    {
        anim.previousState = anim.currentState;
        anim.currentState = newState;
        anim.blendTimer = 0.0f;
    }

    anim.blendTimer += deltaTime;
    float blendFactor = glm::clamp(anim.blendTimer / 0.5f, 0.0f, 1.0f);
    if (useDebugBlend) blendFactor = debugBlendFactor;

    mesh.mesh->animateBlend(
        anim.previousState + 1,
        anim.currentState + 1,
        time,
        time,
        blendFactor);
}

//Entities
void Game::initEntities() {
    entity_registry = std::make_shared<entt::registry>();

    initNPCEntity();
    initPlayerEntity();
    initFoxEntity();
}
void Game::initNPCEntity() 
{
    auto entNPC = entity_registry->create();
    entity_registry->emplace<Tfm>(entNPC, Tfm{
       { 10.0f, 0.0f, 10.0f },
       { 0, 0, 0 },
       { 0.01f, 0.01f, 0.01f } });
    entity_registry->emplace<Velocity>(entNPC, Velocity{ glm::vec3(0.0f) });
    entity_registry->emplace<MeshComponent>(entNPC, MeshComponent{ foxMesh });

    NPCController npc = {};
    npc.waypoints = {
        glm::vec3(0, 0, 0),
        glm::vec3(10, 0, 0),
        glm::vec3(10, 0, 10),
        glm::vec3(0, 0, 10)
    };
    npc.speed = 2.0f;
    entity_registry->emplace<NPCController>(entNPC, npc);
}
void Game::initFoxEntity() 
{
    auto entFox = entity_registry->create();
    entity_registry->emplace<Tfm>(entFox, Tfm{
        { 5.0f, 0.0f, 5.0f },
        { 0, 0, 0 },
        { 0.01f, 0.01f, 0.01f } });
    entity_registry->emplace<MeshComponent>(entFox, MeshComponent{ foxMesh });
    entity_registry->emplace<Velocity>(entFox, Velocity{ {1,1,1} });
}
void Game::initPlayerEntity() 
{
    auto entPlayer = entity_registry->create();
    entity_registry->emplace<Tfm>(entPlayer, Tfm{
       { 5.0f, 0.0f, 5.0f },
       { 0, 0, 0 },
       { 0.01f, 0.01f, 0.01f } });
    entity_registry->emplace<Velocity>(entPlayer, Velocity{ glm::vec3(0.0f) });
    entity_registry->emplace<MeshComponent>(entPlayer, MeshComponent{ characterMesh });
    entity_registry->emplace<PlayerController>(entPlayer);
    entity_registry->emplace<AnimState>(entPlayer);
}
void Game::initMeshes()
{
    grassMesh = std::make_shared<eeng::RenderableMesh>();
    grassMesh->load("assets/grass/grass_trees_merged2.fbx", false);

    horseMesh = std::make_shared<eeng::RenderableMesh>();
    horseMesh->load("assets/Animals/Horse.fbx", false);

    characterMesh = std::make_shared<eeng::RenderableMesh>();

    foxMesh = std::make_shared<eeng::RenderableMesh>();
    foxMesh->load("assets/Animals/Fox.fbx", false);

    marcoMesh = std::make_shared<eeng::RenderableMesh>();
    marcoMesh->load("assets/Animals/Horse.fbx", false);

    // Things that were tehre from the start, animations, annie etc
#if 0
// Character
    characterMesh->load("assets/Ultimate Platformer Pack/Character/Character.fbx", false);
#endif
#if 0
    // Enemy
    characterMesh->load("assets/Ultimate Platformer Pack/Enemies/Bee.fbx", false);
#endif
#if 0
    // ExoRed 5.0.1 PACK FBX, 60fps, No keyframe reduction
    characterMesh->load("assets/ExoRed/exo_red.fbx");
    characterMesh->load("assets/ExoRed/idle (2).fbx", true);
    characterMesh->load("assets/ExoRed/walking.fbx", true);
    // Remove root motion
    characterMesh->removeTranslationKeys("mixamorig:Hips");
#endif
#if 1
    // Amy 5.0.1 PACK FBX
    characterMesh->load("assets/Amy/Ch46_nonPBR.fbx");
    characterMesh->load("assets/Amy/idle.fbx", true);
    characterMesh->load("assets/Amy/walking.fbx", true);
    characterMesh->load("assets/Amy/running.fbx", true);
    // Remove root motion
    characterMesh->removeTranslationKeys("mixamorig:Hips");
#endif
#if 0
    // Eve 5.0.1 PACK FBX
    // Fix for assimp 5.0.1 (https://github.com/assimp/assimp/issues/4486)
    // FBXConverter.cpp, line 648: 
    //      const float zero_epsilon = 1e-6f; => const float zero_epsilon = Math::getEpsilon<float>();
    characterMesh->load("assets/Eve/Eve By J.Gonzales.fbx");
    characterMesh->load("assets/Eve/idle.fbx", true);
    characterMesh->load("assets/Eve/walking.fbx", true);
    // Remove root motion
    characterMesh->removeTranslationKeys("mixamorig:Hips");
#endif
}
void Game::initWorldTransforms() 
{
    grassWorldMatrix = glm_aux::TRS(
        { 0.0f, 0.0f, 0.0f },
        0.0f, { 0, 1, 0 },
        { 100.0f, 100.0f, 100.0f });

    horseWorldMatrix = glm_aux::TRS(
        { 30.0f, 0.0f, -35.0f },
        35.0f, { 0, 1, 0 },
        { 0.01f, 0.01f, 0.01f });
}
void Game::initRenderers() 
{
    forwardRenderer = std::make_shared<eeng::ForwardRenderer>();
    forwardRenderer->init("shaders/phong_vert.glsl", "shaders/phong_frag.glsl");

    shapeRenderer = std::make_shared<ShapeRendering::ShapeRenderer>();
    shapeRenderer->init();
}

// Update 
void Game::updateMovement(float deltaTime) 
{
    auto view = entity_registry->view<Tfm, Velocity>();
    for (auto entity : view) {
        auto& tfm = view.get<Tfm>(entity);
        auto& vel = view.get<Velocity>(entity);
        Game::MovingSystem(tfm, vel, deltaTime);
    }
}
void Game::updateCameraTarget() 
{
    {
        auto view = entity_registry->view<PlayerController, Tfm>();
        for (auto entity : view) {
            const auto& tfm = view.get<Tfm>(entity);
            camera.lookAt = tfm.position;
            break; // Ifall det bara �r en spelare
        }
    }
}
void Game::updateNPCs() 
{
    auto npcs = entity_registry->view<NPCController, Tfm, Velocity>();
    for (auto entity : npcs) {
        auto& npc = npcs.get<NPCController>(entity);
        auto& tfm = npcs.get<Tfm>(entity);
        auto& vel = npcs.get<Velocity>(entity);
        NPCControllerSystem(npc, tfm, vel);
    }
}
void Game::updateCharacterFSMs(float deltaTime, float time) 
{
    auto characters = entity_registry->view<Velocity, MeshComponent, AnimState>();
    for (auto entity : characters) {
        auto& vel = characters.get<Velocity>(entity);
        auto& mesh = characters.get<MeshComponent>(entity);
        auto& anim = characters.get<AnimState>(entity);

        if (useBlendingFSM)
            FSMWithBlend(mesh, vel, anim, deltaTime, time);
        else
            FSM(mesh, vel, time);
    }
}
void Game::updateWorldTransforms(float time) 
{
    pointlight.pos = glm::vec3(
        glm_aux::R(time * 0.1f, { 0.0f, 1.0f, 0.0f }) *
        glm::vec4(100.0f, 100.0f, 100.0f, 1.0f));

    characterWorldMatrix1 = glm_aux::TRS(player.pos, 0.0f, { 0, 1, 0 }, { 0.03f, 0.03f, 0.03f });
    characterWorldMatrix2 = glm_aux::TRS({ -3, 0, 0 }, time * glm::radians(50.0f), { 0, 1, 0 }, { 0.03f, 0.03f, 0.03f });
    characterWorldMatrix3 = glm_aux::TRS({ 3, 0, 0 }, time * glm::radians(50.0f), { 0, 1, 0 }, { 0.03f, 0.03f, 0.03f });
}
void Game::updatePlayerRayIntersections() 
{
    glm_aux::intersect_ray_AABB(player.viewRay, character_aabb2.min, character_aabb2.max);
    glm_aux::intersect_ray_AABB(player.viewRay, character_aabb3.min, character_aabb3.max);
    glm_aux::intersect_ray_AABB(player.viewRay, horse_aabb.min, horse_aabb.max);
}
void Game::handlePicking(InputManagerPtr input, float time) 
{
    // We can also compute a ray from the current mouse position,
    // to use for object picking and such ...
    if (input->GetMouseState().rightButton)
    {
        glm::ivec2 windowPos(camera.mouse_xy_prev.x, matrices.windowSize.y - camera.mouse_xy_prev.y);
        auto ray = glm_aux::world_ray_from_window_coords(windowPos, matrices.V, matrices.P, matrices.VP);
        // Intersect with e.g. AABBs ...

        eeng::Log("Picking ray origin = %s, dir = %s",
            glm_aux::to_string(ray.origin).c_str(),
            glm_aux::to_string(ray.dir).c_str());
    }
}

// UI
void Game::renderPlayerUI() 
{
    ImGui::Begin("Player Settings");

    ImGui::Text("Modify player settings:");
    ImGui::Text("Player position: (%.1f, %.1f, %.1f)", player.pos.x, player.pos.y, player.pos.z);
    ImGui::Text("Player Velocity: (%.1f)", player.velocity);
    ImGui::SliderFloat("Player max velocity", &player.velocity, 1.0f, 20.0f);

    ImGui::End();
}
void Game::renderAnimationUI() 
{
    static const char* stateNames[] = {
   "Idle",
   "Walking",
   "Running"
    };
    ImGui::Begin("Animation Settings");

    ImGui::Checkbox("Use manual blend factor", &useDebugBlend);
    ImGui::SliderFloat("Manual Blend Factor", &debugBlendFactor, 0.0f, 1.0f);
    ImGui::Checkbox("Use Blending FSM", &useBlendingFSM);

    // Show current animation state
    auto characters = entity_registry->view<AnimState>();
    for (auto entity : characters)
    {
        auto& anim = characters.get<AnimState>(entity);

        ImGui::Text("Current FSM State: %s", stateNames[anim.currentState]);
        break;
    }

    ImGui::End();
}
void Game::renderNPCUI() 
{
    ImGui::Begin("NPC Settings");

    ImGui::Text("Modify NPC settings:");

    static float foxScale = 0.01f;
    ImGui::SliderFloat("Fox scale", &foxScale, 0.001f, 0.05f);

    auto view = entity_registry->view<MeshComponent, Tfm>();
    for (auto entity : view)
    {
        auto& mesh = view.get<MeshComponent>(entity);
        if (mesh.mesh == foxMesh) // only change fox
        {
            auto& tfm = view.get<Tfm>(entity);
            tfm.scale = glm::vec3(foxScale);
        }
    }

    // Adjust NPC Behav�or
    auto npcs = entity_registry->view<NPCController>();
    for (auto entity : npcs)
    {
        auto& npc = npcs.get<NPCController>(entity);
        ImGui::SliderFloat("NPC Speed", &npc.speed, 0.1f, 10.0f);
        break; // just show for 1 for now
    }

    ImGui::End();
}
void Game::renderGameInfoUI()
{
    ImGui::Begin("Game Info");

    ImGui::Text("In-game Time: %.2f", SDL_GetTicks() / 1000.0f);

    ImGui::Text("Drawcall count %i", drawcallCount);

    ImGui::Checkbox("Show Bone Gizmos", &showBoneGizmos);

    if (ImGui::ColorEdit3("Light color",
        glm::value_ptr(pointlight.color),
        ImGuiColorEditFlags_NoInputs))
    {
    }

    if (characterMesh)
    {
        // Combo (drop-down) for animation clip
        int curAnimIndex = characterAnimIndex;
        std::string label = (curAnimIndex == -1 ? "Bind pose" : characterMesh->getAnimationName(curAnimIndex));
        if (ImGui::BeginCombo("Character animation##animclip", label.c_str()))
        {
            // Bind pose item
            const bool isSelected = (curAnimIndex == -1);
            if (ImGui::Selectable("Bind pose", isSelected))
                curAnimIndex = -1;
            if (isSelected)
                ImGui::SetItemDefaultFocus();

            // Clip items
            for (int i = 0; i < characterMesh->getNbrAnimations(); i++)
            {
                const bool isSelected = (curAnimIndex == i);
                const auto label = characterMesh->getAnimationName(i) + "##" + std::to_string(i);
                if (ImGui::Selectable(label.c_str(), isSelected))
                    curAnimIndex = i;
                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
            characterAnimIndex = curAnimIndex;
        }

        // In-world position label
        const auto VP_P_V = matrices.VP * matrices.P * matrices.V;
        auto world_pos = glm::vec3(horseWorldMatrix[3]);
        glm::ivec2 window_coords;
        if (glm_aux::window_coords_from_world_pos(world_pos, VP_P_V, window_coords))
        {
            ImGui::SetNextWindowPos(
                ImVec2{ float(window_coords.x), float(matrices.windowSize.y - window_coords.y) },
                ImGuiCond_Always,
                ImVec2{ 0.0f, 0.0f });
            ImGui::PushStyleColor(ImGuiCol_WindowBg, 0x80000000);
            ImGui::PushStyleColor(ImGuiCol_Text, 0xffffffff);

            ImGuiWindowFlags flags =
                ImGuiWindowFlags_NoDecoration |
                ImGuiWindowFlags_NoInputs |
                // ImGuiWindowFlags_NoBackground |
                ImGuiWindowFlags_AlwaysAutoResize;

            if (ImGui::Begin("In-World Debug Label", nullptr, flags))
            {
                ImGui::Text("In-world GUI element");
                ImGui::Text("Window pos (%i, %i)", window_coords.x, window_coords.x);
                ImGui::Text("World pos (%1.1f, %1.1f, %1.1f)", world_pos.x, world_pos.y, world_pos.z);
                ImGui::End();
            }
            ImGui::PopStyleColor(2);
        }
    }

    ImGui::SliderFloat("Animation speed", &characterAnimSpeed, 0.1f, 5.0f);

    ImGui::End(); 
}

// Rendering
void Game::renderEntities() {
    auto view = entity_registry->view<Tfm, MeshComponent>();
    for (auto entity : view) {
        auto& tfm = view.get<Tfm>(entity);
        auto& mesh = view.get<MeshComponent>(entity);
        RenderSystem(forwardRenderer, tfm, mesh);
    }
}
void Game::renderMesh(float time) 
{
    // Grass
    forwardRenderer->renderMesh(grassMesh, grassWorldMatrix);
    grass_aabb = grassMesh->m_model_aabb.post_transform(grassWorldMatrix);

    // Horse
    horseMesh->animate(3, time);
    forwardRenderer->renderMesh(horseMesh, horseWorldMatrix);
    horse_aabb = horseMesh->m_model_aabb.post_transform(horseWorldMatrix);

    // Character, instance 1
    characterMesh->animate(characterAnimIndex, time * characterAnimSpeed);
    forwardRenderer->renderMesh(characterMesh, characterWorldMatrix1);
    character_aabb1 = characterMesh->m_model_aabb.post_transform(characterWorldMatrix1);

    // Character, instance 2
    characterMesh->animate(1, time * characterAnimSpeed);
    forwardRenderer->renderMesh(characterMesh, characterWorldMatrix2);
    character_aabb2 = characterMesh->m_model_aabb.post_transform(characterWorldMatrix2);

    // Character, instance 3
    characterMesh->animate(2, time * characterAnimSpeed);
    forwardRenderer->renderMesh(characterMesh, characterWorldMatrix3);
    character_aabb3 = characterMesh->m_model_aabb.post_transform(characterWorldMatrix3);

}
void Game::beginRenderingPass() {
    forwardRenderer->beginPass(
        matrices.P,
        matrices.V,
        pointlight.pos,
        pointlight.color,
        camera.pos
    );
}
void Game::endRenderingPass() {
    drawcallCount = forwardRenderer->endPass();
}
void Game::renderDebugBoneGizmos() {
    if (!showBoneGizmos || !characterMesh) return;

    float axisLen = 25.0f;
    for (int i = 0; i < characterMesh->boneMatrices.size(); ++i) {
        glm::mat4 global =
            characterWorldMatrix3 *
            characterMesh->boneMatrices[i] *
            glm::inverse(characterMesh->m_bones[i].inversebind_tfm);

        glm::vec3 pos = glm::vec3(global[3]);
        glm::vec3 right = glm::vec3(global[0]); // X
        glm::vec3 up = glm::vec3(global[1]);    // Y
        glm::vec3 fwd = glm::vec3(global[2]);   // Z

        shapeRenderer->push_states(ShapeRendering::Color4u::Red);
        shapeRenderer->push_line(pos, pos + axisLen * right);

        shapeRenderer->push_states(ShapeRendering::Color4u::Green);
        shapeRenderer->push_line(pos, pos + axisLen * up);

        shapeRenderer->push_states(ShapeRendering::Color4u::Blue);
        shapeRenderer->push_line(pos, pos + axisLen * fwd);

        shapeRenderer->pop_states<ShapeRendering::Color4u>();
        shapeRenderer->pop_states<ShapeRendering::Color4u>();
        shapeRenderer->pop_states<ShapeRendering::Color4u>();
    }
}
void Game::renderDebugShapes() 
{
    // Draw player view ray
    if (player.viewRay)
    {
        shapeRenderer->push_states(ShapeRendering::Color4u{ 0xff00ff00 });
        shapeRenderer->push_line(player.viewRay.origin, player.viewRay.point_of_contact());
    }
    else
    {
        shapeRenderer->push_states(ShapeRendering::Color4u{ 0xffffffff });
        shapeRenderer->push_line(player.viewRay.origin, player.viewRay.origin + player.viewRay.dir * 100.0f);
    }
    shapeRenderer->pop_states<ShapeRendering::Color4u>();

    // Draw object bases
    {
        shapeRenderer->push_basis_basic(characterWorldMatrix1, 1.0f);
        shapeRenderer->push_basis_basic(characterWorldMatrix2, 1.0f);
        shapeRenderer->push_basis_basic(characterWorldMatrix3, 1.0f);
        shapeRenderer->push_basis_basic(grassWorldMatrix, 1.0f);
        shapeRenderer->push_basis_basic(horseWorldMatrix, 1.0f);
    }

    // Draw AABBs
    {
        shapeRenderer->push_states(ShapeRendering::Color4u{ 0xFFE61A80 });
        shapeRenderer->push_AABB(character_aabb1.min, character_aabb1.max);
        shapeRenderer->push_AABB(character_aabb2.min, character_aabb2.max);
        shapeRenderer->push_AABB(character_aabb3.min, character_aabb3.max);
        shapeRenderer->push_AABB(horse_aabb.min, horse_aabb.max);
        shapeRenderer->push_AABB(grass_aabb.min, grass_aabb.max);
        shapeRenderer->pop_states<ShapeRendering::Color4u>();
    }

#if 0
    // Demo draw other shapes
    {
        shapeRenderer->push_states(glm_aux::T(glm::vec3(0.0f, 0.0f, -5.0f)));
        ShapeRendering::DemoDraw(shapeRenderer);
        shapeRenderer->pop_states<glm::mat4>();
    }
#endif

    // Draw shape batches
    shapeRenderer->render(matrices.P * matrices.V);
    shapeRenderer->post_render();
}
void Game::updateViewProjectionMatrices(int windowWidth, int windowHeight)
{
    matrices.windowSize = glm::ivec2(windowWidth, windowHeight);

    // Projection matrix
    const float aspectRatio = float(windowWidth) / windowHeight;
    matrices.P = glm::perspective(glm::radians(60.0f), aspectRatio, camera.nearPlane, camera.farPlane);

    // View matrix
    matrices.V = glm::lookAt(camera.pos, camera.lookAt, camera.up);

    matrices.VP = glm_aux::create_viewport_matrix(0.0f, 0.0f, windowWidth, windowHeight, 0.0f, 1.0f);
}