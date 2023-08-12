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
uniform vec2  size;            // size of the mask area
uniform float blur;            // percent of blur

float msign(in float x) { return (x<0.0)?-1.0:1.0; }

// MIT License
// Copyright 2021 Henrik Dick
// https://www.shadertoy.com/view/slS3Rw
float sdEllipse(vec2 p, vec2 e)
{
    float x = p.x;
    float y = p.y;
    float ax = abs(p.x);
    float ay = abs(p.y);
    float a = e.x;
    float b = e.y;
    float aa = e.x*e.x;
    float bb = e.y*e.y;

    vec2 closest = vec2(0.0);
    int iterations = 0;

    // edge special case, handle as AABB
    if (a * b <= 1e-15) {
        closest = clamp(p, -e, e);
        return length(closest-p);
    }

    // this epsilon will guarantee float precision result
    // (error<1e-6) for degenerate cases
    float epsilon = 1e-3;
    float diff = bb - aa;
    if (a < b) {
        if (ax <= epsilon * a) {
            if (ay * b < diff) {
                float yc = bb * y / diff;
                float xc = a * sqrt(1.0 - yc * yc / bb);
                closest = vec2(xc, yc);
                return -length(closest-p);
            }
            closest = vec2(x, b * msign(y));
            return ay - b;
        }
        else if (ay <= epsilon * b) {
            closest = vec2(a * msign(x), y);
            return ax - a;
        }
    }
    else {
        if (ay <= epsilon * b) {
            if (ax * a < -diff) {
                float xc = aa * x / -diff;
                float yc = b * sqrt(1.0 - xc * xc / aa);
                closest = vec2(xc, yc);
                return -length(closest-p);
            }
            closest = vec2(a * msign(x), y);
            return ax - a;
        }
        else if (ax <= epsilon * a) {
            closest = vec2(x, b * msign(y));
            return ay - b;
        }
    }

    float rx = x / a;
    float ry = y / b;
    float inside = rx*rx + ry*ry - 1.0;

    // get lower/upper bound for parameter t
    float s2 = sqrt(2.0);
    float tmin = max(a * ax - aa, b * ay - bb);
    float tmax = max(s2 * a * ax - aa, s2 * b * ay - bb);

    float xx = x * x * aa;
    float yy = y * y * bb;
    float rxx = rx * rx;
    float ryy = ry * ry;
    float t;
    if (inside < 0.0) {
        tmax = min(tmax, 0.0);
        if (ryy < 1.0)
            tmin = max(tmin, sqrt(xx / (1.0 - ryy)) - aa);
        if (rxx < 1.0)
            tmin = max(tmin, sqrt(yy / (1.0 - rxx)) - bb);
        t = tmin * 0.95;
    }
    else {
        tmin = max(tmin, 0.0);
        if (ryy < 1.0)
            tmax = min(tmax, sqrt(xx / (1.0 - ryy)) - aa);
        if (rxx < 1.0)
            tmax = min(tmax, sqrt(yy / (1.0 - rxx)) - bb);
        t = tmin;//2.0 * tmin * tmax / (tmin + tmax);
    }
    t = clamp(t, tmin, tmax);

    int newton_steps = 12;
    if (tmin >= tmax) {
        t = tmin;
        newton_steps = 0;
    }

    // iterate, most of the time 3 iterations are sufficient.
    // bisect/newton hybrid
    int i;
    for (i = 0; i < newton_steps; i++) {
        float at = aa + t;
        float bt = bb + t;
        float abt = at * bt;
        float xxbt = xx * bt;
        float yyat = yy * at;

        float f0 = xxbt * bt + yyat * at - abt * abt;
        float f1 = 2.0 * (xxbt + yyat - abt * (bt + at));
        // bisect
        if (f0 < 0.0)
            tmax = t;
        else if (f0 > 0.0)
            tmin = t;
        // newton iteration
        float newton = f0 / abs(f1);
        newton = clamp(newton, tmin-t, tmax-t);
        newton = min(newton, a*b*2.0);
        t += newton;

        float absnewton = abs(newton);
        if (absnewton < 1e-6 * (abs(t) + 0.1) || tmin >= tmax)
            break;
    }
    iterations = i;

    closest = vec2(x * a / (aa + t), y * b / (bb + t));
    // this normalization is a tradeoff in precision types
    closest = normalize(closest);
    closest *= e;
    return length(closest-p) * msign(inside);
}

void main()
{
    vec2 uv = -1.0 + 2.0 * gl_FragCoord.xy / iResolution.xy;
    uv.x *= iResolution.x / iResolution.y;

    float d = sdEllipse( uv, vec2(size.x * iResolution.x/iResolution.y, size.y)  );

    vec3 col = vec3(1.0- msign(d));
    col *= 1.0 - exp( -600.0/ (blur * 1000.f + 1.0) * abs(d));

    FragColor = vec4( col, 1.0 );
}
