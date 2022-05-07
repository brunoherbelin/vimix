#define SIGMA 10.0
#define BSIGMA 0.1

float kernel[15];

float normpdf(in float x, in float sigma)
{
	return 0.39894*exp(-0.5*x*x/(sigma*sigma))/sigma;
}

float normpdf3(in vec3 v, in float sigma)
{
	return 0.39894*exp(-0.5*dot(v,v)/(sigma*sigma))/sigma;
}

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    vec3 c = texture(iChannel0, (fragCoord.xy / iResolution.xy)).rgb;
   
    kernel[0] = 0.031225216;
    kernel[1] = 0.033322271;
    kernel[2] = 0.035206333;
    kernel[3] = 0.036826804;
    kernel[4] = 0.038138565;
    kernel[5] = 0.039104044;
    kernel[6] = 0.039695028;
    kernel[7] = 0.039894000;
    kernel[8] = 0.039695028;
    kernel[9] = 0.039104044;
    kernel[10] = 0.038138565;
    kernel[11] = 0.036826804;
    kernel[12] = 0.035206333;
    kernel[13] = 0.033322271;
    kernel[14] = 0.031225216;
    
    vec3 final_colour = vec3(0.0);
    float Z = 0.0;
    vec3 cc;
    float f;
    float bZ = 1.0/normpdf(0.0, mix(BSIGMA, 1.0, .0) );
    
    for (int i = -7; i <= 7; ++i)
    {
        for (int j = -7; j <= 7; ++j)
        {
            cc = texture(iChannel0, (fragCoord.xy+vec2(float(i),float(j))) / iResolution.xy).rgb;
            f = normpdf3(cc-c, mix(BSIGMA, 1.0, .0) ) * bZ * kernel[7+j] * kernel[7+i];
            Z += f;
            final_colour += f*cc;
        }
    }
    fragColor = vec4(final_colour/Z, 1.0);
}
