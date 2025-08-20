// This has been adapted from the Vulkan tutorial
#include <cstdlib>
#include <glm/fwd.hpp>
#include <iostream>
#include <sstream>

#include <json.hpp>
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

class Factotum : public BaseProject {
protected:
  // Here you list all the Vulkan objects you need:

  // Descriptor Layouts [what will be passed to the shaders]
  DescriptorSetLayout DSLlocalChar, DSLlocalSimp, DSLlocalPBR, DSLglobal,
      DSLskyBox;

  // Vertex formants, Pipelines [Shader couples] and Render passes
  VertexDescriptor VDchar;
  VertexDescriptor VDsimp;
  VertexDescriptor VDskyBox;
  VertexDescriptor VDtan;
  RenderPass RP;
  Pipeline Pchar, PsimpObj, PskyBox, P_PBR, Pwireframe;
  //*DBG*/Pipeline PDebug;

  // Models, textures and Descriptors (values assigned to the uniforms)
  Scene SC;
  std::vector<VertexDescriptorRef> VDRs;
  std::vector<TechniqueRef> PRs;

  Structure minerStructure;
  bool isPlacingDrill = false;
  glm::mat4 previewTransform;
  float previewRotation = 0.0f;
  //*DBG*/Model MS;
  //*DBG*/DescriptorSet SSD;

  // to provide textual feedback
  TextMaker txt;

  // Other application parameters
  float Ar; // Aspect ratio

  glm::mat4 ViewPrj;
  glm::mat4 World;
  glm::vec3 Pos = glm::vec3(0, 0, 5);
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

  struct PlacedMiner {
    glm::vec3 position;
    float rotation;
  };
  std::vector<PlacedMiner> placedMiners;

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

    VDRs.resize(4);
    VDRs[0].init("VDchar", &VDchar);
    VDRs[1].init("VDsimp", &VDsimp);
    VDRs[2].init("VDskybox", &VDskyBox);
    VDRs[3].init("VDtan", &VDtan);

    // initializes the render passes
    RP.init(this);
    // sets the blue sky
    RP.properties[0].clearValue = {0.0f, 0.9f, 1.0f, 1.0f};

    // Pipelines [Shader couples]
    // The last array, is a vector of pointer to the layouts of the sets that
    // will be used in this pipeline. The first element will be set 0, and so
    // on..
    Pchar.init(this, &VDchar, "shaders/PosNormUvTanWeights.vert.spv",
               "shaders/CookTorranceForCharacter.frag.spv",
               {&DSLglobal, &DSLlocalChar});

    PsimpObj.init(this, &VDsimp, "shaders/SimplePosNormUV.vert.spv",
                  "shaders/CookTorrance.frag.spv", {&DSLglobal, &DSLlocalSimp});

    PskyBox.init(this, &VDskyBox, "shaders/SkyBoxShader.vert.spv",
                 "shaders/SkyBoxShader.frag.spv", {&DSLskyBox});
    PskyBox.setCompareOp(VK_COMPARE_OP_LESS_OR_EQUAL);
    PskyBox.setCullMode(VK_CULL_MODE_BACK_BIT);
    PskyBox.setPolygonMode(VK_POLYGON_MODE_FILL);

    P_PBR.init(this, &VDtan, "shaders/SimplePosNormUvTan.vert.spv",
               "shaders/PBR.frag.spv", {&DSLglobal, &DSLlocalPBR});

    // Initialize the wireframe pipeline
    Pwireframe.init(this, &VDtan, "shaders/SimplePosNormUvTan.vert.spv",
                    "shaders/PBR.frag.spv", {&DSLglobal, &DSLlocalPBR});
    Pwireframe.setPolygonMode(VK_POLYGON_MODE_LINE);

    minerStructure.components.resize(4);

    AssetFile assetMiner;
    assetMiner.init("assets/models/Miner.gltf", GLTF);
    minerStructure.components[0].model.initFromAsset(this, &VDtan, &assetMiner,
                                                     "Cube", 0, "Cube");
    minerStructure.components[1].model.initFromAsset(this, &VDtan, &assetMiner,
                                                     "Cone", 0, "Cone");
    minerStructure.components[2].model.initFromAsset(this, &VDtan, &assetMiner,
                                                     "Cylinder", 0, "Cylinder");
    minerStructure.components[3].model.initFromAsset(this, &VDtan, &assetMiner,
                                                     "Pyramid", 0, "Pyramid");

    PRs.resize(4);
    PRs[0].init("CookTorranceChar",
                {{&Pchar,
                  {// Pipeline and DSL for the first pass
                   /*DSLglobal*/ {},
                   /*DSLlocalChar*/ {
                       /*t0*/ {true, 0, {}} // index 0 of the "texture" field in
                                            // the json file
                   }}}},
                /*TotalNtextures*/ 1, &VDchar);
    PRs[1].init("CookTorranceNoiseSimp",
                {{&PsimpObj,
                  {// Pipeline and DSL for the first pass
                   /*DSLglobal*/ {},
                   /*DSLlocalSimp*/ {
                       /*t0*/ {true, 0, {}}, // index 0 of the "texture" field
                                             // in the json file
                       /*t1*/ {true, 1, {}} // index 1 of the "texture" field in
                                            // the json file
                   }}}},
                /*TotalNtextures*/ 2, &VDsimp);
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

    // Models, textures and Descriptors (values assigned to the uniforms)

    // sets the size of the Descriptor Set Pool
    DPSZs.uniformBlocksInPool = 3;
    DPSZs.texturesInPool = 4;
    DPSZs.setsInPool = 7;

    std::cout << "\nLoading the scene\n\n";
    if (SC.init(this, /*Npasses*/ 1, VDRs, PRs, "assets/models/scene.json") !=
        0) {
      std::cout << "ERROR LOADING THE SCENE\n";
      exit(0);
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
  }

  // Here you create your pipelines and Descriptor Sets!
  void pipelinesAndDescriptorSetsInit() {
    // creates the render pass
    RP.create();

    // This creates a new pipeline (with the current surface), using its shaders
    // for the provided render pass
    Pchar.create(&RP);
    PsimpObj.create(&RP);
    PskyBox.create(&RP);
    P_PBR.create(&RP);
    Pwireframe.create(&RP);

    for (auto &component : minerStructure.components) {
      component.previewDescriptorSet.init(
          this, &DSLlocalPBR,
          {SC.T[0]->getViewAndSampler(), SC.T[1]->getViewAndSampler(),
           SC.T[3]->getViewAndSampler(), SC.T[2]->getViewAndSampler()});

      component.standardDescriptorSet.init(
          this, &DSLlocalPBR,
          {SC.T[0]->getViewAndSampler(), SC.T[1]->getViewAndSampler(),
           SC.T[3]->getViewAndSampler(), SC.T[2]->getViewAndSampler()});
    }

    SC.pipelinesAndDescriptorSetsInit();
    txt.pipelinesAndDescriptorSetsInit();
  }

  // Here you destroy your pipelines and Descriptor Sets!
  void pipelinesAndDescriptorSetsCleanup() {
    Pchar.cleanup();
    PsimpObj.cleanup();
    PskyBox.cleanup();
    P_PBR.cleanup();
    Pwireframe.cleanup();
    RP.cleanup();

    // Cleanup descriptor sets for each component in minerStructurePreview
    for (auto &component : minerStructure.components) {
      component.previewDescriptorSet.cleanup();
      component.standardDescriptorSet.cleanup();
    }

    SC.pipelinesAndDescriptorSetsCleanup();
    txt.pipelinesAndDescriptorSetsCleanup();
  }

  // Here you destroy all the Models, Texture and Desc. Set Layouts you created!
  // You also have to destroy the pipelines
  void localCleanup() {
    DSLlocalChar.cleanup();
    DSLlocalSimp.cleanup();
    DSLlocalPBR.cleanup();
    DSLskyBox.cleanup();
    DSLglobal.cleanup();

    Pchar.destroy();
    PsimpObj.destroy();
    PskyBox.destroy();
    P_PBR.destroy();
    Pwireframe.destroy();
    for (auto &component : minerStructure.components) {
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
    for (auto &component : minerStructure.components) {

      component.model.bind(commandBuffer);
      component.standardDescriptorSet.bind(commandBuffer, P_PBR, 1,
                                           currentImage);
      vkCmdDrawIndexed(commandBuffer,
                       static_cast<uint32_t>(component.model.indices.size()),
                       placedMiners.size(), 0, 0, 0);
    }

    if (isPlacingDrill) {
      for (auto &component : minerStructure.components) {
        Pwireframe.bind(commandBuffer);
        component.model.bind(commandBuffer);
        component.previewDescriptorSet.bind(commandBuffer, Pwireframe, 1,
                                            currentImage);
        vkCmdDrawIndexed(commandBuffer,
                         static_cast<uint32_t>(component.model.indices.size()),
                         1, 0, 0, 0);
      }
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

        debug1.y = 1.0 - debug1.y;
      }
    } else {
      if ((curDebounce == GLFW_KEY_2) && debounce) {
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
        isPlacingDrill = !isPlacingDrill;
        submitCommandBuffer("main", 0, populateCommandBufferAccess, this);
        std::cout << "Wireframe placement mode: "
                  << (isPlacingDrill ? "ON" : "OFF") << std::endl;
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
        std::cout << "Preview rotation: " << glm::degrees(previewRotation) << " degrees" << std::endl;
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

    // defines the local parameters for the uniforms
    UniformBufferObjectChar uboc{};
    uboc.debug1 = debug1;

    // printMat4("TF[55]", (*TMsp)[55]);

    glm::mat4 AdaptMat = glm::scale(glm::mat4(1.0f), glm::vec3(0.01f)) *
                         glm::rotate(glm::mat4(1.0f), glm::radians(90.0f),
                                     glm::vec3(1.0f, 0.0f, 0.0f));

    int instanceId;
    UniformBufferObjectSimp ubos{};

    // skybox pipeline
    skyBoxUniformBufferObject sbubo{};
    sbubo.mvpMat = ViewPrj * glm::translate(glm::mat4(1), cameraPos) *
                   glm::scale(glm::mat4(1), glm::vec3(100.0f));
    SC.TI[2].I[0].DS[0][0]->map(currentImage, &sbubo, 0);

    // std::cout << "Test delle textures" <<SC.I[3]  << "\n";
    // PBR objects
    for (instanceId = 0; instanceId < SC.TI[3].InstanceCount; instanceId++) {
      ubos.mMat[0] = SC.TI[3].I[instanceId].Wm;
      ubos.mvpMat[0] = ViewPrj * ubos.mMat[0];
      ubos.nMat[0] = glm::inverse(glm::transpose(ubos.mMat[0]));

      SC.TI[3].I[instanceId].DS[0][0]->map(currentImage, &gubo, 0); // Set 0
      SC.TI[3].I[instanceId].DS[0][1]->map(currentImage, &ubos, 0); // Set 1
    }

    for (auto &component : minerStructure.components) {
      for (int i = 0; i < placedMiners.size(); i++) {
        ubos.mMat[i] = glm::translate(glm::mat4(1.f), placedMiners[i].position) *
                       glm::rotate(glm::mat4(1.0f), placedMiners[i].rotation, glm::vec3(0.0f, 1.0f, 0.0f)) *
                       component.model.Wm;
        ubos.mvpMat[i] = ViewPrj * ubos.mMat[i];
        ubos.nMat[i] = glm::inverse(glm::transpose(ubos.mMat[i]));
      }

      component.standardDescriptorSet.map(currentImage, &ubos, 0);
    }

    if (isPlacingDrill) {
      UniformBufferObjectSimp ubosComponent{};
      previewTransform = glm::translate(
          glm::mat4(1.0f), calculateGroundPlacementPosition(
                               cameraPos, getLookingVector(), gridSize)) *
                         glm::rotate(glm::mat4(1.0f), previewRotation, glm::vec3(0.0f, 1.0f, 0.0f));

      for (auto &component : minerStructure.components) {
        ubosComponent.mMat[0] = previewTransform * component.model.Wm;
        ubosComponent.mvpMat[0] = ViewPrj * ubosComponent.mMat[0];
        ubosComponent.nMat[0] =
            glm::inverse(glm::transpose(ubosComponent.mMat[0]));

        component.previewDescriptorSet.map(currentImage, &ubosComponent, 0);
      }
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
    const float MOVE_SPEED_RUN = 5.0f;

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

  static void mouse_button_callback(GLFWwindow *window, int button, int action,
                                    int mods) {
    Factotum *app = (Factotum *)glfwGetWindowUserPointer(window);
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
      glm::vec3 newPos = app->calculateGroundPlacementPosition(
          app->cameraPos, app->getLookingVector(), app->gridSize);

      bool positionOccupied = false;
      for (const auto &pos : app->placedMiners) {
        if (pos.position == newPos) {
          positionOccupied = true;
          break;
        }
      }

      if (!positionOccupied) {
        PlacedMiner newPlacedMiner;
        newPlacedMiner.position = newPos;
        newPlacedMiner.rotation = app->previewRotation;
        app->placedMiners.push_back(newPlacedMiner);
        app->submitCommandBuffer("main", 0, populateCommandBufferAccess, app);
        std::cout << "Miner position: " << app->placedMiners.back().position.x << ","
                  << app->placedMiners.back().position.y << ","
                  << app->placedMiners.back().position.z << ", rotation: " << glm::degrees(app->placedMiners.back().rotation) << " degrees\n";
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
