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

#define UNUSED(x) (void)x

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
light_info_t mLight;
Transform mLightTransform;
Texture2::sptr_t texNormal;
Texture2::sptr_t texScene;
Texture2::sptr_t texNoise;
//VertexBuffer::sptr_t vbo[4];
//IndexBuffer::sptr_t ibo;

Mesh* mesh;
RHIBuffer::sptr_t cVp;
RHIBuffer::sptr_t cLight;
Texture2::sptr_t texture;
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
  mCamera->transfrom().localPosition() = { -1, -1, -1 };
  mCamera->lookAt({ -1, -1, -1 }, vec3::zero);
  mCamera->setProjectionPrespective(30.f, 3.f*CLIENT_ASPECT, 3.f, .1f, 500.f);
  
  mDevice = RHIDevice::get();
  mContext = mDevice->defaultRenderContext();

  renderer = &ImmediateRenderer::get();
  uint w = Window::Get()->bounds().width();
  uint h = (uint)Window::Get()->bounds().height();
  texNormal = Texture2::create(
      w, 
      h, 
      TEXTURE_FORMAT_RGBA8, 
      RHIResource::BindingFlag::RenderTarget);

  ssaoMap = Texture2::create(
    w,
    h,
    TEXTURE_FORMAT_RGBA8,
    RHIResource::BindingFlag::RenderTarget);

  texScene = Texture2::create(
    w,
    h,
    TEXTURE_FORMAT_RGBA8,
    RHIResource::BindingFlag::RenderTarget | RHIResource::BindingFlag::UnorderedAccess);

  // main pass
  {
    // FrameBuffer::Desc fDesc;
    // fDesc.defineColorTarget(0, TEXTURE_FORMAT_RGBA8);
    // fDesc.defineColorTarget(1, TEXTURE_FORMAT_RGBA8);
    // fDesc.defineDepthTarget(TEXTURE_FORMAT_D24S8);
    // frameBuffer = new FrameBuffer(fDesc);
    // {
    //   RootSignature::Desc desc;
    //   RootSignature::desc_set_layout_t layout;
    //   layout.addRange(DescriptorSet::Type::Cbv, 0, 2, 0);
    //   layout.addRange(DescriptorSet::Type::TextureSrv, 0, 1, 0);
    //   descriptorSet = DescriptorSet::create(mDevice->gpuDescriptorPool(), layout);
    //   desc.addDescriptorSet(layout);
    //   rootSig = RootSignature::create(desc);
    // }
    {
      GraphicsState::Desc desc;

      std::string shaderPath = "shaders.hlsl";
      Program::sptr_t prog = Program::sptr_t(new Program());
      prog->stage(SHADER_TYPE_VERTEX).setFromFile(shaderPath, "VSMain");
      prog->stage(SHADER_TYPE_FRAGMENT).setFromFile(shaderPath, "PSMain");
      prog->compile();
      renderer->setProgram(prog);


      // GraphicsState = prog->compile();
    }

    Mesher ms;

    ms.begin(DRAW_TRIANGES, false);
    // ms.cube(vec3(30.f, 100.f, 0), vec3(200.f));
    ms.cube(vec3(0.f, 10.f, 0.f), vec3(20.f));
    ms.cube(vec3(10.f, 5.f, 0.f), vec3(10.f));
    // ms.cube(vec3(20.f, 7.5f, 20.f), vec3(15.f));
    ms.quad(vec3(0, -10, 7.f), vec3::right, vec3::up, vec2(40.f));
    // ms.quad(vec3(-30, 10, 0), -vec3::forward, vec3::up, vec2(20.f));
    ms.end();
    // ms.begin(DRAW_TRIANGES, false);
    // ms.cube(vec3(-20.f, 15.f, -30), vec3(30.f));
    // ms.end();
    ms.begin(DRAW_TRIANGES, false);
    ms.quad(vec3::zero, vec3::right, vec3::forward, vec2(300.f));
    // // ms.sphere(vec3(0, 10.f, 0), 10.f, 15, 15);
    ms.end();

    buildMeshDataForCompute(ms);

    mesh = ms.createMesh<vertex_lit_t>();
    meshRenderable.mesh() = mesh;
    meshRenderable.transform() = new Transform();

    meshRenderable.material(defaultMaterial);

    camera_t cameraUbo = mCamera->ubo();
    cVp = RHIBuffer::create(sizeof(camera_t), RHIResource::BindingFlag::ConstantBuffer, RHIBuffer::CPUAccess::Write, &cameraUbo);

    light_info_t light;
    light.asSpotLight(mCamera->transfrom().position(), mCamera->transfrom().forward(), 5.f, 10.f, 1.f);
    cLight = RHIBuffer::create(sizeof(light_info_t), RHIResource::BindingFlag::ConstantBuffer, RHIBuffer::CPUAccess::Write,
    &light);
    //
    std::vector<UINT8> data = GenerateTextureData();
    texture = Texture2::create(texWidth, texHeight, TEXTURE_FORMAT_RGBA8, RHIResource::BindingFlag::ShaderResource, data.data(), data.size());

    defaultMaterial.init();
    defaultMaterial.setTexture(TEXTURE_DIFFUSE, texture);

    std::vector<Rgba> noise = genNoise(w, h);
    texNoise = Texture2::create(w, h, TEXTURE_FORMAT_RGBA8,
      RHIResource::BindingFlag::ShaderResource,
      noise.data(), w * h * sizeof(Rgba));
    // descriptorSet->setCbv(0, 0, *cVp->cbv());
    // descriptorSet->setCbv(0, 1, *cLight->cbv());
    // descriptorSet->setSrv(1, 0, texture->srv());
    mContext->resourceBarrier(texture.get(), RHIResource::State::ShaderResource);

    scene.add(meshRenderable);
    scene.set(*mCamera);
  }

  // SSAO resource
  // {
  //   FrameBuffer::Desc fDesc;
  //   fDesc.defineColorTarget(0, TEXTURE_FORMAT_RGBA8);
  //   fDesc.defineColorTarget(1, TEXTURE_FORMAT_RGBA8);
  //   ssaoFrameBuffer = new FrameBuffer(fDesc);
  //   {
  //     RootSignature::Desc desc;
  //     RootSignature::desc_set_layout_t layout;
  //     layout.addRange(DescriptorSet::Type::Cbv, 0, 2);
  //     layout.addRange(DescriptorSet::Type::TextureSrv, 0, 4);
  //     ssaoDescriptorSet = DescriptorSet::create(mDevice->gpuDescriptorPool(), layout);
  //     desc.addDescriptorSet(layout);
  //     ssaoRootSig = RootSignature::create(desc);
  //   }
  //   {
  //     GraphicsState::Desc desc;
  //
  //     std::string shaderPath = "ssao.hlsl";
  //     Program::sptr_t prog = Program::sptr_t(new Program());
  //     prog->stage(SHADER_TYPE_VERTEX).setFromFile(shaderPath, "VSMain");
  //     prog->stage(SHADER_TYPE_FRAGMENT).setFromFile(shaderPath, "PSMain");
  //     prog->compile();
  //     desc.setProgram(prog);
  //
  //     desc.setRootSignature(ssaoRootSig);
  //     desc.setPrimTye(GraphicsState::PrimitiveType::Triangle);
  //     desc.setVertexLayout(VertexLayout::For<vertex_lit_t>());
  //     desc.setFboDesc(fDesc);
  //     ssaoGraphicsState = GraphicsState::create(desc);
  //   }
  //
  //   cSSAOParams = RHIBuffer::create(sizeof(ssao_param_t), RHIResource::BindingFlag::ConstantBuffer, RHIBuffer::CPUAccess::Write, &mSSAOParam);
  //   ssaoParamCbv = ConstantBufferView::create(cSSAOParams);
  //   ssaoDescriptorSet->setCbv(0, 0, *vpCbv);
  //   ssaoDescriptorSet->setCbv(0, 1, *ssaoParamCbv);
  //   ssaoDescriptorSet->setSrv(1, 1, texNormal->srv());
  //   ssaoDescriptorSet->setSrv(1, 2, texScene->srv());
  //   ssaoDescriptorSet->setSrv(1, 3, texNoise->srv());
  // }

  // compute
  {
    {
      RootSignature::Desc desc;
      RootSignature::desc_set_layout_t layout;
      layout.addRange(DescriptorSet::Type::TextureUav, 0, 2);
      layout.addRange(DescriptorSet::Type::Cbv, 0, 2);
      computeDescriptorSet = DescriptorSet::create(mDevice->gpuDescriptorPool(), layout);
      desc.addDescriptorSet(layout);
      computeRootSig = RootSignature::create(desc);
    }
    {
      ComputeState::Desc desc;
      desc.setRootSignature(computeRootSig);
      computePipelineState = ComputeState::create(desc);
    }

    computeDescriptorSet->setUav(0, 0, *texScene->uav());
    computeDescriptorSet->setUav(0, 1, *computeVerts->uav());
    computeDescriptorSet->setCbv(1, 0, *cVp->cbv());
    computeDescriptorSet->setCbv(1, 1, *cLight->cbv());
  }
}

bool runAO = true;
void onInput() {
  static float angle = 0.f;
  static float distance = 15.f;
  static float langle = 10.f;
  static float ldistance = 15.f;
  if(Input::Get().isKeyDown('W')) {
    distance -= .5f;
  }
  if (Input::Get().isKeyDown('S')) {
    distance += 0.5f;
  }
  if (Input::Get().isKeyDown(KEYBOARD_UP)) {
    ldistance -= 0.5f;
  }
  if (Input::Get().isKeyDown(KEYBOARD_DOWN)) {
    ldistance += 0.5f;
  }

  if(Input::Get().isKeyDown(KEYBOARD_SPACE)) {
    runAO = false;
  } else {
    runAO = true;
  }
  distance = std::max(.1f, distance);
  ldistance = std::max(.1f, ldistance);
  if(Input::Get().isKeyDown('A')) {
    angle -= 1.f;
  }
  if (Input::Get().isKeyDown('D')) {
    angle += 1.f;
  }
  if (Input::Get().isKeyDown(KEYBOARD_LEFT)) {
    langle -= 1.f;
  }
  if (Input::Get().isKeyDown(KEYBOARD_RIGHT)) {
    langle += 1.f;
  }
  vec3 position = fromSpherical(distance, angle, 30.f);
  mCamera->lookAt(position, vec3::zero);
  mLightTransform.setWorldTransform(mat44::lookAt(fromSpherical(ldistance, langle, 30.f), vec3::zero));
  mLight.asSpotLight(mLightTransform.position(), mLightTransform.forward(), 30.f, 45.f, 3.f);

  vec3 fwd = mCamera->forward();

};

void computeTest() {
  GPU_FUNCTION_EVENT();
  mContext->resourceBarrier(texScene.get(), RHIResource::State::UnorderedAccess);
  mContext->setComputeState(*computePipelineState);
  computeDescriptorSet->bindForCompute(*mContext, *computeRootSig);

  uint x = uint(Window::Get()->bounds().width()) / 32 + 1;
  uint y = uint(Window::Get()->bounds().height()) / 32 + 1;
  mContext->dispatch(x, y, 1);

}

void render() {
  sceneRenderer->onRenderFrame(*mContext);
  mDevice->present();
}

void runFrame() {
  Input::Get().beforeFrame();
  onInput();
  render();
  Input::Get().afterFrame();

}

void Shutdown() {
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

