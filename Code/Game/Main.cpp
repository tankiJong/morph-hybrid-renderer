#define WIN32_LEAN_AND_MEAN		// Always #define this before #including <windows.h>
#include <Windows.h>


#include "Engine/Core/type.h"
#include "Engine/File/Utils.hpp"
#include "Engine/Graphics/RHI/RHIDevice.hpp"
#include "Engine/Graphics/RHI/RootSignature.hpp"
#include "Engine/Graphics/RHI/PipelineState.hpp"
#include "Engine/Graphics/RHI/RHITexture.hpp"
#include "Engine/Graphics/RHI/ResourceView.hpp"
#include "Engine/Graphics/RHI/TypedBuffer.hpp"
#include "Engine/Core/Time/Clock.hpp"
#include "Engine/Graphics/RHI/Texture.hpp"
#include "Engine/Graphics/Model/Vertex.hpp"
#include "Engine/Graphics/Camera.hpp"
#include "Engine/Core/Engine.hpp"
#include "Engine/Input/Input.hpp"
#include "Engine/Application/Window.hpp"
#include "Engine/Graphics/Model/Mesher.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Graphics/RHI/RHIBuffer.hpp"
#include "Engine/Graphics/Program/Program.hpp"
#include "Engine/Renderer/ImmediateRenderer.hpp"
#include "Engine/Renderer/SceneRenderer/SceneRenderer.hpp"
#include "Engine/Renderer/Renderable/Renderable.hpp"
#include "Engine/Graphics/Program/Material.hpp"
#include "Engine/Framework/Light.hpp"

#define UNUSED(x) (void)x
static float SCENE_SCALE = .002f;
static bool gQuit = false;
void CALLBACK windowProc(uint wmMessageCode, size_t /*wParam*/, size_t lParam) {
  UNUSED(lParam);
  switch (wmMessageCode) {
    // App close requested via "X" button, or right-click "Close Window" on task bar, or "Close" from system menu, or Alt-F4
    case WM_CLOSE:
    {
      gQuit = true;
      return; // "Consumes" this message (tells Windows "okay, we handled it")
    }
  }
}

// -------------------------  constant ------------------------------
constexpr uint frameCount = 2;
// ------------------------------------------------------------------

constexpr uint texWidth = 256;
constexpr uint texHeight = 256;
// Generate a simple black and white checkerboard texture.
std::vector<UINT8> GenerateTextureData() {
  const UINT rowPitch = texWidth * 4;
  const UINT cellPitch = rowPitch >> 3;		// The width of a cell in the checkboard texture.
  const UINT cellHeight = texHeight >> 3;	// The height of a cell in the checkerboard texture.
  const UINT textureSize = rowPitch * texHeight;

  std::vector<UINT8> data(textureSize);
  UINT8* pData = &data[0];

  for (UINT n = 0; n < textureSize; n += 4) {
    UINT x = n % rowPitch;
    UINT y = n / rowPitch;
    UINT i = x / cellPitch;
    UINT j = y / cellHeight;

    if (i % 2 == j % 2) {
      pData[n] =     0xff;		// R
      pData[n + 1] = 0xff;	// G
      pData[n + 2] = 0x00;	// B
      pData[n + 3] = 0xff;	// A
    } else {
      pData[n] =     0xff;		// R
      pData[n + 1] = 0x00;	// G
      pData[n + 2] = 0x00;	// B
      pData[n + 3] = 0xff;	// A
    }
  }

  return data;
}

std::vector<Rgba> genNoise(uint width, uint height) {
  std::vector<Rgba> noise;
  noise.resize(width * height);

  for(Rgba& n: noise) {
    vec3 normalized;
    normalized.x = getRandomf(-1.f, 1.f);
    normalized.y = 0.f;
    normalized.z = getRandomf(-1.f, 1.f);

    normalized = normalized * .5f + vec3(.5f);
    n = Rgba(normalized, 1.f);
  }

  return noise;
}

S<RHIDevice> mDevice;
S<RHIContext> mContext;
// GraphicsState::sptr_t GraphicsState;
// RootSignature::sptr_t rootSig;

Camera* mCamera;
//VertexBuffer::sptr_t vbo[4];
//IndexBuffer::sptr_t ibo;

Mesh* mesh;
RHIBuffer::sptr_t cVp;
RHIBuffer::sptr_t cLight;
Texture2::sptr_t mTexture;
ShaderResourceView::sptr_t texSrv;
// DescriptorSet::sptr_t descriptorSet;
FrameBuffer* frameBuffer;

#define NUM_KERNEL 64
struct ssao_param_t {
  vec4 kernels[NUM_KERNEL];
};
ssao_param_t mSSAOParam;
FrameBuffer* ssaoFrameBuffer;
GraphicsState::sptr_t ssaoGraphicsState;
RootSignature::sptr_t ssaoRootSig;
RHIBuffer::sptr_t cSSAOParams;
ConstantBufferView::sptr_t ssaoParamCbv;
DescriptorSet::sptr_t ssaoDescriptorSet;
Texture2::sptr_t ssaoMap;

ImmediateRenderer* renderer;

RHIBuffer::sptr_t computeVerts;
//uint elementCount = 0; 
//uint numVerts = 0;

Light mLight;
void buildMeshDataForCompute(Mesher& mesher) {
  uint size = mesher.mVertices.count();
  vec4* loca = (vec4*)_alloca(size * sizeof(vec4));

  float count = mesher.mIns.size();
  uint i = 0;

  vec3* position = mesher.mVertices.vertices().position;
  for(uint j = 0; j < count; j++) {
    for(uint e = 0; e < mesher.mIns[j].elementCount; e++) {
      loca[i] = vec4(position[i], float(j) / count);
      i++;
    }
  }

  loca[0].w = 0;
  loca[3].w = .25f;
  loca[6].w = .75f;
  loca[9].w = 1.f;

  computeVerts = RHIBuffer::create(sizeof(vec4) * size, RHIResource::BindingFlag::UnorderedAccess | RHIResource::BindingFlag::ShaderResource, RHIBuffer::CPUAccess::None);
  computeVerts->updateData(loca, 0, sizeof(vec4) * size);
}

void genSSAOData() {
  for(uint i = 0; i<NUM_KERNEL; i++) {
    float scale = (float)i / (float)NUM_KERNEL;
    scale = smoothStart3(scale);
    vec3 normalized;
    normalized.x = getRandomf(-1.f, 1.f);
    normalized.y = getRandomf01();
    normalized.z = getRandomf(-1.f, 1.f);

    normalized = normalized.normalized();
    normalized *= scale;
    mSSAOParam.kernels[i] = vec4(normalized, 0.f);
    //mSSAOParam.kernels[i] = vec4(normalized, 0.f);
  }
}


RootSignature::sptr_t computeRootSig;
DescriptorSet::sptr_t computeDescriptorSet;
ComputeState::sptr_t  computePipelineState;

SceneRenderer* sceneRenderer;
RenderScene scene;
Renderable meshRenderable;
Material defaultMaterial;
void Initialize() {
  sceneRenderer = new SceneRenderer(scene);
  sceneRenderer->onLoad(*mContext);
  genSSAOData();
  Window::Get()->addWinMessageHandler(windowProc);
  mCamera = new Camera();
  mCamera->lookAt({ 2, -2, -2 }, { -0.278000f, 0.273000f, 0.800000f });
  mCamera->setProjectionPrespective(19.5, 3.f*CLIENT_ASPECT, 3.f, 0.100000f, 1500.000000f);
  
  mDevice = RHIDevice::get();
  mContext = mDevice->defaultRenderContext();

  renderer = &ImmediateRenderer::get();
  uint w = Window::Get()->bounds().width();
  uint h = (uint)Window::Get()->bounds().height();

  // main pass
  {
    Mesher ms;

    ms.begin(DRAW_TRIANGES, false);
    // ms.cube(vec3(30.f, 100.f, 0), vec3(200.f));
    ms.color(vec4{ 0.725, 0.71, 0.68, 1.f});
    ms.quad(SCENE_SCALE * vec3{ 0.0f, 0.0f, 0.0f },
            SCENE_SCALE * vec3{ 500.f, 0.0f, 0.0f },
            SCENE_SCALE * vec3{ 500.f, 0.0f, 500.f },
            SCENE_SCALE * vec3{ 0.0f, 0.0f, 500.f });  // floor

    ms.quad(
            SCENE_SCALE * vec3{ 0.0f, 500.f, 500.f },
            SCENE_SCALE * vec3{ 500.f, 500.f, 500.f },
            SCENE_SCALE * vec3{ 500.f, 500.f, 0.0f },
            SCENE_SCALE * vec3{ 0.0f, 500.f,   0.0f } );  // ceiling

    ms.quad(SCENE_SCALE * vec3{ 500.f,   0.0f, 500.f },
            SCENE_SCALE * vec3{ 500.f, 500.f, 500.f },
            SCENE_SCALE * vec3{ 0.0f, 500.f, 500.f },
            SCENE_SCALE * vec3{ 0.0f,   0.0f, 500.f });  // back wall

    // ms.quad(SCENE_SCALE * vec3{ 0.0f,   0.0f, 0.f },
    //         SCENE_SCALE * vec3{ 0.0f, 548.8f, 0.f },
    //         SCENE_SCALE * vec3{ 556.0f, 548.8f, 0.f },
    //         SCENE_SCALE * vec3{ 549.6f,   0.0f, 0.f });  // back wall
    ms.color(vec4{ 0.725, 0.71, 0.68, 1.f });

    ms.cube(
            SCENE_SCALE * (vec3{ 82.f, 0.f, 114.f } +vec3{ 160.f, 0, 0 }),
            SCENE_SCALE * vec3{ 150.f, 145.f, 150.f },
            (vec3(240, 0, 65) - vec3(82, 0, 114)).normalized(),
            vec3::up,
            (vec3(130, 0, 272) - vec3(82, 0, 114)).normalized()); // short cube
    
    ms.cube(
            SCENE_SCALE * (vec3{ 265.f, 0, 406.f } +vec3{ -190.f, 0, -30.f }),
            SCENE_SCALE * vec3{ 160.f, 300.f, 150.f }, 
            (vec3(314, 0, 247) - vec3(265.f, 0, 406.f)).normalized(),
            vec3::up,
            (vec3(423, 0, 456) - vec3(265.f, 0, 406.f)).normalized()); // tall cube

    ms.color(vec4{ 0.14, 0.45, 0.091, 1.f }); // G
    ms.quad(SCENE_SCALE * vec3{ 500.f,   0.0f,  0.0f },
            SCENE_SCALE * vec3{ 500.f,  500.f,  0.0f },
            SCENE_SCALE * vec3{ 500.f,  500.f, 500.f },
            SCENE_SCALE * vec3{ 500.f,   0.0f, 500.f });  // right wall


    ms.color(vec4{ 0.63, 0.065, 0.05, 1.f }); // R
    ms.quad(SCENE_SCALE * vec3{ 0.0f,   0.0f,  500.f },
            SCENE_SCALE * vec3{ 0.0f,  500.f,  500.f },
            SCENE_SCALE * vec3{ 0.0f,  500.f,   0.0f },
            SCENE_SCALE * vec3{ 0.0f,   0.0f,   0.0f });  // left wall

    // ms.quad(vec3(-30, 10, 0), -vec3::forward, vec3::up, vec2(20.f));
    ms.end();
    // ms.begin(DRAW_TRIANGES, false);
    
    // ms.end();
    // ms.begin(DRAW_TRIANGES, false);
    // ms.color(Rgba(20, 20, 200));
    // ms.quad(vec3::zero, vec3::right, vec3::forward, vec2(300.f));
    // // ms.sphere(vec3(0, 10.f, 0), 10.f, 15, 15);
    // ms.end();

    buildMeshDataForCompute(ms);

    mesh = ms.createMesh<vertex_lit_t>();
    meshRenderable.mesh() = mesh;
    meshRenderable.transform() = new Transform();

    meshRenderable.material(defaultMaterial);

    camera_t cameraUbo = mCamera->ubo();
    cVp = RHIBuffer::create(sizeof(camera_t), RHIResource::BindingFlag::ConstantBuffer, RHIBuffer::CPUAccess::Write, &cameraUbo);

    mLight.asSpotLight(5.f, 10.f, 1.f);

    //
    std::vector<UINT8> data = GenerateTextureData();
    mTexture = Texture2::create(texWidth, texHeight, TEXTURE_FORMAT_RGBA8, RHIResource::BindingFlag::ShaderResource, data.data(), data.size());
    NAME_RHIRES(mTexture);
    defaultMaterial.init();
    defaultMaterial.setTexture(TEXTURE_DIFFUSE, mTexture);

    std::vector<Rgba> noise = genNoise(w, h);
    
    mContext->resourceBarrier(mTexture.get(), RHIResource::State::ShaderResource);

    scene.add(meshRenderable);
    scene.set(*mCamera);
    scene.add(mLight);
  }
  vec3 position = SCENE_SCALE * vec3{ 250, 250, -1500 };
  mCamera->lookAt(position, position + vec3::forward);

}

bool runAO = true;
void onInput() {

  Window::Get()->setTitle(Stringf("Tanki - Hybird Renderer. Frame time: %.0f ms", 
                                  float(GetMainClock().frame.second * 1000.0)).c_str());

  float shift = 0;
  float distance = 0;
  static float langle = -45.f;
  static float ldistance = 497.8f;
  static float lx = 250.f;
  if(Input::Get().isKeyDown('W')) {
    distance -= .5f;
  }
  if (Input::Get().isKeyDown('S')) {
    distance += 0.5f;
  }
  if (Input::Get().isKeyDown(KEYBOARD_UP)) {
    ldistance -= 10.f;
  }
  if (Input::Get().isKeyDown(KEYBOARD_DOWN)) {
    ldistance += 10.f;
  }

  runAO = !Input::Get().isKeyDown(KEYBOARD_SPACE);

  // distance = std::max(.1f, distance);
  // ldistance = std::max(.1f, ldistance);
  if(Input::Get().isKeyDown('A')) {
    shift -= 1.f;
  }
  if (Input::Get().isKeyDown('D')) {
    shift += 1.f;
  }
  if (Input::Get().isKeyDown(KEYBOARD_LEFT)) {
    lx -= 10.f;
  }
  if (Input::Get().isKeyDown(KEYBOARD_RIGHT)) {
    lx += 10.f;
  }

  vec2 rotation = Input::Get().mouseDeltaPosition(true) * 90.f;

  mCamera->translate(mCamera->forward() * distance + mCamera->right() * shift);
  // mCamera->rotate({ rotation.y, rotation.x, 0 });
  // vec3 position = SCENE_SCALE * vec3{ 278, 273, -800};
  // mCamera->lookAt(position, position + vec3::forward);
  mLight.transform.localPosition() = SCENE_SCALE * vec3{ lx, ldistance, 250.f };
  mLight.asPointLight(1.f, vec3{ 1.f, 0, 0.f }, vec3(1.f, 1.f, 1.f));
};

void render() {
  sceneRenderer->onRenderFrame(*mContext);
  mDevice->present();
}

void runFrame() {
  GetMainClock().beginFrame();
  Input::Get().beforeFrame();
  onInput();
  render();
  Input::Get().afterFrame();

}

void Shutdown() {
  delete sceneRenderer;
  mDevice->cleanup();

}
//-----------------------------------------------------------------------------------------------
int __stdcall WinMain(HINSTANCE, HINSTANCE, LPSTR commandLineString, int) {
  UNUSED(commandLineString);
  Engine::Get();
  Initialize();

  // Program main loop; keep running frames until it's time to quit
  while (!gQuit) {
    MSG message;
    while (PeekMessage(&message, mDevice->mWindow, 0, 0, PM_REMOVE)) {
      TranslateMessage(&message);
      DispatchMessage(&message);
    }
    runFrame();
  }

  Shutdown();
  return 0;
}

