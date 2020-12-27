#version 330 core

out vec4 FragColor;

in vec4 vertexColor;
in vec2 vertexUV;

// from General Shader
uniform vec3 iResolution;           // viewport image resolution (in pixels)
uniform mat4 iTransform;            // image transformation
uniform vec4 color;
uniform vec4 uv;

// Image Shader
uniform sampler2D iChannel0;             // input channel (texture id).
uniform sampler2D iChannel1;             // input mask
uniform float stipple;

void main()
{
    // adjust UV
    vec4 texcoord = iTransform * vec4(vertexUV.x, vertexUV.y, 0.0, 1.0);

    // color is a mix of texture (manipulated with brightness & contrast), vertex and uniform colors
    vec4 textureColor = texture(iChannel0, texcoord.xy);
    vec3 RGB = textureColor.rgb * vertexColor.rgb * color.rgb;

    // alpha is a mix of texture alpha, vertex alpha, and uniform alpha affected by stippling
    vec4 maskColor = texture(iChannel1, vertexUV);
    float maskIntensity = (maskColor.r + maskColor.g + maskColor.b) / 3.0;

    float A = textureColor.a * vertexColor.a * color.a * maskIntensity;
    A += stipple * ( int(gl_FragCoord.x + gl_FragCoord.y) % 2 );

    // output RGBA
    FragColor = vec4(RGB, clamp(A, 0.0, 1.0) );
}
