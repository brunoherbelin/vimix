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
uniform vec2 size;
uniform float blur;

// See: http://www.iquilezles.org/www/articles/ellipsoids/ellipsoids.htm
float sdEllipsoid( in vec2 p, in vec2 r )
{
    float k0 = length(p/r);
    float k1 = length(p/(r*r));
    return k0*(k0-1.0)/k1;
}

float sdEllipse( in vec2 p, in vec2 e )
{
    p = abs( p );

    if( e.x<e.y ) { p = p.yx; e = e.yx; }

    vec2 r = e*e;
    vec2 z = p/e;
    vec2 n = r*z;

    float g = dot(z,z) - 1.0;
    float s0 = z.y - 1.0;
    float s1 = (g<0.0) ? 0.0 : length( n )/r.y - 1.0;
    float s = 0.0;
    for( int i=0; i<64; i++ )
    {
        s = 0.5*(s0+s1);
        vec2 ratio = n/(r.y*s+r);
        g = dot(ratio,ratio) - 1.0;
        if( g>0.0 ) s0=s; else s1=s;
    }

    vec2 q = p*r/(r.y*s+r);
    return length( p-q ) * (((p.y-q.y)<0.0)?-1.0:1.0);
}

void main()
{
    vec2 uv = -1.0 + 2.0 * gl_FragCoord.xy / iResolution.xy;
    uv.x *= iResolution.x / iResolution.y;

    float d = sdEllipse( uv, vec2(size.x * iResolution.x/iResolution.y, size.y)  );

    vec3 col = vec3(1.0- sign(d));
    col *= 1.0 - exp( -60.0/ (blur * 100.f + 1.0) * abs(d));

    FragColor = vec4( col, 1.0 );
}
