// This has been adapted from the Vulkan tutorial
#include <cstdlib>
#include <glm/fwd.hpp>
#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>
#include <iostream>
#include <memory>
#include <sstream>

#include <json.hpp>
#include <vector>
#include <vulkan/vulkan_core.h>

#include "modules/Starter.hpp"
#include "modules/TextMaker.hpp"
#include "modules/Scene.hpp"
#include "modules/Animations.hpp"
// #include "../src/Libs.cpp"

// The uniform buffer object used in this example
struct VertexChar {
  glm::vec3 pos;
  glm::vec3 norm;
  glm::vec2 UV;
  glm::uvec4 jointIndices;
  glm::vec4 weights;
};

struct VertexSimp {
  glm::vec3 pos;
  glm::vec3 norm;
  glm::vec2 UV;
};

struct skyBoxVertex {
  glm::vec3 pos;
};

struct VertexTan {
  glm::vec3 pos;
  glm::vec3 norm;
  glm::vec2 UV;
  glm::vec4 tan;
};

struct GlobalUniformBufferObject {
  alignas(16) glm::vec3 lightDir;
  alignas(16) glm::vec4 lightColor;
  alignas(16) glm::vec3 eyePos;
};

struct UniformBufferObjectChar {
  alignas(16) glm::vec4 debug1;
  alignas(16) glm::mat4 mvpMat[65];
  alignas(16) glm::mat4 mMat[65];
  alignas(16) glm::mat4 nMat[65];
};

struct UniformBufferObjectSimp {
  alignas(16) glm::mat4 mvpMat[100];
  alignas(16) glm::mat4 mMat[100];
  alignas(16) glm::mat4 nMat[100];
};

struct ComponentModel {
  Model model;
  DescriptorSet previewDescriptorSet;
  DescriptorSet standardDescriptorSet;
};

struct Structure {
  std::vector<ComponentModel> components;
};

struct skyBoxUniformBufferObject {
  alignas(16) glm::mat4 mvpMat;
};

struct GridUniformBufferObject {
  alignas(16) glm::mat4 gVP;
  alignas(16) glm::vec3 camWorldPos;
  alignas(4) float gridSize;
};

class Factotum : public BaseProject {
protected:
  // Here you list all the Vulkan objects you need:

  // Descriptor Layouts [what will be passed to the shaders]
  DescriptorSetLayout DSLlocalChar, DSLlocalSimp, DSLlocalPBR, DSLglobal,
      DSLskyBox, DSLgrid, DSLlocalPBRCoal, DSLwireframe;

  // Vertex formants, Pipelines [Shader couples] and Render passes
  VertexDescriptor VDchar;
  VertexDescriptor VDsimp;
  VertexDescriptor VDskyBox;
  VertexDescriptor VDtan;
  VertexDescriptor VDgrid;
  RenderPass RP;
  Pipeline P_Ground, Pchar, PskyBox, P_PBR, P_PBRCoal, Pwireframe, Pgrid;
  //*DBG*/Pipeline PDebug;

  // Models, textures and Descriptors (values assigned to the uniforms)
  Scene SC;
  std::vector<VertexDescriptorRef> VDRs;
  std::vector<TechniqueRef> PRs;

  Structure minerStructure, conveyorStructure, furnaceStructure,
      mineralMinedStructure, metalIngotStructure;
  bool isPlacing = false;
  glm::mat4 previewTransform;
  float previewRotation = 0.0f;
  DescriptorSet DSgrid, DSglobal;

  const glm::vec4 validColorWRF = glm::vec4(0.0f, 1.0f, 1.0f, 1.0f);   // Cyan
  const glm::vec4 invalidColorWRF = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f); // Red
  bool isPlacementValid = true;

  // inventory
  enum InventoryItem { MINER, CONVEYOR_BELT, FURNACE };
  InventoryItem inventoryItem = MINER;

  // to provide textual feedback
  TextMaker txt;

  // Other application parameters
  float Ar; // Aspect ratio

  glm::mat4 ViewPrj;
  glm::mat4 World;
  glm::vec3 Pos = glm::vec3(0, 5, 0);
  glm::vec3 cameraPos;
  float Yaw = 0.0f;
  float Pitch = 0.0f;
  float Roll = glm::radians(0.0f);
  float gridSize = 2.f;

  glm::vec4 debug1 = glm::vec4(0);

  // Mouse handling
  double lastX, lastY;
  bool firstMouse = true;

  //-Timing
  float lastFrame = 0.0f;

  struct PlacedObject {
    int id;
    InventoryItem type;
    glm::vec3 position;
    float rotation;
    virtual ~PlacedObject() = default; // Add this line
  };
  struct PlacedMiner : PlacedObject {
    float lastSpawnTime = 0.0;
  };

  struct PlacedConveyor : PlacedObject {};
  struct PlacedFurnace : PlacedObject {
    std::vector<float> coal;
    std::vector<float> ore;
  };

  std::vector<std::shared_ptr<PlacedObject>> placedObjects;
  int nextPlacedObjectId = 0;

  struct SpawnedMineral {
    glm::vec3 initialPosition;
    glm::vec3 position;
    glm::vec3 direction;
    float spawnTime;
  };
  std::vector<SpawnedMineral> spawnedMinerals;

  struct Mineral {
    glm::vec3 position;
    float remainingAmount = 100.0f; // Example: 100 units per mineral node
    bool isBeingMined = false;
  };
  std::vector<Mineral> minerals; // To store mineral data loaded from scene.json

  // Here you set the main application parameters
  void setWindowParameters() {
    // window size, titile and initial background
    windowWidth = 800;
    windowHeight = 600;
    windowTitle = "Factotum";
    windowResizable = GLFW_TRUE;

    // Initial aspect ratio
    Ar = 4.0f / 3.0f;
  }

  // What to do when the window changes size
  void onWindowResize(int w, int h) {
    std::cout << "Window resized to: " << w << " x " << h << "\n";
    Ar = (float)w / (float)h;
    // Update Render Pass
    RP.width = w;
    RP.height = h;

    // updates the textual output
    txt.resizeScreen(w, h);
  }

  // Here you load and setup all your Vulkan Models and Texutures.
  // Here you also create your Descriptor set layouts and load the shaders for
  // the pipelines
  void localInit() {
    // Code used to take the mouse callback
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetWindowUserPointer(window, this);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);

    // Descriptor Layouts [what will be passed to the shaders]
    DSLglobal.init(
        this,
        {// this array contains the binding:
         // first  element : the binding number
         // second element : the type of element (buffer or texture)
         // third  element : the pipeline stage where it will be used
         {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS,
          sizeof(GlobalUniformBufferObject), 1}});

    DSLlocalChar.init(
        this, {// this array contains the binding:
               // first  element : the binding number
               // second element : the type of element (buffer or texture)
               // third  element : the pipeline stage where it will be used
               {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                VK_SHADER_STAGE_VERTEX_BIT, sizeof(UniformBufferObjectChar), 1},
               {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                VK_SHADER_STAGE_FRAGMENT_BIT, 0, 1}});

    DSLlocalSimp.init(
        this, {// this array contains the binding:
               // first  element : the binding number
               // second element : the type of element (buffer or texture)
               // third  element : the pipeline stage where it will be used
               {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                VK_SHADER_STAGE_VERTEX_BIT, sizeof(UniformBufferObjectSimp), 1},
               {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                VK_SHADER_STAGE_FRAGMENT_BIT, 0, 1},
               {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                VK_SHADER_STAGE_FRAGMENT_BIT, 1, 1}});

    DSLskyBox.init(this, {{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                           VK_SHADER_STAGE_VERTEX_BIT,
                           sizeof(skyBoxUniformBufferObject), 1},
                          {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                           VK_SHADER_STAGE_FRAGMENT_BIT, 0, 1}});

    DSLlocalPBR.init(
        this, {// this array contains the binding:
               // first  element : the binding number
               // second element : the type of element (buffer or texture)
               // third  element : the pipeline stage where it will be used
               {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                VK_SHADER_STAGE_VERTEX_BIT, sizeof(UniformBufferObjectSimp), 1},
               {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                VK_SHADER_STAGE_FRAGMENT_BIT, 0, 1},
               {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                VK_SHADER_STAGE_FRAGMENT_BIT, 1, 1},
               {3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                VK_SHADER_STAGE_FRAGMENT_BIT, 2, 1},
               {4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                VK_SHADER_STAGE_FRAGMENT_BIT, 3, 1}});

    DSLlocalPBRCoal.init(
        this, {// this array contains the binding:
               // first  element : the binding number
               // second element : the type of element (buffer or texture)
               // third  element : the pipeline stage where it will be used
               {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                VK_SHADER_STAGE_VERTEX_BIT, sizeof(UniformBufferObjectSimp), 1},
               {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                VK_SHADER_STAGE_FRAGMENT_BIT, 0, 1},
               {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                VK_SHADER_STAGE_FRAGMENT_BIT, 1, 1},
               {3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                VK_SHADER_STAGE_FRAGMENT_BIT, 2, 1},
               {4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                VK_SHADER_STAGE_FRAGMENT_BIT, 3, 1},
               {5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                VK_SHADER_STAGE_FRAGMENT_BIT, 4, 1}});

    DSLgrid.init(this, {{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                         VK_SHADER_STAGE_ALL_GRAPHICS,
                         sizeof(GridUniformBufferObject), 1}});

    DSLwireframe.init(this, {{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                              VK_SHADER_STAGE_VERTEX_BIT,
                              sizeof(UniformBufferObjectSimp), 1}});

    VDchar.init(
        this, {{0, sizeof(VertexChar), VK_VERTEX_INPUT_RATE_VERTEX}},
        {{0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(VertexChar, pos),
          sizeof(glm::vec3), POSITION},
         {0, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(VertexChar, norm),
          sizeof(glm::vec3), NORMAL},
         {0, 2, VK_FORMAT_R32G32_SFLOAT, offsetof(VertexChar, UV),
          sizeof(glm::vec2), UV},
         {0, 3, VK_FORMAT_R32G32B32A32_UINT, offsetof(VertexChar, jointIndices),
          sizeof(glm::uvec4), JOINTINDEX},
         {0, 4, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(VertexChar, weights),
          sizeof(glm::vec4), JOINTWEIGHT}});

    VDsimp.init(this, {{0, sizeof(VertexSimp), VK_VERTEX_INPUT_RATE_VERTEX}},
                {{0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(VertexSimp, pos),
                  sizeof(glm::vec3), POSITION},
                 {0, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(VertexSimp, norm),
                  sizeof(glm::vec3), NORMAL},
                 {0, 2, VK_FORMAT_R32G32_SFLOAT, offsetof(VertexSimp, UV),
                  sizeof(glm::vec2), UV}});

    VDskyBox.init(this,
                  {{0, sizeof(skyBoxVertex), VK_VERTEX_INPUT_RATE_VERTEX}},
                  {{0, 0, VK_FORMAT_R32G32B32_SFLOAT,
                    offsetof(skyBoxVertex, pos), sizeof(glm::vec3), POSITION}});

    VDtan.init(this, {{0, sizeof(VertexTan), VK_VERTEX_INPUT_RATE_VERTEX}},
               {{0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(VertexTan, pos),
                 sizeof(glm::vec3), POSITION},
                {0, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(VertexTan, norm),
                 sizeof(glm::vec3), NORMAL},
                {0, 2, VK_FORMAT_R32G32_SFLOAT, offsetof(VertexTan, UV),
                 sizeof(glm::vec2), UV},
                {0, 3, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(VertexTan, tan),
                 sizeof(glm::vec4), TANGENT}});

    // Grid vertex descriptor - no vertex attributes since positions are
    // hardcoded in shader
    VDgrid.init(this, {}, {});

    VDRs.resize(5);
    VDRs[0].init("VDchar", &VDchar);
    VDRs[1].init("VDsimp", &VDsimp);
    VDRs[2].init("VDskybox", &VDskyBox);
    VDRs[3].init("VDtan", &VDtan);
    VDRs[4].init("VDgrid", &VDgrid);

    // initializes the render passes
    RP.init(this);
    // sets the blue sky
    RP.properties[0].clearValue = {0.0f, 0.9f, 1.0f, 1.0f};

    // Pipelines [Shader couples]
    // The last array, is a vector of pointer to the layouts of the sets that
    // will be used in this pipeline. The first element will be set 0, and so
    // on..
    Pchar.init(this, &VDchar, "shaders/SimplePosNormUvTan.vert.spv",
               "shaders/CookTorrance.frag.spv",
               {&DSLglobal, &DSLlocalChar});

    P_Ground.init(this, &VDtan, "shaders/SimplePosNormUvTan.vert.spv",
                  "shaders/CookTorranceGround.frag.spv",
                  {&DSLglobal, &DSLlocalSimp});

    PskyBox.init(this, &VDskyBox, "shaders/SkyBoxShader.vert.spv",
                 "shaders/SkyBoxShader.frag.spv", {&DSLskyBox});
    PskyBox.setCompareOp(VK_COMPARE_OP_LESS_OR_EQUAL);
    PskyBox.setCullMode(VK_CULL_MODE_BACK_BIT);
    PskyBox.setPolygonMode(VK_POLYGON_MODE_FILL);

    P_PBR.init(this, &VDtan, "shaders/SimplePosNormUvTan.vert.spv",
               "shaders/CookTorranceObjects.frag.spv",
               {&DSLglobal, &DSLlocalPBR});

    P_PBRCoal.init(this, &VDtan, "shaders/SimplePosNormUvTan.vert.spv",
                   "shaders/CookTorranceEmissive.frag.spv",
                   {&DSLglobal, &DSLlocalPBRCoal});

    VkPushConstantRange pushConstRange{};
    pushConstRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstRange.offset = 0;
    pushConstRange.size = sizeof(glm::vec4);

    Pwireframe.init(this, &VDtan, "shaders/SimplePosNormUvTan.vert.spv",
                    "shaders/OnlyCyan.frag.spv", {&DSLglobal, &DSLwireframe},
                    {pushConstRange});
    Pwireframe.setPolygonMode(VK_POLYGON_MODE_LINE);
    // I want to be able to see the wireframe even from behind
    Pwireframe.setCullMode(VK_CULL_MODE_NONE);

    Pgrid.init(this, &VDgrid, "shaders/Grid.vert.spv", "shaders/Grid.frag.spv",
               {&DSLgrid});

    Pgrid.setTransparency(true);
    minerStructure.components.resize(4);

    AssetFile assetMiner;
    assetMiner.init("assets/models/miner/miner.gltf", GLTF);
    minerStructure.components[0].model.initFromAsset(this, &VDtan, &assetMiner,
                                                     "Cube.006", 0, "Cube.006");
    minerStructure.components[1].model.initFromAsset(this, &VDtan, &assetMiner,
                                                     "Cone", 0, "Cone");
    minerStructure.components[2].model.initFromAsset(this, &VDtan, &assetMiner,
                                                     "Cube", 0, "Cube");
    minerStructure.components[3].model.initFromAsset(this, &VDtan, &assetMiner,
                                                     "Cylinder", 0, "Cylinder");

    conveyorStructure.components.resize(3);
    AssetFile assetConveyor;
    assetConveyor.init("assets/models/conveyor_belt/conveyor_belt.gltf", GLTF);
    conveyorStructure.components[0].model.initFromAsset(
        this, &VDtan, &assetConveyor, "Cube.001", 0, "Cube.001");
    conveyorStructure.components[1].model.initFromAsset(
        this, &VDtan, &assetConveyor, "Cube.002", 0, "Cube.002");
    conveyorStructure.components[2].model.initFromAsset(
        this, &VDtan, &assetConveyor, "Cube.003", 0, "Cube.003");

    // for (auto &component : conveyorStructure.components) {
    //   component.model.Wm = glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f),
    //   glm::vec3(1.0f, 0.0f, 0.0f)) *
    //                        glm::scale(glm::mat4(1.0f), glm::vec3(0.1f,
    //                        0.1f, 1.0f)) * component.model.Wm;
    // }
    //
    // conveyorStructure.components[0].model.Wm =
    // glm::translate(glm::mat4(1.0f), glm::vec3(-0.9f, 0.0f, -0.1f)) *
    // conveyorStructure.components[0].model.Wm;
    // conveyorStructure.components[1].model.Wm =
    // glm::translate(glm::mat4(1.0f), glm::vec3(0.9f, 0.0f, -0.1f)) *
    // conveyorStructure.components[1].model.Wm;
    // conveyorStructure.components[2].model.Wm =
    // glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -0.1f)) *
    // conveyorStructure.components[2].model.Wm;

    furnaceStructure.components.resize(3);
    AssetFile assetFurnace;
    assetFurnace.init("assets/models/furnace_new/furnace.gltf", GLTF);
    furnaceStructure.components[0].model.initFromAsset(
        this, &VDtan, &assetFurnace, "LP_coal_Coal_0", 0, "LP_coal_Coal_0");
    furnaceStructure.components[1].model.initFromAsset(
        this, &VDtan, &assetFurnace, "LP_rail_Metal_0", 0, "LP_rail_Metal_0");
    furnaceStructure.components[2].model.initFromAsset(
        this, &VDtan, &assetFurnace, "LP_rail_Metal_0", 1, "LP_rail_Metal_0");

    mineralMinedStructure.components.resize(1);
    AssetFile assetMineralMined;
    assetMineralMined.init("assets/models/mineral_mined/Asteroid_1b.gltf",
                           GLTF);
    mineralMinedStructure.components[0].model.initFromAsset(
        this, &VDtan, &assetMineralMined, "Asteroid_1b", 0, "Asteroid_1b");
    mineralMinedStructure.components[0].model.Wm =
        glm::scale(glm::vec3(0.4)) *
        mineralMinedStructure.components[0].model.Wm;

    metalIngotStructure.components.resize(1);
    AssetFile assetMetalIngot;
    assetMetalIngot.init("assets/models/metal_ingot/scene.gltf", GLTF);
    metalIngotStructure.components[0].model.initFromAsset(
        this, &VDtan, &assetMetalIngot, "Ingot_LP_Ingot_0", 0,
        "Ingot_LP_Ingot_0");

    furnaceStructure.components[0].model.Wm =
        glm::translate(glm::mat4(1.0f), glm::vec3(-2.0f, 1.0f, .0f)) *
        glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f),
                    glm::vec3(1.0f, 0.0f, 0.0f)) *
        furnaceStructure.components[0].model.Wm;

    for (int i = 1; i < furnaceStructure.components.size(); i++) {
      furnaceStructure.components[i].model.Wm =
          glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.0f, 0.0f)) *
          glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f),
                      glm::vec3(1.0f, 0.0f, 0.0f)) *
          glm::rotate(glm::mat4(1.0f), glm::radians(27.0f),
                      glm::vec3(0.0f, 1.0f, 0.0f)) *
          furnaceStructure.components[i].model.Wm;
    }

    for (auto &component : furnaceStructure.components) {
      component.model.Wm =
          glm::scale(glm::vec3(0.7f)) *
          glm::translate(glm::mat4(1.0f), glm::vec3(-1.0f, 0.0f, 0.0f)) *
          component.model.Wm;
    }

    PRs.resize(5);
    PRs[0].init("CookTorrance",
                {{&Pchar,
                  {// Pipeline and DSL for the first pass
                   /*DSLglobal*/ {},
                   /*DSLlocalChar*/ {
                       /*t0*/ {true, 0, {}} // index 0 of the "texture" field in
                                            // the json file
                   }}}},
                /*TotalNtextures*/ 1, &VDchar);
    PRs[1].init("CookTorranceGround",
                {{&P_Ground,
                  {// Pipeline and DSL for the first pass
                   /*DSLglobal*/ {},
                   /*DSLlocalPBR*/ {
                       /*t0*/ {true, 0, {}}, // index 0 of the "texture" field
                                             // in the json file
                       /*t1*/ {true, 1, {}}, // index 1 of the "texture" field
                                             // in the json file
                   }}}},
                /*TotalNtextures*/ 2, &VDtan);
    PRs[2].init("SkyBox",
                {{&PskyBox,
                  {// Pipeline and DSL for the first pass
                   /*DSLskyBox*/ {
                       /*t0*/ {true, 0, {}} // index 0 of the "texture" field in
                                            // the json file
                   }}}},
                /*TotalNtextures*/ 1, &VDskyBox);
    PRs[3].init("PBR",
                {{&P_PBR,
                  {// Pipeline and DSL for the first pass
                   /*DSLglobal*/ {},
                   /*DSLlocalPBR*/ {
                       /*t0*/ {true, 0, {}}, // index 0 of the "texture" field
                                             // in the json file
                       /*t1*/ {true, 1, {}}, // index 1 of the "texture" field
                                             // in the json file
                       /*t2*/ {true, 2, {}}, // index 2 of the "texture" field
                                             // in the json file
                       /*t3*/ {true, 3, {}} // index 3 of the "texture" field in
                                            // the json file
                   }}}},
                /*TotalNtextures*/ 4, &VDtan);
    PRs[4].init("PBR_coal",
                {{&P_PBRCoal,
                  {// Pipeline and DSL for the first pass
                   /*DSLglobal*/ {},
                   /*DSLlocalPBRCoal*/ {
                       /*t0*/ {true, 0, {}}, // index 0 of the "texture" field
                                             // in the json file
                       /*t1*/ {true, 1, {}}, // index 1 of the "texture" field
                                             // in the json file
                       /*t2*/ {true, 2, {}}, // index 2 of the "texture" field
                                             // in the json file
                       /*t3*/ {true, 3, {}}, // index 3 of the "texture" field
                                             // in the json file
                       /*t4*/ {true, 4, {}} // index 4 of the "texture" field in
                                            // the json file
                   }}}},
                /*TotalNtextures*/ 5, &VDtan);

    // Models, textures and Descriptors (values assigned to the uniforms)

    // sets the size of the Descriptor Set Pool
    DPSZs.uniformBlocksInPool = 100;
    DPSZs.texturesInPool = 100;
    DPSZs.setsInPool = 100;

    std::cout << "\nLoading the scene\n\n";
    if (SC.init(this, /*Npasses*/ 1, VDRs, PRs, "assets/models/scene.json") !=
        0) {
      std::cout << "ERROR LOADING THE SCENE\n";
      exit(0);
    }

    // read minerals positions
    for (int i = 0; i < 4; i++) {
      minerals.push_back({glm::vec3(SC.TI[3].I[i].Wm[3]), 100.0f, false});
    }

    // initializes the textual output
    txt.init(this, windowWidth, windowHeight);

    // submits the main command buffer
    submitCommandBuffer("main", 0, populateCommandBufferAccess, this);

    // Prepares for showing the FPS count
    txt.print(1.0f, 1.0f, "FPS:", 1, "CO", false, false, true, TAL_RIGHT,
              TRH_RIGHT, TRV_BOTTOM, {1.0f, 0.0f, 0.0f, 1.0f},
              {0.8f, 0.8f, 0.0f, 1.0f});
    txt.print(0.0f, 0.0f, "+", 2, "CO", false, false, true, TAL_CENTER,
              TRH_CENTER, TRV_MIDDLE, {1.0f, 0.0f, 0.0f, 1.0f},
              {0.8f, 0.8f, 0.0f, 1.0f});
    txt.print(0.0f, 0.0f, "Rocket", 10, "CO", false, false, true, TAL_CENTER,
              TRH_CENTER, TRV_BOTTOM, {1.0f, 1.0f, 1.0f, 1.0f},
              {0.0f, 0.0f, 0.0f, 1.0f});
  }

  // Here you create your pipelines and Descriptor Sets!
  void pipelinesAndDescriptorSetsInit() {
    // creates the render pass
    RP.create();

    // This creates a new pipeline (with the current surface), using its shaders
    // for the provided render pass
    Pchar.create(&RP);
    P_Ground.create(&RP);
    PskyBox.create(&RP);
    P_PBR.create(&RP);
    Pwireframe.create(&RP);
    Pgrid.create(&RP);
    P_PBRCoal.create(&RP);

    // init miner components
    minerStructure.components[0].previewDescriptorSet.init(this, &DSLwireframe, {});
    minerStructure.components[0].standardDescriptorSet.init(this, &DSLlocalPBR,
                                                            {SC.T[45]->getViewAndSampler(), SC.T[46]->getViewAndSampler(),
                                                             SC.T[47]->getViewAndSampler(), SC.T[48]->getViewAndSampler()});
    minerStructure.components[1].previewDescriptorSet.init(this, &DSLwireframe, {});
    minerStructure.components[1].standardDescriptorSet.init(this, &DSLlocalPBR,
                                                            {SC.T[57]->getViewAndSampler(), SC.T[58]->getViewAndSampler(),
                                                             SC.T[59]->getViewAndSampler(), SC.T[60]->getViewAndSampler()});
    minerStructure.components[2].previewDescriptorSet.init(this, &DSLwireframe, {});
    minerStructure.components[2].standardDescriptorSet.init(this, &DSLlocalPBR,
                                                            {SC.T[53]->getViewAndSampler(), SC.T[54]->getViewAndSampler(),
                                                             SC.T[55]->getViewAndSampler(), SC.T[56]->getViewAndSampler()});
    minerStructure.components[3].previewDescriptorSet.init(this, &DSLwireframe, {});
    minerStructure.components[3].standardDescriptorSet.init(this, &DSLlocalPBR,
                                                            {SC.T[49]->getViewAndSampler(), SC.T[50]->getViewAndSampler(),
                                                             SC.T[51]->getViewAndSampler(), SC.T[52]->getViewAndSampler()});

//    for (auto &component : minerStructure.components) {
//      component.previewDescriptorSet.init(this, &DSLwireframe, {});
//
//      component.standardDescriptorSet.init(
//          this, &DSLlocalPBR,
//          {SC.T[0]->getViewAndSampler(), SC.T[1]->getViewAndSampler(),
//           SC.T[3]->getViewAndSampler(), SC.T[2]->getViewAndSampler()});
//    }

    // init conveyor components
    conveyorStructure.components[0].previewDescriptorSet.init(
        this, &DSLwireframe, {});
    conveyorStructure.components[0].standardDescriptorSet.init(
        this, &DSLlocalPBR,
        {SC.T[11]->getViewAndSampler(), SC.T[11]->getViewAndSampler(),
         SC.T[11]->getViewAndSampler(), SC.T[11]->getViewAndSampler()});
    conveyorStructure.components[1].previewDescriptorSet.init(
        this, &DSLwireframe, {});
    conveyorStructure.components[1].standardDescriptorSet.init(
        this, &DSLlocalPBR,
        {SC.T[11]->getViewAndSampler(), SC.T[11]->getViewAndSampler(),
         SC.T[11]->getViewAndSampler(), SC.T[11]->getViewAndSampler()});
    conveyorStructure.components[2].previewDescriptorSet.init(
        this, &DSLwireframe, {});
    conveyorStructure.components[2].standardDescriptorSet.init(
        this, &DSLlocalPBR,
        {SC.T[10]->getViewAndSampler(), SC.T[10]->getViewAndSampler(),
         SC.T[10]->getViewAndSampler(), SC.T[10]->getViewAndSampler()});

    // init furnace components
    furnaceStructure.components[0].previewDescriptorSet.init(this,
                                                             &DSLwireframe, {});
    furnaceStructure.components[0].standardDescriptorSet.init(
        this, &DSLlocalPBRCoal,
        {SC.T[15]->getViewAndSampler(), SC.T[18]->getViewAndSampler(),
         SC.T[37]->getViewAndSampler(), SC.T[38]->getViewAndSampler(),
         SC.T[17]->getViewAndSampler()});

    furnaceStructure.components[1].previewDescriptorSet.init(this,
                                                             &DSLwireframe, {});
    furnaceStructure.components[1].standardDescriptorSet.init(
        this, &DSLlocalPBR,
        {SC.T[19]->getViewAndSampler(), SC.T[21]->getViewAndSampler(),
         SC.T[39]->getViewAndSampler(), SC.T[40]->getViewAndSampler()});

    furnaceStructure.components[2].previewDescriptorSet.init(this,
                                                             &DSLwireframe, {});
    furnaceStructure.components[2].standardDescriptorSet.init(
        this, &DSLlocalPBR,
        {SC.T[12]->getViewAndSampler(), SC.T[13]->getViewAndSampler(),
         SC.T[35]->getViewAndSampler(), SC.T[36]->getViewAndSampler()});

    // init mineral mined structure
    mineralMinedStructure.components[0].previewDescriptorSet.init(
        this, &DSLwireframe, {});
    mineralMinedStructure.components[0].standardDescriptorSet.init(
        this, &DSLlocalPBR,
        {SC.T[27]->getViewAndSampler(), SC.T[28]->getViewAndSampler(),
         SC.T[29]->getViewAndSampler(), SC.T[30]->getViewAndSampler()});

    // init metal ingot structure
    metalIngotStructure.components[0].previewDescriptorSet.init(
        this, &DSLwireframe, {});
    metalIngotStructure.components[0].standardDescriptorSet.init(
        this, &DSLlocalPBR,
        {SC.T[31]->getViewAndSampler(), SC.T[32]->getViewAndSampler(),
         SC.T[33]->getViewAndSampler(), SC.T[34]->getViewAndSampler()});

    DSgrid.init(this, &DSLgrid, {});
    DSglobal.init(this, &DSLglobal, {});
    SC.pipelinesAndDescriptorSetsInit();
    txt.pipelinesAndDescriptorSetsInit();
  }

  // Here you destroy your pipelines and Descriptor Sets!
  void pipelinesAndDescriptorSetsCleanup() {
    Pchar.cleanup();
    P_Ground.cleanup();
    PskyBox.cleanup();
    P_PBR.cleanup();
    Pwireframe.cleanup();
    Pgrid.cleanup();
    RP.cleanup();
    P_PBRCoal.cleanup();

    // Cleanup descriptor sets for each component in minerStructurePreview
    for (auto &component : minerStructure.components) {
      component.previewDescriptorSet.cleanup();
      component.standardDescriptorSet.cleanup();
    }

    // conveyor cleanup
    for (auto &component : conveyorStructure.components) {
      component.previewDescriptorSet.cleanup();
      component.standardDescriptorSet.cleanup();
    }

    // furnace cleanup
    for (auto &component : furnaceStructure.components) {
      component.previewDescriptorSet.cleanup();
      component.standardDescriptorSet.cleanup();
    }

    // mineral mined cleanup
    for (auto &component : mineralMinedStructure.components) {
      component.previewDescriptorSet.cleanup();
      component.standardDescriptorSet.cleanup();
    }

    // metal ingot cleanup
    for (auto &component : metalIngotStructure.components) {
      component.previewDescriptorSet.cleanup();
      component.standardDescriptorSet.cleanup();
    }

    DSgrid.cleanup();
    DSglobal.cleanup();

    SC.pipelinesAndDescriptorSetsCleanup();
    txt.pipelinesAndDescriptorSetsCleanup();
  }

  // Here you destroy all the Models, Texture and Desc. Set Layouts you created!
  // You also have to destroy the pipelines
  void localCleanup() {
    DSLlocalChar.cleanup();
    DSLlocalSimp.cleanup();
    DSLlocalPBR.cleanup();
    DSLlocalPBRCoal.cleanup();
    DSLskyBox.cleanup();
    DSLglobal.cleanup();
    DSLgrid.cleanup();
    DSLwireframe.cleanup();

    Pchar.destroy();
    P_Ground.destroy();
    PskyBox.destroy();
    P_PBR.destroy();
    Pwireframe.destroy();
    Pgrid.destroy();
    P_PBRCoal.destroy();
    for (auto &component : minerStructure.components) {
      component.model.cleanup();
    }

    for (auto &component : conveyorStructure.components) {
      component.model.cleanup();
    }

    for (auto &component : furnaceStructure.components) {
      component.model.cleanup();
    }

    for (auto &component : mineralMinedStructure.components) {
      component.model.cleanup();
    }

    for (auto &component : metalIngotStructure.components) {
      component.model.cleanup();
    }

    RP.destroy();

    SC.localCleanup();
    txt.localCleanup();
  }

  // Here it is the creation of the command buffer:
  // You send to the GPU all the objects you want to draw,
  // with their buffers and textures
  static void populateCommandBufferAccess(VkCommandBuffer commandBuffer,
                                          int currentImage, void *Params) {
    // Simple trick to avoid having always 'T->'
    // in che code that populates the command buffer!
    // std::cout << "Populating command buffer for " << currentImage << "\n";
    Factotum *T = (Factotum *)Params;
    T->populateCommandBuffer(commandBuffer, currentImage);
  }
  // This is the real place where the Command Buffer is written
  void populateCommandBuffer(VkCommandBuffer commandBuffer, int currentImage) {

    // begin standard pass
    RP.begin(commandBuffer, currentImage);

    SC.populateCommandBuffer(commandBuffer, 0, currentImage);

    P_PBR.bind(commandBuffer);
    DSglobal.bind(commandBuffer, P_PBR, 0, currentImage);

    std::vector<std::shared_ptr<PlacedObject>> placedMiners, placedConveyors,
        placedFurnaces;
    for (const auto &obj : placedObjects) {
      if (obj->type == MINER) {
        placedMiners.push_back(obj);
      } else if (obj->type == CONVEYOR_BELT) {
        placedConveyors.push_back(obj);
      } else if (obj->type == FURNACE) {
        placedFurnaces.push_back(obj);
      }
    }

    for (auto &component : minerStructure.components) {
      component.model.bind(commandBuffer);
      component.standardDescriptorSet.bind(commandBuffer, P_PBR, 1,
                                           currentImage);
      vkCmdDrawIndexed(commandBuffer,
                       static_cast<uint32_t>(component.model.indices.size()),
                       placedMiners.size(), 0, 0, 0);
    }

    for (auto &component : conveyorStructure.components) {
      component.model.bind(commandBuffer);
      component.standardDescriptorSet.bind(commandBuffer, P_PBR, 1,
                                           currentImage);
      vkCmdDrawIndexed(commandBuffer,
                       static_cast<uint32_t>(component.model.indices.size()),
                       placedConveyors.size(), 0, 0, 0);
    }

    P_PBRCoal.bind(commandBuffer);
    DSglobal.bind(commandBuffer, P_PBRCoal, 0, currentImage);
    furnaceStructure.components[0].model.bind(commandBuffer);
    furnaceStructure.components[0].standardDescriptorSet.bind(
        commandBuffer, P_PBRCoal, 1, currentImage);
    vkCmdDrawIndexed(commandBuffer,
                     static_cast<uint32_t>(
                         furnaceStructure.components[0].model.indices.size()),
                     placedFurnaces.size(), 0, 0, 0);

    P_PBR.bind(commandBuffer);
    DSglobal.bind(commandBuffer, P_PBR, 0, currentImage);
    furnaceStructure.components[1].model.bind(commandBuffer);
    furnaceStructure.components[1].standardDescriptorSet.bind(
        commandBuffer, P_PBR, 1, currentImage);
    vkCmdDrawIndexed(commandBuffer,
                     static_cast<uint32_t>(
                         furnaceStructure.components[1].model.indices.size()),
                     placedFurnaces.size(), 0, 0, 0);

    furnaceStructure.components[2].model.bind(commandBuffer);
    furnaceStructure.components[2].standardDescriptorSet.bind(
        commandBuffer, P_PBR, 1, currentImage);
    vkCmdDrawIndexed(commandBuffer,
                     static_cast<uint32_t>(
                         furnaceStructure.components[2].model.indices.size()),
                     placedFurnaces.size(), 0, 0, 0);

    mineralMinedStructure.components[0].model.bind(commandBuffer);
    mineralMinedStructure.components[0].standardDescriptorSet.bind(
        commandBuffer, P_PBR, 1, currentImage);
    vkCmdDrawIndexed(
        commandBuffer,
        static_cast<uint32_t>(
            mineralMinedStructure.components[0].model.indices.size()),
        spawnedMinerals.size(), 0, 0, 0);

    metalIngotStructure.components[0].model.bind(commandBuffer);
    metalIngotStructure.components[0].standardDescriptorSet.bind(
        commandBuffer, P_PBR, 1, currentImage);
    vkCmdDrawIndexed(
        commandBuffer,
        static_cast<uint32_t>(
            metalIngotStructure.components[0].model.indices.size()),
        1, 0, 0, 0);

    if (isPlacing) {
      Structure *selectedStructure = &minerStructure;
      switch (inventoryItem) {
      case MINER:
        selectedStructure = &minerStructure;
        break;
      case CONVEYOR_BELT:
        selectedStructure = &conveyorStructure;
        break;
      case FURNACE:
        selectedStructure = &furnaceStructure;
        break;
      }

      auto color = isPlacementValid ? validColorWRF : invalidColorWRF;

      for (auto &component : selectedStructure->components) {
        Pwireframe.bind(commandBuffer);

        vkCmdPushConstants(commandBuffer, Pwireframe.pipelineLayout,
                           VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(glm::vec4),
                           &color);

        component.model.bind(commandBuffer);
        component.previewDescriptorSet.bind(commandBuffer, Pwireframe, 1,
                                            currentImage);
        vkCmdDrawIndexed(commandBuffer,
                         static_cast<uint32_t>(component.model.indices.size()),
                         1, 0, 0, 0);
      }

      // Render grid preview
      Pgrid.bind(commandBuffer);
      DSgrid.bind(commandBuffer, Pgrid, 0, currentImage);
      vkCmdDraw(commandBuffer, 6, 1, 0, 0);
    }

    RP.end(commandBuffer);
  }

  // Here is where you update the uniforms.
  // Very likely this will be where you will be writing the logic of your
  // application.
  void updateUniformBuffer(uint32_t currentImage) {
    static bool debounce = false;
    static int curDebounce = 0;

    // handle the ESC key to exit the app
    if (glfwGetKey(window, GLFW_KEY_ESCAPE)) {
      glfwSetWindowShouldClose(window, GL_TRUE);
    }

    if (glfwGetKey(window, GLFW_KEY_1)) {
      if (!debounce) {
        debounce = true;
        curDebounce = GLFW_KEY_1;

        inventoryItem = MINER;
        std::cout << "Selected MINER\n";
        if (isPlacing)
          submitCommandBuffer("main", 0, populateCommandBufferAccess, this);

        debug1.x = 1.0 - debug1.x;
      }
    } else {
      if ((curDebounce == GLFW_KEY_1) && debounce) {
        debounce = false;
        curDebounce = 0;
      }
    }

    if (glfwGetKey(window, GLFW_KEY_2)) {
      if (!debounce) {
        debounce = true;
        curDebounce = GLFW_KEY_2;

        inventoryItem = CONVEYOR_BELT;
        std::cout << "Selected CONVEYOR_BELT\n";
        if (isPlacing)
          submitCommandBuffer("main", 0, populateCommandBufferAccess, this);

        debug1.y = 1.0 - debug1.y;
      }
    } else {
      if ((curDebounce == GLFW_KEY_2) && debounce) {
        debounce = false;
        curDebounce = 0;
      }
    }

    if (glfwGetKey(window, GLFW_KEY_3)) {
      if (!debounce) {
        debounce = true;
        curDebounce = GLFW_KEY_3;

        inventoryItem = FURNACE;
        std::cout << "Selected FURNACE\n";
        if (isPlacing)
          submitCommandBuffer("main", 0, populateCommandBufferAccess, this);

        debug1.y = 1.0 - debug1.y;
      }
    } else {
      if ((curDebounce == GLFW_KEY_3) && debounce) {
        debounce = false;
        curDebounce = 0;
      }
    }

    if (glfwGetKey(window, GLFW_KEY_P)) {
      if (!debounce) {
        debounce = true;
        curDebounce = GLFW_KEY_P;

        debug1.z = (float)(((int)debug1.z + 1) % 65);
        std::cout << "Showing bone index: " << debug1.z << "\n";
      }
    } else {
      if ((curDebounce == GLFW_KEY_P) && debounce) {
        debounce = false;
        curDebounce = 0;
      }
    }

    if (glfwGetKey(window, GLFW_KEY_O)) {
      if (!debounce) {
        debounce = true;
        curDebounce = GLFW_KEY_O;

        debug1.z = (float)(((int)debug1.z + 64) % 65);
        std::cout << "Showing bone index: " << debug1.z << "\n";
      }
    } else {
      if ((curDebounce == GLFW_KEY_O) && debounce) {
        debounce = false;
        curDebounce = 0;
      }
    }

    if (glfwGetKey(window, GLFW_KEY_T) == GLFW_PRESS) {
      if (!debounce) {
        debounce = true;
        curDebounce = GLFW_KEY_T;
        isPlacing = !isPlacing;
        submitCommandBuffer("main", 0, populateCommandBufferAccess, this);
        std::cout << "Wireframe placement mode: " << (isPlacing ? "ON" : "OFF")
                  << std::endl;
        return;
      }
    } else {
      if ((curDebounce == GLFW_KEY_T) && debounce) {
        debounce = false;
        curDebounce = 0;
      }
    }

    if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
      if (!debounce) {
        debounce = true;
        curDebounce = GLFW_KEY_R;
        previewRotation += glm::radians(90.0f);
        if (previewRotation >= glm::radians(360.0f)) {
          previewRotation -= glm::radians(360.0f);
        }
        std::cout << "Preview rotation: " << glm::degrees(previewRotation)
                  << " degrees" << std::endl;
      }
    } else {
      if ((curDebounce == GLFW_KEY_R) && debounce) {
        debounce = false;
        curDebounce = 0;
      }
    }

    // moves the view
    float deltaT = GameLogic();

    // defines the global parameters for the uniform
    const glm::mat4 lightView = glm::rotate(glm::mat4(1), glm::radians(-30.0f),
                                            glm::vec3(0.0f, 1.0f, 0.0f)) *
                                glm::rotate(glm::mat4(1), glm::radians(-45.0f),
                                            glm::vec3(1.0f, 0.0f, 0.0f));
    const glm::vec3 lightDir =
        glm::vec3(lightView * glm::vec4(0.0f, -0.5f, 1.0f, 1.0f));

    GlobalUniformBufferObject gubo{};

    gubo.lightDir = lightDir;
    gubo.lightColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    gubo.eyePos = cameraPos;

    DSglobal.map(currentImage, &gubo, 0);

    // defines the local parameters for the uniforms
    UniformBufferObjectChar uboc{};
    uboc.debug1 = debug1;

    // printMat4("TF[55]", (*TMsp)[55]);

    glm::mat4 AdaptMat = glm::scale(glm::mat4(1.0f), glm::vec3(0.01f)) *
                         glm::rotate(glm::mat4(1.0f), glm::radians(90.0f),
                                     glm::vec3(1.0f, 0.0f, 0.0f));

    int instanceId;
    UniformBufferObjectSimp ubos{};

    // ground pipeline
    ubos.mMat[0] = SC.TI[1].I[0].Wm;
    ubos.mvpMat[0] = ViewPrj * ubos.mMat[0];
    ubos.nMat[0] = glm::inverse(glm::transpose(ubos.mMat[0]));
    SC.TI[1].I[0].DS[0][0]->map(currentImage, &gubo, 0); // Set 0
    SC.TI[1].I[0].DS[0][1]->map(currentImage, &ubos, 0); // Set 1

    // skybox pipeline
    skyBoxUniformBufferObject sbubo{};
    sbubo.mvpMat = ViewPrj * glm::translate(glm::mat4(1), cameraPos) *
                   glm::scale(glm::mat4(1), glm::vec3(100.0f));
    SC.TI[2].I[0].DS[0][0]->map(currentImage, &sbubo, 0);

    // grid pipeline
    GridUniformBufferObject gridUbo{};
    gridUbo.gVP = ViewPrj;
    gridUbo.camWorldPos = cameraPos;
    gridUbo.gridSize = gridSize;
    DSgrid.map(currentImage, &gridUbo, 0);

    // std::cout << "Test delle textures" <<SC.I[3]  << "\n";
    // PBR objects
    for (instanceId = 0; instanceId < SC.TI[3].InstanceCount; instanceId++) {
      ubos.mMat[0] = SC.TI[3].I[instanceId].Wm;
      ubos.mvpMat[0] = ViewPrj * ubos.mMat[0];
      ubos.nMat[0] = glm::inverse(glm::transpose(ubos.mMat[0]));

      SC.TI[3].I[instanceId].DS[0][0]->map(currentImage, &gubo, 0); // Set 0
      SC.TI[3].I[instanceId].DS[0][1]->map(currentImage, &ubos, 0); // Set 1
    }

    // PBR_coal objects
    for (instanceId = 0; instanceId < SC.TI[4].InstanceCount; instanceId++) {
      ubos.mMat[0] = SC.TI[4].I[instanceId].Wm;
      ubos.mvpMat[0] = ViewPrj * ubos.mMat[0];
      ubos.nMat[0] = glm::inverse(glm::transpose(ubos.mMat[0]));

      SC.TI[4].I[instanceId].DS[0][0]->map(currentImage, &gubo, 0); // Set 0
      SC.TI[4].I[instanceId].DS[0][1]->map(currentImage, &ubos, 0); // Set 1
    }

    std::vector<std::shared_ptr<PlacedMiner>> placedMiners;
    std::vector<std::shared_ptr<PlacedObject>> placedConveyors;
    std::vector<std::shared_ptr<PlacedFurnace>> placedFurnaces;
    for (const auto &object : placedObjects) {
      if (object->type == MINER) {
        placedMiners.push_back(std::dynamic_pointer_cast<PlacedMiner>(object));
      } else if (object->type == CONVEYOR_BELT) {
        placedConveyors.push_back(object);
      } else if (object->type == FURNACE) {
        placedFurnaces.push_back(
            std::dynamic_pointer_cast<PlacedFurnace>(object));
      }
    }

    for (auto &component : minerStructure.components) {
      for (int i = 0; i < placedMiners.size(); i++) {
        ubos.mMat[i] =
            glm::translate(glm::mat4(1.f), placedMiners[i]->position) *
            glm::rotate(glm::mat4(1.0f), placedMiners[i]->rotation,
                        glm::vec3(0.0f, 1.0f, 0.0f)) *
            component.model.Wm;
        ubos.mvpMat[i] = ViewPrj * ubos.mMat[i];
        ubos.nMat[i] = glm::inverse(glm::transpose(ubos.mMat[i]));
      }

      component.standardDescriptorSet.map(currentImage, &ubos, 0);
    }

    for (auto &component : conveyorStructure.components) {
      for (int i = 0; i < placedConveyors.size(); i++) {
        ubos.mMat[i] =
            glm::translate(glm::mat4(1.f), placedConveyors[i]->position) *
            glm::rotate(glm::mat4(1.0f), placedConveyors[i]->rotation,
                        glm::vec3(0.0f, 1.0f, 0.0f)) *
            component.model.Wm;
        ubos.mvpMat[i] = ViewPrj * ubos.mMat[i];
        ubos.nMat[i] = glm::inverse(glm::transpose(ubos.mMat[i]));
      }

      component.standardDescriptorSet.map(currentImage, &ubos, 0);
    }

    for (auto &component : furnaceStructure.components) {
      for (int i = 0; i < placedFurnaces.size(); i++) {
        ubos.mMat[i] =
            glm::translate(glm::mat4(1.f), placedFurnaces[i]->position) *
            glm::rotate(glm::mat4(1.0f), placedFurnaces[i]->rotation,
                        glm::vec3(0.0f, 1.0f, 0.0f)) *
            component.model.Wm;
        ubos.mvpMat[i] = ViewPrj * ubos.mMat[i];
        ubos.nMat[i] = glm::inverse(glm::transpose(ubos.mMat[i]));
      }

      component.standardDescriptorSet.map(currentImage, &ubos, 0);
    }

    for (int i = 0; i < spawnedMinerals.size(); i++) {
      if (i >= 100)
        break;
      ubos.mMat[i] =
          glm::translate(glm::mat4(1.0f), spawnedMinerals[i].position) *
          mineralMinedStructure.components[0].model.Wm;
      ubos.mvpMat[i] = ViewPrj * ubos.mMat[i];
      ubos.nMat[i] = glm::inverse(glm::transpose(ubos.mMat[i]));
    }
    mineralMinedStructure.components[0].standardDescriptorSet.map(currentImage,
                                                                  &ubos, 0);

    for (auto &component : metalIngotStructure.components) {
      ubos.mMat[0] =
          glm::translate(glm::mat4(1.0f), glm::vec3(5.0f, 0.0f, 0.0f)) *
          glm::scale(glm::vec3(8.f)) *
          metalIngotStructure.components[0].model.Wm;
      ubos.mvpMat[0] = ViewPrj * ubos.mMat[0];
      ubos.nMat[0] = glm::inverse(glm::transpose(ubos.mMat[0]));

      metalIngotStructure.components[0].standardDescriptorSet.map(currentImage,
                                                                  &ubos, 0);
    }

    float currentTime = glfwGetTime();
    for (auto &miner : placedMiners) {
      if (miner->type == MINER) {
        if (currentTime - miner->lastSpawnTime > 5.0f) {
          glm::vec3 centerLeftPos =
              getMinerCenterLeftBlock(miner->position, miner->rotation);

          bool conveyorFound = false;
          for (const auto &obj : placedObjects) {
            if (obj->type == CONVEYOR_BELT && obj->position == centerLeftPos) {
              conveyorFound = true;
              break;
            }
          }

          if (conveyorFound) {
            glm::vec2 forward = getForwardVector(miner->rotation);
            glm::vec3 direction = glm::vec3(-forward.y, 0, forward.x);
            glm::vec3 spawnPos = centerLeftPos - direction * gridSize;

            SpawnedMineral newMineral;
            newMineral.initialPosition = spawnPos;
            newMineral.position = spawnPos;
            newMineral.direction = direction;
            newMineral.spawnTime = currentTime;

            spawnedMinerals.push_back(newMineral);
            miner->lastSpawnTime = currentTime;

            submitCommandBuffer("main", 0, populateCommandBufferAccess, this);
          }
        }
      }
    }

    float mineralSpeed = 1.0f;
    for (int i = 0; i < spawnedMinerals.size(); i++) {
      auto &mineral = spawnedMinerals[i];
      mineral.position =
          mineral.position + mineral.direction * deltaT * mineralSpeed;

      bool valid = false;

      for (auto &cb : placedConveyors) {
        // auto forward = getForwardVector(cb.rotation);
        // auto direction = glm::vec3(-forward.y, 0, forward.x);
        // std::cout << "Changed direction " << direction.x << ' ' <<
        // direction.y
        //           << ' ' << direction.z << " distance "
        //           << glm::distance(mineral.position, cb.position)
        //           << " other distance "
        //           << glm::distance(mineral.position + mineral.direction,
        //                            cb.position)
        //           << "\n";
        if (glm::distance(mineral.position, cb->position) <= gridSize / 6.0f) {
          auto forward = getForwardVector(cb->rotation);
          auto direction = glm::vec3(-forward.y, 0, forward.x);
          mineral.direction = direction;
          valid = true;
          break;
        } else if (glm::distance(mineral.position +
                                     mineral.direction * deltaT * mineralSpeed,
                                 cb->position) <= gridSize) {
          valid = true;
          break;
        }
      }

      for (auto &furnace : placedFurnaces) {
        if (valid) {
          auto positions =
              getFurnaceOccupiedBlocks(furnace->position, furnace->rotation);
          for (auto &pos : positions) {
            if (glm::distance(pos, mineral.position) <= gridSize / 6.f) {
              std::cout << "new stuff" << furnace->ore.size() << "\n";
              furnace->ore.push_back(currentTime);
              valid = false;
              break;
            }
          }
        }
      }

      if (!valid) {
        spawnedMinerals.erase(spawnedMinerals.begin() + i);
        submitCommandBuffer("main", 0, populateCommandBufferAccess, this);
      }
    }

    if (isPlacing) {
      glm::vec3 placementPos = calculateGroundPlacementPosition(
          cameraPos, getLookingVector(), gridSize);

      bool prevState = isPlacementValid;
      isPlacementValid = true;

      // Check for existing placed objects
      for (auto &pos : placedObjects) {
        std::vector<glm::vec3> positions;
        switch (pos->type) {
        case MINER:
          positions = getMinerOccupiedBlocks(pos->position, pos->rotation);
          break;
        case FURNACE:
          positions = getFurnaceOccupiedBlocks(pos->position, pos->rotation);
          break;
        case CONVEYOR_BELT:
          positions = {pos->position};
        }

        std::vector<glm::vec3> placingPositions;
        switch (inventoryItem) {
        case MINER:
          placingPositions =
              getMinerOccupiedBlocks(placementPos, previewRotation);
          break;
        case FURNACE:
          placingPositions =
              getFurnaceOccupiedBlocks(placementPos, previewRotation);
          break;
        case CONVEYOR_BELT:
          placingPositions = {placementPos};
        }

        for (auto &minPos : positions) {
          if (isPlacementValid) {
            for (auto &placPos : placingPositions) {
              if (minPos == placPos) {
                isPlacementValid = false;
                break;
              }
            }
          }
        }
      }

      // Additional check for MINER placement
      if (isPlacementValid && inventoryItem == MINER) {
        Mineral *mineralAtPos = getMineralAtPosition(placementPos);
        if (mineralAtPos == nullptr || mineralAtPos->isBeingMined) {
          isPlacementValid = false;
        }
      } else if (isPlacementValid && inventoryItem != MINER) {
        // Prevent placing other structures on top of minerals
        Mineral *mineralAtPos = getMineralAtPosition(placementPos);
        if (mineralAtPos != nullptr) {
          isPlacementValid = false;
        }
      }

      if (prevState != isPlacementValid) {
        submitCommandBuffer("main", 0, populateCommandBufferAccess, this);
      }

      UniformBufferObjectSimp ubosComponent{};
      previewTransform =
          glm::translate(glm::mat4(1.0f),
                         calculateGroundPlacementPosition(
                             cameraPos, getLookingVector(), gridSize)) *
          glm::rotate(glm::mat4(1.0f), previewRotation,
                      glm::vec3(0.0f, 1.0f, 0.0f));

      Structure *selectedStructure = nullptr;

      switch (inventoryItem) {
      case MINER:
        selectedStructure = &minerStructure;
        break;
      case CONVEYOR_BELT:
        selectedStructure = &conveyorStructure;
        break;
      case FURNACE:
        selectedStructure = &furnaceStructure;
        break;
      }

      for (auto &component : selectedStructure->components) {
        ubosComponent.mMat[0] = previewTransform * component.model.Wm;
        ubosComponent.mvpMat[0] = ViewPrj * ubosComponent.mMat[0];
        ubosComponent.nMat[0] =
            glm::inverse(glm::transpose(ubosComponent.mMat[0]));

        component.previewDescriptorSet.map(currentImage, &ubosComponent, 0);
      }
    }

    // Add labels for placed objects
    for (int i = 0; i < placedObjects.size(); i++) {
      auto& obj = placedObjects[i];
      if (obj->type == MINER || obj->type == FURNACE) {
          glm::vec3 pos = obj->position;
          glm::vec4 clipPos = ViewPrj * glm::vec4(pos + glm::vec3(0.0f, 1.0f, 0.0f), 1.0);
          glm::vec3 ndcPos = glm::vec3(clipPos) / clipPos.w;

          if (ndcPos.z < 1.0f && isPlacing) {
              std::string label = (obj->type == MINER) ? "Miner" : "Furnace";
              txt.print(ndcPos.x, ndcPos.y, label, 100 + obj->id, "CO", false, false, true, TAL_CENTER, TRH_CENTER, TRV_BOTTOM, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f, 1.0f});
          } else {
              txt.print(0.0f, -2.0f, "", 100 + obj->id, "CO", false, false, true, TAL_CENTER, TRH_CENTER, TRV_BOTTOM, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f, 1.0f});
          }
      }
    }

    // display inventory information
    std::stringstream inventory_str;
    inventory_str << "Selected item: ";

    switch (inventoryItem) {
    case MINER:
      inventory_str << "Miner";
      break;
    case CONVEYOR_BELT:
      inventory_str << "Conveyor Belt";
      break;
    case FURNACE:
      inventory_str << "Furnace";
      break;
    default:
      inventory_str << "Unrecognized Item";
      break;
    }

    txt.print(-1.0f, -1.0f, inventory_str.str(), 3, "CO", false, false, false,
              TAL_LEFT, TRH_LEFT, TRV_TOP, {1.0f, 1.0f, 1.0f, 1.0f},
              {0.0f, 0.0f, 0.0f, 1.0f});

    // ROCKET LABEL
    glm::vec3 rocketPos = glm::vec3(SC.TI[3].I[4].Wm[3]);
    glm::vec4 clipPos = ViewPrj * glm::vec4(rocketPos, 1.0);
    glm::vec3 ndcPos = glm::vec3(clipPos) / clipPos.w;
    if (ndcPos.z < 1.0f) {
        txt.print(ndcPos.x, ndcPos.y, "Rocket", 10, "CO", false, false, true, TAL_CENTER, TRH_CENTER, TRV_BOTTOM, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f, 1.0f});
    } else {
        txt.print(0.0f, -2.0f, "Rocket", 10, "CO", false, false, true, TAL_CENTER, TRH_CENTER, TRV_BOTTOM, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f, 1.0f});
    }

    // updates the FPS
    static float elapsedT = 0.0f;
    static int countedFrames = 0;

    countedFrames++;
    elapsedT += deltaT;
    if (elapsedT > 1.0f) {
      float Fps = (float)countedFrames / elapsedT;

      std::ostringstream oss;
      oss << "FPS: " << Fps << "\n";

      txt.print(1.0f, 1.0f, oss.str(), 1, "CO", false, false, true, TAL_RIGHT,
                TRH_RIGHT, TRV_BOTTOM, {1.0f, 0.0f, 0.0f, 1.0f},
                {0.8f, 0.8f, 0.0f, 1.0f});

      elapsedT = 0.0f;
      countedFrames = 0;
    }

    txt.updateCommandBuffer();
  }

  glm::vec2 getForwardVector(float rotationRadians) {
    glm::vec2 forward;
    float rotationDegrees = glm::degrees(rotationRadians);
    rotationDegrees = fmod(rotationDegrees, 360.0f);
    if (rotationDegrees < 0)
      rotationDegrees += 360.0f;

    if (abs(rotationDegrees - 0.0) < 1.0 ||
        abs(rotationDegrees - 360.0) < 1.0) {
      forward = glm::vec2(1, 0);
    } else if (abs(rotationDegrees - 90.0) < 1.0) {
      forward = glm::vec2(0, -1);
    } else if (abs(rotationDegrees - 180.0) < 1.0) {
      forward = glm::vec2(-1, 0);
    } else if (abs(rotationDegrees - 270.0) < 1.0) {
      forward = glm::vec2(0, 1);
    } else {
      forward = glm::vec2(0, 1);
    }
    return forward;
  }

  std::vector<glm::vec3> getMinerOccupiedBlocks(glm::vec3 position,
                                                float rotationRadians) {
    std::vector<glm::vec3> occupiedBlocks;
    glm::vec2 forward = getForwardVector(rotationRadians);

    // 3-block line
    for (int i = 0; i < 3; ++i) {
      occupiedBlocks.push_back(
          glm::vec3(position.x - i * forward.x * gridSize, 0,
                    position.z - i * forward.y * gridSize));
    }

    // 5x5 square
    for (int i = 3; i < 8; ++i) {
      for (int j = -2; j <= 2; ++j) {
        occupiedBlocks.push_back(glm::vec3(
            position.x - i * forward.x * gridSize + j * forward.y * gridSize, 0,
            position.z - i * forward.y * gridSize + j * forward.x * gridSize));
      }
    }

    return occupiedBlocks;
  }

  std::vector<glm::vec3> getFurnaceOccupiedBlocks(glm::vec3 position,
                                                  float rotationRadians) {
    std::vector<glm::vec3> occupiedBlocks;
    glm::vec2 forward = getForwardVector(rotationRadians);

    occupiedBlocks.push_back(position);

    glm::vec2 nextBlockPos =
        glm::vec2(position.x, position.z) - forward * gridSize;
    occupiedBlocks.push_back(glm::vec3(nextBlockPos.x, 0, nextBlockPos.y));

    return occupiedBlocks;
  }

  glm::vec3 getMinerCenterLeftBlock(glm::vec3 position, float rotationRadians) {
    glm::vec2 forward = getForwardVector(rotationRadians);

    float x =
        position.x - 5.0f * forward.x * gridSize - 3.0f * forward.y * gridSize;
    float z =
        position.z - 5.0f * forward.y * gridSize + 3.0f * forward.x * gridSize;

    return glm::vec3(x, 0, z);
  }

  glm::vec3 getLookingVector() {
    glm::vec3 front;
    front.x = cos(glm::radians(Yaw)) * cos(glm::radians(Pitch));
    front.y = sin(glm::radians(Pitch));
    front.z = sin(glm::radians(Yaw)) * cos(glm::radians(Pitch));
    front = glm::normalize(front);

    return front;
  }

  glm::vec3 calculateGroundPlacementPosition(glm::vec3 cameraPos,
                                             glm::vec3 lookingVector,
                                             float gridSize) {
    float t = -cameraPos.y / lookingVector.y;
    if (t > 0) {
      glm::vec3 intersection = cameraPos + t * lookingVector;
      float x = round(intersection.x / gridSize) * gridSize;
      float z = round(intersection.z / gridSize) * gridSize;
      return glm::vec3(x, 0.0f, z);
    }
    return glm::vec3(
        0.0f); // Return a default position if no valid intersection
  }

  float GameLogic() {
    //-Timing
    float currentFrame = glfwGetTime();
    float deltaT = currentFrame - lastFrame;
    lastFrame = currentFrame;

    // Parameters
    // Player starting point
    const glm::vec3 StartingPosition = glm::vec3(0.0, 0.0, 5);
    // Rotation and motion speed
    const float MOVE_SPEED_BASE = 2.0f;
    const float MOVE_SPEED_RUN = 10.0f;

    // bool fire = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS;
    float MOVE_SPEED = MOVE_SPEED_RUN;
    auto front = getLookingVector();

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
      Pos += MOVE_SPEED * deltaT * front;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
      Pos -= MOVE_SPEED * deltaT * front;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
      Pos -= glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f))) *
             MOVE_SPEED * deltaT;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
      Pos += glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f))) *
             MOVE_SPEED * deltaT;

    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
      Pos.y += MOVE_SPEED * deltaT;
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
      Pos.y -= MOVE_SPEED * deltaT;

    //    std::cout << "Position: " << Pos.x << ", " << Pos.y << ", " << Pos.z
    //    << "\n";

    // Game Logic implementation
    // Current Player Position - statc variable make sure its value remain
    // unchanged in subsequent calls to the procedure
    static int currRunState = 1;

    // std::cout << "Current position: " << Pos.x << "," << Pos.y << "," <<
    // Pos.z << "\n"; To be done in the assignment
    ViewPrj = glm::mat4(1);
    World = glm::mat4(1);

    static float relDir = glm::radians(0.0f);
    static float dampedRelDir = glm::radians(0.0f);
    static glm::vec3 dampedCamPos = StartingPosition;

    // World
    // Position
    glm::vec3 ux = glm::rotate(glm::mat4(1.0f), Yaw, glm::vec3(0, 1, 0)) *
                   glm::vec4(1, 0, 0, 1);
    glm::vec3 uz = glm::rotate(glm::mat4(1.0f), Yaw, glm::vec3(0, 1, 0)) *
                   glm::vec4(0, 0, -1, 1);

    float ef = exp(-10.0 * deltaT);

    // Final world matrix computaiton
    World = glm::translate(glm::mat4(1), Pos) *
            glm::rotate(glm::mat4(1.0f), dampedRelDir, glm::vec3(0, 1, 0));

    // Projection
    glm::mat4 Prj = getPrj();
    // View
    cameraPos = Pos;
    glm::mat4 View =
        glm::lookAt(cameraPos, cameraPos + front, glm::vec3(0.0f, 1.0f, 0.0f));

    ViewPrj = Prj * View;

    return deltaT;
  }

  glm::mat4 getPrj() {
    const float FOVy = glm::radians(45.0f);
    const float nearPlane = 0.1f;
    const float farPlane = 100.f;
    glm::mat4 Prj = glm::perspective(FOVy, Ar, nearPlane, farPlane);
    Prj[1][1] *= -1;
    return Prj;
  }

  glm::mat4 getView() {
    glm::vec3 front;
    front.x = cos(glm::radians(Yaw)) * cos(glm::radians(Pitch));
    front.y = sin(glm::radians(Pitch));
    front.z = sin(glm::radians(Yaw)) * cos(glm::radians(Pitch));
    front = glm::normalize(front);
    return glm::lookAt(cameraPos, cameraPos + front,
                       glm::vec3(0.0f, 1.0f, 0.0f));
  }

  static void mouse_callback(GLFWwindow *window, double xpos, double ypos) {
    Factotum *app = (Factotum *)glfwGetWindowUserPointer(window);
    if (app->firstMouse) {
      app->lastX = xpos;
      app->lastY = ypos;
      app->firstMouse = false;
    }

    float xoffset = xpos - app->lastX;
    float yoffset = app->lastY - ypos;
    app->lastX = xpos;
    app->lastY = ypos;

    float sensitivity = 0.05f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    app->Yaw += xoffset;
    app->Pitch += yoffset;

    if (app->Pitch > 89.0f)
      app->Pitch = 89.0f;
    if (app->Pitch < -89.0f)
      app->Pitch = -89.0f;
  }

  Mineral *getMineralAtPosition(glm::vec3 position) {
    for (auto &mineral : minerals) {
      if (mineral.position == position) {
        return &mineral;
      }
    }
    return nullptr;
  }

  static void mouse_button_callback(GLFWwindow *window, int button, int action,
                                    int mods) {
    Factotum *app = (Factotum *)glfwGetWindowUserPointer(window);
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS &&
        app->isPlacing) {
      glm::vec3 newPos = app->calculateGroundPlacementPosition(
          app->cameraPos, app->getLookingVector(), app->gridSize);

      bool canPlace = app->isPlacementValid; // Initial check for occupied slot

      if (app->inventoryItem == MINER && canPlace) {
        Mineral *mineralAtPos = app->getMineralAtPosition(newPos);
        if (mineralAtPos != nullptr && !mineralAtPos->isBeingMined) {
          // Can place miner, and it will start mining this mineral
          canPlace = true;
          mineralAtPos->isBeingMined = true;
          std::cout << "Miner placed and started mining at " << newPos.x << ","
                    << newPos.y << "," << newPos.z << std::endl;
        } else {
          // Cannot place miner if no mineral or mineral already being mined
          canPlace = false;
          if (mineralAtPos == nullptr) {
            std::cout << "Cannot place miner: no mineral at this position."
                      << std::endl;
          } else {
            std::cout << "Cannot place miner: mineral already being mined."
                      << std::endl;
          }
        }
      }

      if (canPlace) {
        switch (app->inventoryItem) {
        case MINER: {
          PlacedMiner newPlacedObject;
          newPlacedObject.id = app->nextPlacedObjectId++;
          newPlacedObject.type = app->inventoryItem;
          newPlacedObject.position = newPos;
          newPlacedObject.rotation = app->previewRotation;
          app->placedObjects.push_back(
              std::make_shared<PlacedMiner>(newPlacedObject));
        } break;
        case FURNACE: {
          PlacedFurnace newPlacedObject;
          newPlacedObject.id = app->nextPlacedObjectId++;
          newPlacedObject.type = app->inventoryItem;
          newPlacedObject.position = newPos;
          newPlacedObject.rotation = app->previewRotation;
          app->placedObjects.push_back(
              std::make_shared<PlacedFurnace>(newPlacedObject));
        } break;
        case CONVEYOR_BELT: {
          PlacedConveyor newPlacedObject;
          newPlacedObject.id = app->nextPlacedObjectId++;
          newPlacedObject.type = app->inventoryItem;
          newPlacedObject.position = newPos;
          newPlacedObject.rotation = app->previewRotation;
          app->placedObjects.push_back(
              std::make_shared<PlacedConveyor>(newPlacedObject));
        } break;
        }
        app->submitCommandBuffer("main", 0, populateCommandBufferAccess, app);
      } else {
        std::cout << "Cannot place object here: position is invalid."
                  << std::endl;
      }
    } else if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS &&
               app->isPlacing) {
      glm::vec3 removalPos = app->calculateGroundPlacementPosition(
          app->cameraPos, app->getLookingVector(), app->gridSize);

      // Find and remove the object at removalPos
      bool removed = false;
      for (auto it = app->placedObjects.begin(); it != app->placedObjects.end();
           ++it) {
        if ((*it)->position == removalPos) {
          // If removing a miner, set the mineral's isBeingMined to false
          if ((*it)->type == MINER) {
            Mineral *mineralAtPos = app->getMineralAtPosition(removalPos);
            if (mineralAtPos != nullptr) {
              mineralAtPos->isBeingMined = false;
              std::cout << "Miner removed, mineral at " << removalPos.x << ","
                        << removalPos.y << "," << removalPos.z
                        << " is no longer being mined." << std::endl;
            }
          }
          std::cout << "Removing object of type " << (*it)->type
                    << " at position: " << (*it)->position.x << ","
                    << (*it)->position.y << "," << (*it)->position.z
                    << std::endl;
          app->txt.print(0.0f, -2.0f, "", 100 + (*it)->id, "CO", false, false, true, TAL_CENTER, TRH_CENTER, TRV_BOTTOM, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f, 1.0f});
          app->placedObjects.erase(it);
          removed = true;
          app->submitCommandBuffer("main", 0, populateCommandBufferAccess, app);
          break; // Assuming only one object can be at a given position
        }
      }
      if (!removed) {
        std::cout << "No object found at position: " << removalPos.x << ","
                  << removalPos.y << "," << removalPos.z << " to remove."
                  << std::endl;
      }
    }
  }
};

// This is the main: probably you do not need to touch this!
int main() {
  Factotum app;

  try {
    app.run();
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
