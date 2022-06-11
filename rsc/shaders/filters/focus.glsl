// Bilateral filter
// inspired from mrharicot https://www.shadertoy.com/view/4dfGDH

#define SIGMA 10.0
#define BSIGMA 0.1
#define MSIZE 13

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
    const int kSize = (MSIZE-1)/2;
    float kernel[MSIZE];
    vec4 O = vec4(0.0);

    //create the 1-D kernel
    float Z = 0.0;
    for (int j = 0; j <= kSize; ++j)
        kernel[kSize+j] = kernel[kSize-j] = normpdf(float(j), SIGMA);

    vec4 cc;
    vec4 c = texture(iChannel0, (fragCoord.xy / iResolution.xy) );
    float bZ = 1.0/normpdf(0.0, BSIGMA);
    for (int i=-kSize; i <= kSize; ++i)
    {
        for (int j=-kSize; j <= kSize; ++j)
        {
            cc = texture(iChannel0, (fragCoord+vec2(float(i),float(j))) / iResolution.xy);
            float fac = normpdf3(cc-c, BSIGMA)*bZ*kernel[kSize+j]*kernel[kSize+i];
            Z += fac;
            O += fac*cc;
        }
    }

    fragColor = O/Z;
}

