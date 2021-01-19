/*
 *  Copyright (C) 1998-2000 Peter Alm, Mikael Alm, Olle Hallnas, Thomas Nilsson and 4Front Technologies
 *  Copyright (C) 2005-2020 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */


/*
 *  Mon Dec 08 23:59:59 CET 2020
 *  Modified by Captain Geek < CaptainGeek@users.noreply.github.com> to add the following:
 *  * true FFT display (since the previous implementation used wave data with some weird algorithm).
 *  * color types of the bars.
 *  * DoxyGen style function comments.
 *  * shuffled arround a bunch of things.
 */

/*
 *  Wed May 24 10:49:37 CDT 2000
 *  Fixes to threading/context creation for the nVidia X4 drivers by
 *  Christian Zander <phoenix@minion.de>
 */

/*
 *  Ported to XBMC by d4rk
 *  Also added 'm_hSpeed' to animate transition between bar heights
 *
 *  Ported to GLES 2.0 by Gimli
 */

/* MACRO DEFINES */
/* Defines the number of bars to display in the X and Y planes, uses the same number for both. */
#define NUM_BARS  (16U)



/* The "__STDC_LIMIT_MACROS" define is not really self explanatory, so I did an inquiry: */
/* */
/* Explanation of the macros from Dingo on StackExchange */
/* __STDC_LIMIT_MACROS and __STDC_CONSTANT_MACROS are a workaround to allow C++ programs to use        */
/* stdint.h macros specified in the C99 standard that aren't in the C++ standard. The macros,          */
/* such as UINT8_MAX, INT64_MIN, and INT32_C() may be defined already in C++ applications in other     */
/* ways. To allow the user to decide if they want the macros defined as C99 does, many implementations */
/* require that __STDC_LIMIT_MACROS and __STDC_CONSTANT_MACROS be defined before stdint.h is included. */
/* This isn't part of the C++ standard, but it has been adopted by more than one implementation.       */
/* */
/* Explanation of the macros from laalto on StackExchange */
/* In stdint.h under C++, they control whether to define macros like INT32_MAX or INT32_C(v). */
/* See your platform's stdint.h for additional information.*/
/* */
/* Explanation of the macros from malat on StackExchange */
/* The above issue has vanished. C99 is an old standard, so this has been explicitly overruled in the 
/* C++11 standard, and as a consequence C11 has removed this rule.*/
/* */
/* More details there: */
/*    https://sourceware.org/bugzilla/show_bug.cgi?id=15366 */
#define __STDC_LIMIT_MACROS

/* The definition of PI is OUT OF PLACE, it should not be here, on Ubuntu Linux this was not needed */
/* If it is needed, then it means there is another problem */
/*
#ifndef M_PI
#define M_PI 3.141592654f
#endif
*/

/* INCLUDES */
#include <kodi/addon-instance/Visualization.h>
#include <kodi/gui/gl/GL.h>
#include <kodi/gui/gl/Shader.h>

#include <string.h>
#include <math.h>
#include <stdint.h>
#include <cstddef>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

/* CLASS DEFINITION */
class ATTRIBUTE_HIDDEN CVisualizationSpectrum
  : public kodi::addon::CAddonBase,
    public kodi::addon::CInstanceVisualization,
    public kodi::gui::gl::CShaderProgram
{
public:
  CVisualizationSpectrum();
  ~CVisualizationSpectrum() override = default;

  bool Start(int channels, int samplesPerSec, int bitsPerSample, std::string songName) override;
  void Stop() override;
  void Render() override;
  void AudioData(const float* audioData, int audioDataLength, float* freqData, int freqDataLength) override;
  ADDON_STATUS SetSetting(const std::string& settingName, const kodi::CSettingValue& settingValue) override;

  void OnCompiledAndLinked() override;
  bool OnEnabled() override;

  void GetInfo(bool &wantsFreq, int &syncDelay) override;

private:
  // Setter functions
  void SetBarHeightSetting(int settingValue);
  void SetSpeedSetting(int settingValue);
  void SetModeSetting(int settingValue);
  void SetBarColorSetting(int settingValue);
  void SetRotationSpeedSetting(int settingValue);

  GLfloat   m_heights [NUM_BARS][NUM_BARS];
  GLfloat   m_scale;
  GLenum    m_mode;
  float m_y_angle, m_y_speed, m_y_fixedAngle;
  float m_x_angle, m_x_speed;
  float m_z_angle, m_z_speed;
  int   m_updateLag;

  // Helper functions
  void draw_bar(GLfloat x_offset, GLfloat z_offset, GLfloat height, GLfloat red, GLfloat green, GLfloat blue);
  void draw_all_bars(void);

  // Private data
  int   m_bar_color_type;
  bool  m_debugInfoAlreadyDisplayed = false;

  glm::mat4 m_projMat;
  glm::mat4 m_modelMat;
  GLfloat   m_pointSize = 0.0f;

  std::vector<glm::vec3> m_vertex_buffer_data;
  std::vector<glm::vec3> m_color_buffer_data;
  #ifdef HAS_GL
    GLuint  m_vertexVBO[2] = {0};
  #endif

  // Shader related data
  GLint     m_uProjMatrix = -1;
  GLint     m_uModelMatrix = -1;
  GLint     m_uPointSize = -1;
  GLint     m_hPos = -1;
  GLint     m_hCol = -1;

  bool  m_startOK = false;
};


/* CLASS IMPLEMENTATION */

/**
 * Class constructor.
 *
 * It constructrs... :)
 */
CVisualizationSpectrum::CVisualizationSpectrum()
  : m_mode(GL_TRIANGLES),
    m_y_angle(45.0f),
    m_y_speed(1.5f),
    m_x_angle(20.0f),
    m_x_speed(0.0f),
    m_z_angle(0.0f),
    m_z_speed(0.0f), // Setting the speed to zero will disable rotation around that axis.
    m_updateLag(0),  // Previously this was named m_hSpeed, which is not pertinent anymmore.
    m_bar_color_type(0)
{
  m_scale = 1.0 / log(256.0);

  SetBarHeightSetting(kodi::GetSettingInt("bar_height"));
  SetSpeedSetting(kodi::GetSettingInt("speed"));
  SetModeSetting(kodi::GetSettingInt("mode"));
  m_y_fixedAngle = kodi::GetSettingInt("rotation_angle");

  SetBarColorSetting (kodi::GetSettingInt("bar_color_type"));
  SetRotationSpeedSetting(kodi::GetSettingInt("rotation_speed"));

  m_vertex_buffer_data.resize(48);
  m_color_buffer_data.resize(48);

  kodi::Log(ADDON_LOG_INFO, "Spectrumolator construction completed...");
}


/**
 * Start function of CVisualizationSpectrum class.
 *
 * TBI (add description)
 * This description is not based on the documentation.
 *
 * @param[in] channels
 * @param[in] samplesPerSec
 * @param[in] bitsPerSample
 * @param[in] songName
 */
bool CVisualizationSpectrum::Start(int channels, int samplesPerSec, int bitsPerSample, std::string songName)
{
  (void)channels;
  (void)samplesPerSec;
  (void)bitsPerSample;
  (void)songName;

  std::string fraqShader = kodi::GetAddonPath("resources/shaders/" GL_TYPE_STRING "/frag.glsl");
  std::string vertShader = kodi::GetAddonPath("resources/shaders/" GL_TYPE_STRING "/vert.glsl");
  if (!LoadShaderFiles(vertShader, fraqShader) || !CompileAndLink())
  {
    kodi::Log(ADDON_LOG_ERROR, "Failed to create or compile shader");
    return false;
  }

  int x, y;

  for(x = 0; x < NUM_BARS; x++)
  {
    for(y = 0; y < NUM_BARS; y++)
    {
      m_heights[y][x] = 0.0f;
    }
  }

/*
  //Removed superfluous initialiation of member variables, this is the job of the constructor!
  m_x_speed = 0.0f;
  m_y_speed = 0.5f;
  m_z_speed = 0.0f;
  m_x_angle = 20.0f;
  m_y_angle = 45.0f;
  m_z_angle = 0.0f;
*/

  m_projMat = glm::frustum(-1.0f, 1.0f, -1.0f, 1.0f, 1.5f, 10.0f);

#ifdef HAS_GL
  glGenBuffers(2, m_vertexVBO);
#endif

  m_startOK = true;
  return true;
}


/**
 * Stop function of CVisualizationSpectrum class.
 *
 * TBI (add description)
 * This description is not based on the documentation.
 *
 */
void CVisualizationSpectrum::Stop()
{
  if (!m_startOK)
    return;

  m_startOK = false;

#ifdef HAS_GL
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glDeleteBuffers(2, m_vertexVBO);
  m_vertexVBO[0] = 0;
  m_vertexVBO[1] = 0;
#endif
}

/**
 * Rendering function.
 *
 * Called once per frame. Do all rendering here.
 */
void CVisualizationSpectrum::Render()
{
  if (!m_startOK)
    return;

#ifdef HAS_GL
  glBindBuffer(GL_ARRAY_BUFFER, m_vertexVBO[0]);
  glVertexAttribPointer(m_hPos, 3, GL_FLOAT, GL_FALSE, sizeof(GLfloat)*3, nullptr);
  glEnableVertexAttribArray(m_hPos);

  glBindBuffer(GL_ARRAY_BUFFER, m_vertexVBO[1]);
  glVertexAttribPointer(m_hCol, 3, GL_FLOAT, GL_FALSE, sizeof(GLfloat)*3, nullptr);
  glEnableVertexAttribArray(m_hCol);
#else
  // 1st attribute buffer : vertices
  glEnableVertexAttribArray(m_hPos);
  glVertexAttribPointer(m_hPos, 3, GL_FLOAT, GL_FALSE, 0, &m_vertex_buffer_data[0]);

  // 2nd attribute buffer : colors
  glEnableVertexAttribArray(m_hCol);
  glVertexAttribPointer(m_hCol, 3, GL_FLOAT, GL_FALSE, 0, &m_color_buffer_data[0]);
#endif

  glDisable(GL_BLEND);
#ifdef HAS_GL
  glEnable(GL_PROGRAM_POINT_SIZE);
#endif
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);

  // Clear the screen
  glClear(GL_DEPTH_BUFFER_BIT);

  m_x_angle += m_x_speed;
  if(m_x_angle >= 360.0f)
    m_x_angle -= 360.0f;

  if (m_y_fixedAngle < 0.0f)
  {
    m_y_angle += m_y_speed;
    if(m_y_angle >= 360.0f)
      m_y_angle -= 360.0f;
  }
  else
  {
    m_y_angle = m_y_fixedAngle;
  }

  m_z_angle += m_z_speed;
  if(m_z_angle >= 360.0f)
    m_z_angle -= 360.0f;

  m_modelMat = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -0.5f, -5.0f));
  m_modelMat = glm::rotate(m_modelMat, glm::radians(m_x_angle), glm::vec3(1.0f, 0.0f, 0.0f));
  m_modelMat = glm::rotate(m_modelMat, glm::radians(m_y_angle), glm::vec3(0.0f, 1.0f, 0.0f));
  m_modelMat = glm::rotate(m_modelMat, glm::radians(m_z_angle), glm::vec3(0.0f, 0.0f, 1.0f));

  EnableShader();

  draw_all_bars();

  DisableShader();

  glDisableVertexAttribArray(m_hPos);
  glDisableVertexAttribArray(m_hCol);

  glDisable(GL_DEPTH_TEST);
#ifdef HAS_GL
  glDisable(GL_PROGRAM_POINT_SIZE);
#endif
  glEnable(GL_BLEND);
}

void CVisualizationSpectrum::OnCompiledAndLinked()
{
  // Variables passed directly to the Vertex shader
  m_uProjMatrix = glGetUniformLocation(ProgramHandle(), "u_projectionMatrix");
  m_uModelMatrix = glGetUniformLocation(ProgramHandle(), "u_modelViewMatrix");
  m_uPointSize = glGetUniformLocation(ProgramHandle(), "u_pointSize");
  m_hPos = glGetAttribLocation(ProgramHandle(), "a_position");
  m_hCol = glGetAttribLocation(ProgramHandle(), "a_color");
}

bool CVisualizationSpectrum::OnEnabled()
{
  // This is called after glUseProgram()
  glUniformMatrix4fv(m_uProjMatrix, 1, GL_FALSE, glm::value_ptr(m_projMat));
  glUniformMatrix4fv(m_uModelMatrix, 1, GL_FALSE, glm::value_ptr(m_modelMat));
  glUniform1f(m_uPointSize, m_pointSize);

  return true;
}



/**
 * Function to draw one bar.
 *
 * Called only from draw_all_bars() function.
 *
 * @param GLfloat x_offset
 * @param GLfloat z_offset
 * @param GLfloat height
 * @param GLfloat red
 * @param GLfloat green
 * @param GLfloat blue
 */
void CVisualizationSpectrum::draw_bar(GLfloat x_offset, GLfloat z_offset, GLfloat height, GLfloat red, GLfloat green, GLfloat blue )
{
  GLfloat width = 0.1f;
  m_vertex_buffer_data =
  {
    // Bottom
    { x_offset + width, 0.0f,   z_offset + width },
    { x_offset,         0.0f,   z_offset },
    { x_offset + width, 0.0f,   z_offset },
    { x_offset + width, 0.0f,   z_offset + width },
    { x_offset,         0.0f,   z_offset + width },
    { x_offset,         0.0f,   z_offset },

    { x_offset,         0.0f,   z_offset + width },
    { x_offset + width, 0.0f,   z_offset },
    { x_offset + width, 0.0f,   z_offset + width },
    { x_offset,         0.0f,   z_offset + width },
    { x_offset + width, 0.0f,   z_offset },
    { x_offset,         0.0f,   z_offset },

    // Side
    { x_offset,         0.0f,   z_offset },
    { x_offset,         0.0f,   z_offset + width },
    { x_offset,         height, z_offset + width },
    { x_offset,         0.0f,   z_offset },
    { x_offset,         height, z_offset + width },
    { x_offset,         height, z_offset },

    { x_offset + width, height, z_offset },
    { x_offset,         0.0f,   z_offset },
    { x_offset,         height, z_offset },
    { x_offset + width, height, z_offset },
    { x_offset + width, 0.0f,   z_offset },
    { x_offset,         0.0f,   z_offset },

    { x_offset,         height, z_offset + width },
    { x_offset,         0.0f,   z_offset + width },
    { x_offset + width, 0.0f,   z_offset + width },
    { x_offset + width, height, z_offset + width },
    { x_offset,         height, z_offset + width },
    { x_offset + width, 0.0f,   z_offset + width },

    { x_offset + width, height, z_offset + width },
    { x_offset + width, 0.0f,   z_offset },
    { x_offset + width, height, z_offset },
    { x_offset + width, 0.0f,   z_offset },
    { x_offset + width, height, z_offset + width },
    { x_offset + width, 0.0f,   z_offset + width },

    // Top
    { x_offset + width, height, z_offset + width },
    { x_offset + width, height, z_offset },
    { x_offset,         height, z_offset },
    { x_offset + width, height, z_offset + width },
    { x_offset,         height, z_offset },
    { x_offset,         height, z_offset + width },

    { x_offset,         height, z_offset + width },
    { x_offset + width, height, z_offset },
    { x_offset,         height, z_offset },
    { x_offset + width, height, z_offset },
    { x_offset + width, height, z_offset + width },
    { x_offset,         height, z_offset + width }
  };

  float sideMlpy1, sideMlpy2, sideMlpy3, sideMlpy4;
  if (m_mode == GL_TRIANGLES)
  {
    sideMlpy1 = 0.5f;
    sideMlpy2 = 0.25f;
    sideMlpy3 = 0.75f;
    sideMlpy4 = 0.5f;
  }
  else
  {
    sideMlpy1 = sideMlpy2 = sideMlpy3 = sideMlpy4 = 1.0f;
  }

  // One color for each vertex. They were generated randomly.
  m_color_buffer_data =
  {
    // Bottom
    { red, green, blue },
    { red, green, blue },
    { red, green, blue },
    { red, green, blue },
    { red, green, blue },
    { red, green, blue },

    { red, green, blue },
    { red, green, blue },
    { red, green, blue },
    { red, green, blue },
    { red, green, blue },
    { red, green, blue },

    // Side
    { red * sideMlpy1, green * sideMlpy1, blue * sideMlpy1 },
    { red * sideMlpy1, green * sideMlpy1, blue * sideMlpy1 },
    { red * sideMlpy1, green * sideMlpy1, blue * sideMlpy1 },
    { red * sideMlpy1, green * sideMlpy1, blue * sideMlpy1 },
    { red * sideMlpy1, green * sideMlpy1, blue * sideMlpy1 },
    { red * sideMlpy1, green * sideMlpy1, blue * sideMlpy1 },

    { red * sideMlpy2, green * sideMlpy2, blue * sideMlpy2 },
    { red * sideMlpy2, green * sideMlpy2, blue * sideMlpy2 },
    { red * sideMlpy2, green * sideMlpy2, blue * sideMlpy2 },
    { red * sideMlpy2, green * sideMlpy2, blue * sideMlpy2 },
    { red * sideMlpy2, green * sideMlpy2, blue * sideMlpy2 },
    { red * sideMlpy2, green * sideMlpy2, blue * sideMlpy2 },

    { red * sideMlpy3, green * sideMlpy3, blue * sideMlpy3 },
    { red * sideMlpy3, green * sideMlpy3, blue * sideMlpy3 },
    { red * sideMlpy3, green * sideMlpy3, blue * sideMlpy3 },
    { red * sideMlpy3, green * sideMlpy3, blue * sideMlpy3 },
    { red * sideMlpy3, green * sideMlpy3, blue * sideMlpy3 },
    { red * sideMlpy3, green * sideMlpy3, blue * sideMlpy3 },

    { red * sideMlpy4, green * sideMlpy4, blue * sideMlpy4 },
    { red * sideMlpy4, green * sideMlpy4, blue * sideMlpy4 },
    { red * sideMlpy4, green * sideMlpy4, blue * sideMlpy4 },
    { red * sideMlpy4, green * sideMlpy4, blue * sideMlpy4 },
    { red * sideMlpy4, green * sideMlpy4, blue * sideMlpy4 },
    { red * sideMlpy4, green * sideMlpy4, blue * sideMlpy4 },

    // Top
    { red, green, blue },
    { red, green, blue },
    { red, green, blue },
    { red, green, blue },
    { red, green, blue },
    { red, green, blue },

    { red, green, blue },
    { red, green, blue },
    { red, green, blue },
    { red, green, blue },
    { red, green, blue },
    { red, green, blue },
  };

#ifdef HAS_GL
  glBindBuffer(GL_ARRAY_BUFFER, m_vertexVBO[0]);
  glBufferData(GL_ARRAY_BUFFER, m_vertex_buffer_data.size()*sizeof(glm::vec3), &m_vertex_buffer_data[0], GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, m_vertexVBO[1]);
  glBufferData(GL_ARRAY_BUFFER, m_color_buffer_data.size()*sizeof(glm::vec3), &m_color_buffer_data[0], GL_STATIC_DRAW);
#endif
  glDrawArrays(m_mode, 0, m_vertex_buffer_data.size()); /* 12*3 indices starting at 0 -> 12 triangles + 4*3 to have on lines show correct */
}


/**
 * Function to draw all the bars (it's in the name).
 *
 * Called only from CVisualizationSpectrum::Render() function.
 *
 */
void CVisualizationSpectrum::draw_all_bars(void)
{
  int x;
  int y;
  GLfloat x_offset;
  GLfloat y_offset;
  GLfloat r_base;
  GLfloat b_base;
  GLfloat rgb_component_r;
  GLfloat rgb_component_g;
  GLfloat rgb_component_b;

  for(y = 0; y < NUM_BARS; y++)
  {
    y_offset = -1.6 + ((NUM_BARS - y) * 0.2);

    b_base = y * (1.0 / NUM_BARS);
    r_base = 1.0 - b_base;

    for(x = 0; x < NUM_BARS; x++)
    {
      x_offset = -1.6 + ((float)x * 0.2);

      switch(m_bar_color_type)
      {
        case 2:
          /* Two gradient color */
          rgb_component_r = 1.0f - (float(x) - float(NUM_BARS))/float(NUM_BARS);
          rgb_component_g = (float(x) - float(NUM_BARS))/float(NUM_BARS);
          rgb_component_b = 0.0f;
          break;

        case 1:
          /* One solid color */
          rgb_component_r = 1;
          rgb_component_g = 0;
          rgb_component_b = 0;
          break;

        case 0:
        default:
          // Original code... which is a bit arbitrary
          rgb_component_r = r_base - (float(x) * (r_base / float(NUM_BARS))); /* R component */
          rgb_component_g = (float)x * (1.0 / float(NUM_BARS));                 /* G component */
          rgb_component_b = b_base;                                /* B component */
          break;
      };

      draw_bar( x_offset,            /* X Offset */
                y_offset,            /* Y Offset */
                m_heights[y][x],     /* Height */
                rgb_component_r,     /* R component */
                rgb_component_g,     /* G component */
                rgb_component_b);    /* B component */
    };
  };
}



/**
 * GetInfo function of CVisualizationSpectrum class.
 * 
 * Used to get the number of buffers from the current visualization.
 *
 * DoxyGen location: https://alwinesch.github.io/group__cpp__kodi__addon__visualization.html#ga5357e3fc0644da927689ec6161b2ef46
 *   virtual void GetInfo  ( bool &  wantsFreq, int &  syncDelay )   
 *
 * Note
 * If this function is not implemented, it will default to wantsFreq = false and syncDelay = 0.
 *
 * Parameters
 * @param[out] wantsFreq Indicates whether the add-on wants FFT data. If set to true, the freqData and freqDataLength parameters of AudioData() are used.
 * @param[out] syncDelay The number of buffers to delay before calling AudioData().
 */ 
void CVisualizationSpectrum::GetInfo(bool &wantsFreq, int &syncDelay)
{
  wantsFreq = true;
  syncDelay = m_updateLag;
}


/**
 * Implements the audio processing function.
 *
 * It performs a re-scaling of the number of "iFreqDataLength" FFT samples pointed by "pFreqData" to the NUM_BARS of bars.
 * The "GetInfo()" member function needs to be be overriden with a function to return "true" for the "wantsFFT" out parameter.
 * Otherwise, "iFreqDataLength" is always zero, when this function is called.
 *
 * @param[in] pAudioData 
 * @param[in] iAudioDataLength 
 * @param[in] pFreqData 
 * @param[in] iFreqDataLength
 */
void CVisualizationSpectrum::AudioData(const float* pAudioData, int iAudioDataLength, float *pFreqData, int iFreqDataLength)
{
  int x, y, c;
  int dividerOfFFTSamples;
  float *pTempFreqData;
  
  /* Shift the old data by one row backwards, no matter what we are going to display. Since, you know: "Tempus fugit!" */
  for (y = (NUM_BARS-1); y > 0; y--)
  {
    for (x = 0; x < NUM_BARS; x++)
    {
      m_heights[y][x] = m_heights[y - 1][x];
    };
  };

  /* If the number of FFT samples are less than the number of bars, we have a problem. */
  if (iFreqDataLength < NUM_BARS)
  {
    /* No valid FFT data, bailing out! But first, populate the buffer with some stuff, just in case. */
    for (x = 0; x < NUM_BARS; x++)
      m_heights[y][x] = -1.0f;
    
    kodi::Log(ADDON_LOG_ERROR, "iFreqDataLength=%d but we expected a number greater than: %d", iFreqDataLength, NUM_BARS);
  }
  else
  {
    /* Fetch the FFT data and convert it to the bar height that we want to display by shifting the bars to produce a time series... */
    
    dividerOfFFTSamples = iFreqDataLength / NUM_BARS;
    pTempFreqData = pFreqData;

    /* Display some debug info, but only once... */
    if (m_debugInfoAlreadyDisplayed == false)
    {
      kodi::Log(ADDON_LOG_DEBUG, "iAudioDataLength=%d, iFreqDataLength=%d, dividerOfFFTSamples=%d", iAudioDataLength, iFreqDataLength, dividerOfFFTSamples);
      m_debugInfoAlreadyDisplayed = true;
    };

    /* Computate the new data to vizualize */
    /* On my testing we get 256 FFT samples, that we want to show with 16 bars, therefore 16 FFT samples will be summed up into on bar's height. */
    for (x = 0; x < NUM_BARS; x++)
    {
      m_heights[0][x] = 0.0f;
      for (c = 0; c < dividerOfFFTSamples; c++)
      {
        m_heights[0][x] += *pTempFreqData;
        pTempFreqData++;
      };
      //m_heights[0][x] = m_heights[0][x] / (float)dividerOfFFTSamples;
    };
    
  };  /*End of: if (iFreqDataLength <= 0)*/
} /* End of the function: CVisualizationSpectrum::AudioData :) */





/* SETTER FUNCTIONS */
void CVisualizationSpectrum::SetBarHeightSetting(int settingValue)
{
  switch (settingValue)
  {
  case 1://standard
    m_scale = 1.f / log(256.f);
    break;

  case 2://big
    m_scale = 2.f / log(256.f);
    break;

  case 3://real big
    m_scale = 3.f / log(256.f);
    break;

  case 4://unused
    m_scale = 0.33f / log(256.f);
    break;

  case 0://small
  default:
    m_scale = 0.5f / log(256.f);
    break;
  }
}


void CVisualizationSpectrum::SetModeSetting(int settingValue)
{
  switch (settingValue)
  {
    case 1:
      m_mode = GL_LINES;
      m_pointSize = 0.0f;
      break;

    case 2:
      m_mode = GL_POINTS;
      m_pointSize = kodi::GetSettingInt("pointsize");
      break;

    case 0:
    default:
      m_mode = GL_TRIANGLES;
      m_pointSize = 0.0f;
      break;
  }
}

void CVisualizationSpectrum::SetSpeedSetting(int settingValue)
{
  /* Acceptable values should be: positive integers up to a reasonable limit (let's say 4, for now:) */
  if ((settingValue >= 0) && (settingValue <= 4))
  {
    m_updateLag = settingValue;
  };
}

void CVisualizationSpectrum::SetBarColorSetting(int settingValue)
{
  // TBI add an upper limit (for peace of mind) to the validation, but at the moment we don't really know how many color schemes will be supported.
  if (settingValue >= 0)
    m_bar_color_type = settingValue;
}

void CVisualizationSpectrum::SetRotationSpeedSetting(int settingValue)
{
  switch (settingValue)
  {
  case 4:
    m_y_speed = 10.0f;
    break;
  case 3:
    m_y_speed = 6.0f;
    break;
  case 2:
    m_y_speed = 3.0f;
    break;
  case 1:
    m_y_speed = 1.5f;
    break;
  case 0:
  default:
    m_y_speed = 0.5f;
    break;
  case -1:
    m_y_speed = 0.25f;
    break;
  case -2:
    m_y_speed = 0.0625f;
    break;
  case -3:
    m_y_speed = 0.03125f;
    break;
  case -4:
    m_y_speed = 0.015625f;
    break;
  };
}


/**
 * Brief description.
 *
 * Sets specific Setting value (called from Kodi)
 * Add-on master function.
 *
 * @param[in] settingName
 * @param[in] settingValue
 */
ADDON_STATUS CVisualizationSpectrum::SetSetting(const std::string& settingName, const kodi::CSettingValue& settingValue)
{
  if (settingName.empty() || settingValue.empty())
    return ADDON_STATUS_UNKNOWN;

  if (settingName == "bar_height")
  {
    SetBarHeightSetting(settingValue.GetInt());
    return ADDON_STATUS_OK;
  }
  else if (settingName == "speed")
  {
    SetSpeedSetting(settingValue.GetInt());
    return ADDON_STATUS_OK;
  }
  else if (settingName == "mode")
  {
    SetModeSetting(settingValue.GetInt());
    return ADDON_STATUS_OK;
  }
  else if (settingName == "rotation_angle")
  {
    m_y_fixedAngle = settingValue.GetInt();
    return ADDON_STATUS_OK;
  }
  else if (settingName == "bar_color_type")
  {
    SetBarColorSetting(settingValue.GetInt());
    return ADDON_STATUS_OK;
  }
  else if (settingName == "rotation_speed")
  {
    SetRotationSpeedSetting(settingValue.GetInt());
    return ADDON_STATUS_OK;
  }

  return ADDON_STATUS_UNKNOWN;
}

/**
 * Doxygen function commenting style, included here for reference only. TBI: can be deleted later.
 * Fetched from: https://www.doxygen.nl/manual/docblocks.html 
 */

/**
 * Brief description.
 *
 * Detailed description.
 *
 * @param paramName Parameter description. 
 */


ADDONCREATOR(CVisualizationSpectrum)
