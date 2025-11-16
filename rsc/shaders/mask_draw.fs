#version 330 core

out vec4 FragColor;

in vec4 vertexColor;
in vec2 vertexUV;

// from General Shader
uniform vec3 iResolution;      // viewport resolution (in pixels)
uniform mat4 iTransform;       // image transformation
uniform vec4 color;            // drawing color
uniform vec4 uv;

// Mask Shader
uniform sampler2D iChannel0;   // base texture
uniform vec2  size;            // size of the mask area
uniform float blur;            // percent of blur

uniform vec4  cursor;
uniform vec3  brush;
uniform int   option;
uniform int   effect;

float sdBox( in vec2 v1, in vec2 v2, float r )
{
    vec2 ba = v2 - v1;
    vec2 pa = -v1;
    float h = clamp( dot(pa,ba)/dot(ba,ba), 0.0, 1.0 );
    vec2 q = abs(pa -h*ba) - vec2(r);
    float d = min( max(q.x, q.y), 0.0) + length(max(q,0.0));
    return 1.0 - abs( mix( 0.5, 1.0, d) ) * step(d, 0.0);
}

float sdSegment( in vec2 v1, in vec2 v2, float r )
{
    vec2 ba = v2 - v1;
    vec2 pa = -v1;
    float h = clamp( dot(pa,ba) / dot(ba,ba), 0.0, 1.0 );
    return length(pa-h*ba) / r;
}

const mat3 KERNEL = mat3( 0.0555555, 0.111111, 0.0555555,
                          0.111111,  0.333334, 0.111111,
                          0.0555555, 0.111111, 0.0555555); 

vec3 gaussian()
{
    vec2 filter_step = 1.f / textureSize(iChannel0, 0);
    int i = 0, j = 0;
    vec3 sum = vec3(0.0);

    for (i = 0; i<3; ++i)
        for (j = 0; j<3; ++j)
            sum += texture(iChannel0, vertexUV.xy + filter_step * vec2 (i-1, j-1) ).rgb * KERNEL[i][j];

    return sum;
}

void main()
{
    vec3 color = texture(iChannel0, vertexUV).rgb; // raw color

    // clear
    color += mix(vec3(0), vec3(1) - color, int(effect == 1));
    // invert
    color = mix(color, vec3(1) - color, int(effect == 2));
    // step edge
    color = mix(color, vec3(1.0 - step(dot(color, vec3(1.0/3.0)), 0.6)), int(effect == 3));

    if ( option > 0 ) {

        // fragment coordinates
        vec2 uv =  -1.0 + 2.0 * gl_FragCoord.xy / iResolution.xy;
        // adjust coordinates to match scaling area
        uv.x *= size.x ;
        uv.y *= size.y ;
        // cursor coordinates
        vec2 cur = vec2(cursor.x, - cursor.y);
        vec2 cur_prev = vec2(cursor.z, - cursor.w);
        // use distance function relative to length brush.x (size), depending on brush.z (shape):
        // - brush.z = 0 : circle shape
        // - brush.z = 1 : square shape
        float d = (1.0 -brush.z) * sdSegment(cur_prev - uv, cur - uv, brush.x) + brush.z * sdBox(cur_prev - uv, cur - uv, brush.x);

        // modify only the pixels inside the brush
        if( d < 1.0 )
        {
            // blur the image incrementally each step
            color = gaussian();

            // mask intensity
            float c = dot(color, vec3(1.0/3.0));

            // depending on option:
            // - option 1 : paint (add color)
            // - option 2 : erase (substract color)
            float p = c + (option > 1 ? d - 1.0 : 1.0 - d );

            // new intensity is a mix betwen current intensity and painted intensity
            c = (1.0 - brush.y) * c + (brush.y) * p;

            // distribute value in RGB (will be averaged in image.fs)
            // NB : this encodes the mask intensity over 3*8 bits
            c *= 3.0;
            color.r = clamp(c, 0.0, 1.0);
            color.g = step(1.0, c) * (clamp(c, 1.0, 2.0) - 1.0);
            color.b = step(2.0, c) * (clamp(c, 2.0, 3.0) - 2.0);

        }

    }


    FragColor = vec4( clamp(color, 0.0, 1.0), 1.0);
}



