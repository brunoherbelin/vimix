//  Bilateral filter
// (c) mrharicot https://www.shadertoy.com/view/4dfGDH

#define SIGMA 10.0
#define MSIZE 13

uniform float Factor;

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
    vec4 c = texture(iChannel0, (fragCoord.xy / iResolution.xy) );

    const int kSize = (MSIZE-1)/2;
    float kernel[MSIZE];
    vec4 final_colour = vec4(0.0);

    //create the 1-D kernel
    float Z = 0.0;
    for (int j = 0; j <= kSize; ++j)
    {
        kernel[kSize+j] = kernel[kSize-j] = normpdf(float(j), SIGMA);
    }

    vec4 cc;
    float fac;
    float bsigma = mix(0.01, 1.2, Factor);
    float bZ = 1.0/normpdf(0.0, bsigma);
    for (int i=-kSize; i <= kSize; ++i)
    {
        for (int j=-kSize; j <= kSize; ++j)
        {
            cc = texture(iChannel0, (fragCoord.xy+vec2(float(i),float(j))) / iResolution.xy);
            fac = normpdf3(cc-c, bsigma)*bZ*kernel[kSize+j]*kernel[kSize+i];
            Z += fac;
            final_colour += fac*cc;
        }
    }

    fragColor = final_colour/Z;
}

