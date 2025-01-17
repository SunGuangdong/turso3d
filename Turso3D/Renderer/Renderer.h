// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../IO/JSONValue.h"
#include "../Object/AutoPtr.h"
#include "../Math/Color.h"
#include "../Math/Frustum.h"
#include "../Resource/Image.h"
#include "Batch.h"

class Camera;
class GeometryNode;
class Graphics;
class Material;
class Octree;
class RenderBuffer;
class Scene;
class UniformBuffer;
class VertexBuffer;

static const size_t NUM_CLUSTER_X = 16;
static const size_t NUM_CLUSTER_Y = 8;
static const size_t NUM_CLUSTER_Z = 8;
static const size_t MAX_LIGHTS = 255;
static const size_t MAX_LIGHTS_CLUSTER = 16;

/// High-level rendering subsystem. Performs rendering of 3D scenes.
class Renderer : public Object
{
    OBJECT(Renderer);

public:
    /// Construct. Register subsystem and objects. %Graphics subsystem must have been initialized.
    Renderer();
    /// Destruct.
    ~Renderer();

    /// Set size and format of shadow maps. First map is used for a directional light, the second as an atlas for others.
    void SetupShadowMaps(int dirLightSize, int lightAtlasSize, ImageFormat format);
    /// Set global depth bias multipiers for shadow maps.
    void SetShadowDepthBiasMul(float depthBiasMul, float slopeScaleBiasMul);
    /// Prepare view for rendering.
    void PrepareView(Scene* scene, Camera* camera, bool drawShadows);
    /// Render shadowmaps before rendering the view. Last shadow framebuffer will be left bound.
    void RenderShadowMaps();
    /// Render opaque objects into currently set framebuffer and viewport. 
    void RenderOpaque();
    /// Render transparent objects into currently set framebuffer and viewport.
    void RenderAlpha();

    /// Clear the current framebuffer.
    void Clear(bool clearColor = true, bool clearDepth = true, const IntRect& clearRect = IntRect::ZERO, const Color& backgroundColor = Color::BLACK);
    /// Set the viewport rectangle.
    void SetViewport(const IntRect& viewRect);
    /// Set basic renderstates.
    void SetRenderState(BlendMode blendMode, CullMode cullMode = CULL_BACK, CompareMode depthTest = CMP_LESS, bool colorWrite = true, bool depthWrite = true);
    /// Set depth bias.
    void SetDepthBias(float constantBias = 0.0f, float slopeScaleBias = 0.0f);

    /// Set a shader program and bind. Return pointer on success or null otherwise.
    ShaderProgram* SetProgram(const std::string& shaderName, const std::string& vsDefines = JSONValue::emptyString, const std::string& fsDefines = JSONValue::emptyString);
     /// Set float uniform. Low performance, provided for convenience.
    void SetUniform(ShaderProgram* program, const char* name, float value);
    /// Set Vector2 uniform. Low performance, provided for convenience.
    void SetUniform(ShaderProgram* program, const char* name, const Vector2& value);
    /// Set Vector3 uniform. Low performance, provided for convenience.
    void SetUniform(ShaderProgram* program, const char* name, const Vector3& value);
    /// Set Vector4 uniform. Low performance, provided for convenience.
    void SetUniform(ShaderProgram* program, const char* name, const Vector4& value);
    /// Draw a quad with current renderstate.
    void DrawQuad();

private:
    /// Find visible objects within frustum.
    void CollectVisibleNodes();
    /// Check which lights affect which objects.
    void CollectLightInteractions(bool drawShadows);
    /// Collect (unlit) shadow batches from geometry nodes and sort them.
    void CollectShadowBatches(ShadowMap& shadowMap, ShadowView& view, const std::vector<GeometryNode*>& potentialShadowCasters, bool checkFrustum);
    /// Collect batches from visible objects.
    void CollectNodeBatches();
    /// Sort batches from visible objects.
    void SortNodeBatches();
    /// Render a batch queue.
    void RenderBatches(Camera* camera, const std::vector<Batch>& batches);
    /// Allocate shadow map for light. Return true on success.
    bool AllocateShadowMap(Light* light);
    /// %Octree callback for collecting lights and geometries.
    void CollectGeometriesAndLights(std::vector<OctreeNode*>::const_iterator begin, std::vector<OctreeNode*>::const_iterator end, unsigned char planeMask);
    /// Define face selection texture for point light shadows.
    void DefineFaceSelectionTextures();
    /// Define vertex data for rendering full-screen quads.
    void DefineQuadVertexBuffer();
    /// Setup light cluster frustums and bounding boxes if necessary.
    void DefineClusterFrustums();

    /// Current scene.
    Scene* scene;
    /// Current scene octree.
    Octree* octree;
    /// Current camera.
    Camera* camera;
    /// Camera frustum.
    Frustum frustum;
    /// Geometries in frustum.
    std::vector<GeometryNode*> geometries;
    /// Brightest directional light in frustum.
    Light* dirLight;
    /// Point and spot lights in frustum.
    std::vector<Light*> lights;
    /// Initial shadowcaster list for processing shadowed lights.
    std::vector<GeometryNode*> initialShadowCasters;
    /// Intermediate filtered shadowcaster list for processing.
    std::vector<GeometryNode*> shadowCasters;
    /// Shadow maps.
    std::vector<ShadowMap> shadowMaps;
    /// Face selection UV indirection texture 1.
    AutoPtr<Texture> faceSelectionTexture1;
    /// Face selection UV indirection texture 2.
    AutoPtr<Texture> faceSelectionTexture2;
    /// Cluster lookup 3D texture.
    AutoPtr<Texture> clusterTexture;
    /// Light data uniform buffer.
    AutoPtr<UniformBuffer> lightDataBuffer;
    /// Cluster frustums for lights.
    Frustum clusterFrustums[NUM_CLUSTER_X * NUM_CLUSTER_Y * NUM_CLUSTER_Z];
    /// Cluster bounding boxes.
    BoundingBox clusterBoundingBoxes[NUM_CLUSTER_X * NUM_CLUSTER_Y * NUM_CLUSTER_Z];
    /// Amount of lights per cluster.
    unsigned char numClusterLights[NUM_CLUSTER_X * NUM_CLUSTER_Y * NUM_CLUSTER_Z];
    /// Cluster data CPU copy.
    unsigned char clusterData[MAX_LIGHTS_CLUSTER * NUM_CLUSTER_X * NUM_CLUSTER_Y * NUM_CLUSTER_Z];
    /// Light constantbuffer data CPU copy
    LightData lightData[MAX_LIGHTS + 1];
    /// Last projection matrix used to initialize cluster frustums.
    Matrix4 lastClusterFrustumProj;
    /// Opaque batches.
    BatchQueue opaqueBatches;
    /// Transparent batches.
    BatchQueue alphaBatches;
    /// Instancing world transforms.
    std::vector<Matrix3x4> instanceTransforms;
    /// Instancing vertex buffer.
    AutoPtr<VertexBuffer> instanceVertexBuffer;
    /// Quad vertex buffer.
    AutoPtr<VertexBuffer> quadVertexBuffer;
    /// Cached static object shadow buffer.
    AutoPtr<RenderBuffer> staticObjectShadowBuffer;
    /// Cached static object shadow framebuffer.
    AutoPtr<FrameBuffer> staticObjectShadowFbo;
    /// Instancing supported flag.
    bool hasInstancing;
    /// Instancing vertex arrays enabled flag.
    bool instancingEnabled;
    /// Instancing buffer need update flag.
    bool instanceTransformsDirty;
    /// Shadow maps globally dirty flag. All cached shadow content should be reset.
    bool shadowMapsDirty;
    /// Cluster frustums init flag.
    bool clusterFrustumsDirty;
    /// Vertex elements for the instancing buffer.
    std::vector<VertexElement> instanceVertexElements;
    /// Camera view mask.
    unsigned viewMask;
    /// Framenumber.
    unsigned short frameNumber;
    /// Subview framenumber for state sorting.
    unsigned short sortViewNumber;
    /// Last camera used for rendering.
    Camera* lastCamera;
    /// Last camera uniforms assignment number.
    unsigned lastPerViewUniforms;
    /// Last material pass used for rendering.
    Pass* lastPass;
    /// Last material used for rendering.
    Material* lastMaterial;
    /// Last material uniforms assignment number.
    unsigned lastPerMaterialUniforms;
    /// Last blend mode.
    BlendMode lastBlendMode;
    /// Last cull mode.
    CullMode lastCullMode;
    /// Last depth test.
    CompareMode lastDepthTest;
    /// Last color write.
    bool lastColorWrite;
    /// Last depth write.
    bool lastDepthWrite;
    /// Last depth bias enabled.
    bool lastDepthBias;
    /// Constant depth bias multiplier.
    float depthBiasMul;
    /// Slope-scaled depth bias multiplier.
    float slopeScaleBiasMul;
};

/// Register Renderer related object factories and attributes.
void RegisterRendererLibrary();

