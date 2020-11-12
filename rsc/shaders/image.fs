#version 330 core

out vec4 FragColor;

in vec4 vertexColor;
in vec2 vertexUV;

uniform sampler2D iChannel0;             // input channel (texture id).
uniform sampler2D iChannel1;             // input mask
uniform vec3      iResolution;           // viewport resolution (in pixels)

uniform vec4 color;
uniform float stipple;
uniform vec4 uv;

void main()
{
    // adjust UV
    vec2 texcoord = vec2(uv.x, uv.y) + vertexUV * vec2(uv.z - uv.x, uv.w - uv.y);

    // color is a mix of texture (manipulated with brightness & contrast), vertex and uniform colors
    vec4 textureColor = texture(iChannel0, texcoord);
    vec3 RGB = textureColor.rgb * vertexColor.rgb * color.rgb;

    // alpha is a mix of texture alpha, vertex alpha, and uniform alpha affected by stippling
    vec4 maskColor = texture(iChannel1, vertexUV);
    float maskIntensity = (maskColor.r + maskColor.g + maskColor.b) / 3.0;

    float A = textureColor.a * vertexColor.a * color.a * maskIntensity;
    A -= stipple * ( int(gl_FragCoord.x + gl_FragCoord.y) % 2 > 0 ? 0.05 : 0.95 );

    // output RGBA
    FragColor = vec4(RGB, clamp(A, 0.0, 1.0) );
}
