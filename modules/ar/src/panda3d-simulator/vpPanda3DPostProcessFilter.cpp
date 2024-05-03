#include <visp3/ar/vpPanda3DPostProcessFilter.h>


#if defined(VISP_HAVE_PANDA3D)

const char *vpPanda3DPostProcessFilter::FILTER_VERTEX_SHADER = R"shader(
#version 330
in vec4 p3d_Vertex;
uniform mat4 p3d_ModelViewProjectionMatrix;
in vec2 p3d_MultiTexCoord0;
out vec2 texcoords;

void main()
{
  gl_Position = p3d_ModelViewProjectionMatrix * p3d_Vertex;
  texcoords = p3d_MultiTexCoord0;
}
)shader";


void vpPanda3DPostProcessFilter::setupScene()
{
  CardMaker cm("cm");
  cm.set_frame_fullscreen_quad();
  m_renderRoot = NodePath(cm.generate()); // Render root is a 2D rectangle
  m_renderRoot.set_depth_test(false);
  m_renderRoot.set_depth_write(false);
  GraphicsOutput *buffer = m_inputRenderer->getMainOutputBuffer();
  if (buffer == nullptr) {
    throw vpException(vpException::fatalError,
    "Cannot add a postprocess filter to a renderer that does not define getMainOutputBuffer()");
  }
  m_shader = Shader::make(Shader::ShaderLanguage::SL_GLSL,
                        FILTER_VERTEX_SHADER,
                        m_fragmentShader);
  m_renderRoot.set_shader(m_shader);
  m_renderRoot.set_shader_input("dp", LVector2f(1.0 / buffer->get_texture()->get_x_size(), 1.0 / buffer->get_texture()->get_y_size()));
  std::cout << m_fragmentShader << std::endl;
  m_renderRoot.set_texture(buffer->get_texture());
  m_renderRoot.set_attrib(LightRampAttrib::make_identity());
}

void vpPanda3DPostProcessFilter::setupCamera()
{
  m_cameraPath = m_window->make_camera();
  m_camera = (Camera *)m_cameraPath.node();
  PT(OrthographicLens) lens = new OrthographicLens();
  lens->set_film_size(2, 2);
  lens->set_film_offset(0, 0);
  lens->set_near_far(-1000, 1000);
  m_camera->set_lens(lens);
  m_cameraPath = m_renderRoot.attach_new_node(m_camera);
  m_camera->set_scene(m_renderRoot);
}

void vpPanda3DPostProcessFilter::setupRenderTarget()
{

  if (m_window == nullptr) {
    throw vpException(vpException::fatalError, "Cannot setup render target when window is null");
  }
  FrameBufferProperties fbp = getBufferProperties();
  WindowProperties win_prop;
  win_prop.set_size(m_renderParameters.getImageWidth(), m_renderParameters.getImageHeight());

  // Don't open a window - force it to be an offscreen buffer.
  int flags = GraphicsPipe::BF_refuse_window | GraphicsPipe::BF_resizeable;
  GraphicsOutput *windowOutput = m_window->get_graphics_output();
  GraphicsEngine *engine = windowOutput->get_engine();
  GraphicsStateGuardian *gsg = windowOutput->get_gsg();
  GraphicsPipe *pipe = windowOutput->get_pipe();
  m_buffer = engine->make_output(pipe, m_name, m_renderOrder,
                                      fbp, win_prop, flags,
                                      gsg, windowOutput);
  if (m_buffer == nullptr) {
    throw vpException(vpException::fatalError, "Could not create buffer");
  }
  m_buffers.push_back(m_buffer);
  m_buffer->set_inverted(gsg->get_copy_texture_inverted());
  m_texture = new Texture();
  m_buffer->add_render_texture(m_texture, m_isOutput ? GraphicsOutput::RenderTextureMode::RTM_copy_ram : GraphicsOutput::RenderTextureMode::RTM_copy_texture);
  m_buffer->set_clear_color(LColor(0.f));
  m_buffer->set_clear_color_active(true);
  DisplayRegion *region = m_buffer->make_display_region();
  if (region == nullptr) {
    throw vpException(vpException::fatalError, "Could not create display region");
  }
  region->set_camera(m_cameraPath);
  region->set_clear_color(LColor(0.f));
}

void vpPanda3DPostProcessFilter::setRenderParameters(const vpPanda3DRenderParameters &params)
{
  m_renderParameters = params;
  unsigned int previousH = m_renderParameters.getImageHeight(), previousW = m_renderParameters.getImageWidth();
  bool resize = previousH != params.getImageHeight() || previousW != params.getImageWidth();

  m_renderParameters = params;
  if (m_window != nullptr) {
    GraphicsOutput *buffer = m_inputRenderer->getMainOutputBuffer();
    m_renderRoot.set_shader_input("dp", LVector2f(1.0 / buffer->get_texture()->get_x_size(), 1.0 / buffer->get_texture()->get_y_size()));
  }
  if (resize) {
    for (GraphicsOutput *buffer: m_buffers) {
      //buffer->get_type().is_derived_from()
      GraphicsBuffer *buf = dynamic_cast<GraphicsBuffer *>(buffer);
      if (buf == nullptr) {
        throw vpException(vpException::fatalError, "Panda3D: could not cast to GraphicsBuffer when rendering.");
      }
      else {
        buf->set_size(m_renderParameters.getImageWidth(), m_renderParameters.getImageHeight());
      }
    }
  }
}

#endif
