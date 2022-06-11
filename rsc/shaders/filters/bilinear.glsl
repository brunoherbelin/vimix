// Fast simple bilinear RGBA filter
// by Bruno Herbelin

#define SIGMA 10.0

uniform float Factor;

float kernel[15] = {0.031225216, 0.033322271, 0.035206333, 0.036826804, 0.038138565,
0.039104044, 0.039695028, 0.039894000, 0.039695028, 0.039104044,
0.038138565, 0.036826804, 0.035206333, 0.033322271, 0.031225216 };

float normpdf(in float x, in float sigma)
{
    return 0.39894*exp(-0.5*x*x/(sigma*sigma))/sigma;
}

float normpdf3(in vec4 v, in float sigma)
{
    return 0.39894*exp(-0.5*dot(v,v)/(sigma*sigma))/sigma;
}

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    vec4 O = vec4(0.0);
    float Z = 0.0;
    vec4 cc;
    float bsigma = mix(0.01, 0.5, Factor);
    float bZ = 1.0/normpdf(0.0, mix(bsigma, 1.0, .0) );
    vec4 C = texture(iChannel0, (fragCoord.xy / iResolution.xy));

    for (int i = -7; i <= 7; ++i)
    {
        for (int j = -7; j <= 7; ++j)
        {
            cc = texture(iChannel0, (fragCoord+vec2(float(i),float(j))) / iResolution.xy);
            float f = normpdf3(cc -C, mix(bsigma, 1.0, .0) ) * bZ * kernel[7+j] * kernel[7+i];
            Z += f;
            O += f*cc;
        }
    }
    fragColor = O/Z;
}
