/*
 * This file is part of vimix - video live mixer
 * https://github.com/brunoherbelin/vimix
 * (C) 2019-2022 Bruno Herbelin <bruno.herbelin@gmail.com>
 * Distributed under GNU GPL3+ License
**/
//=======================================================================================
vec4 CubicHermite (vec4 A, vec4 B, vec4 C, vec4 D, float t)
{
    float t2 = t*t;
    float t3 = t*t*t;
    vec4 a = -A/2.0 + (3.0*B)/2.0 - (3.0*C)/2.0 + D/2.0;
    vec4 b = A - (5.0*B)/2.0 + 2.0*C - D / 2.0;
    vec4 c = -A/2.0 + C/2.0;
    vec4 d = B;

    return a*t3 + b*t2 + c*t + d;
}

//=======================================================================================
vec4 BicubicHermiteTextureSample (vec2 P)
{
    float c_textureSize = iChannelResolution[0].x;
    float c_onePixel = 1.0 / c_textureSize;
    float c_twoPixels = 2.0 / c_textureSize;

    vec2 pixel = P * c_textureSize + 0.5;

    vec2 frac = fract(pixel);
    pixel = floor(pixel) / c_textureSize - vec2(c_onePixel/2.0);

    vec4 C00 = texture(iChannel0, pixel + vec2(-c_onePixel ,-c_onePixel));
    vec4 C10 = texture(iChannel0, pixel + vec2( 0.0        ,-c_onePixel));
    vec4 C20 = texture(iChannel0, pixel + vec2( c_onePixel ,-c_onePixel));
    vec4 C30 = texture(iChannel0, pixel + vec2( c_twoPixels,-c_onePixel));

    vec4 C01 = texture(iChannel0, pixel + vec2(-c_onePixel , 0.0));
    vec4 C11 = texture(iChannel0, pixel + vec2( 0.0        , 0.0));
    vec4 C21 = texture(iChannel0, pixel + vec2( c_onePixel , 0.0));
    vec4 C31 = texture(iChannel0, pixel + vec2( c_twoPixels, 0.0));

    vec4 C02 = texture(iChannel0, pixel + vec2(-c_onePixel , c_onePixel));
    vec4 C12 = texture(iChannel0, pixel + vec2( 0.0        , c_onePixel));
    vec4 C22 = texture(iChannel0, pixel + vec2( c_onePixel , c_onePixel));
    vec4 C32 = texture(iChannel0, pixel + vec2( c_twoPixels, c_onePixel));

    vec4 C03 = texture(iChannel0, pixel + vec2(-c_onePixel , c_twoPixels));
    vec4 C13 = texture(iChannel0, pixel + vec2( 0.0        , c_twoPixels));
    vec4 C23 = texture(iChannel0, pixel + vec2( c_onePixel , c_twoPixels));
    vec4 C33 = texture(iChannel0, pixel + vec2( c_twoPixels, c_twoPixels));

    vec4 CP0X = CubicHermite(C00, C10, C20, C30, frac.x);
    vec4 CP1X = CubicHermite(C01, C11, C21, C31, frac.x);
    vec4 CP2X = CubicHermite(C02, C12, C22, C32, frac.x);
    vec4 CP3X = CubicHermite(C03, C13, C23, C33, frac.x);

    return CubicHermite(CP0X, CP1X, CP2X, CP3X, frac.y);
}

// Insprired from https://www.shadertoy.com/view/MtVGWz
// Reference http://www.decarpentier.nl/2d-catmull-rom-in-4-samples
vec4 SampleTextureCatmullRom( vec2 uv, vec2 texSize )
{
    // We're going to sample a a 4x4 grid of texels surrounding the target UV coordinate. We'll do this by rounding
    // down the sample location to get the exact center of our "starting" texel. The starting texel will be at
    // location [1, 1] in the grid, where [0, 0] is the top left corner.
    vec2 samplePos = uv * texSize;
    vec2 texPos1 = floor(samplePos - 0.5) + 0.5;

    // Compute the fractional offset from our starting texel to our original sample location, which we'll
    // feed into the Catmull-Rom spline function to get our filter weights.
    vec2 f = samplePos - texPos1;

    // Compute the Catmull-Rom weights using the fractional offset that we calculated earlier.
    // These equations are pre-expanded based on our knowledge of where the texels will be located,
    // which lets us avoid having to evaluate a piece-wise function.
    vec2 w0 = f * ( -0.5 + f * (1.0 - 0.5*f));
    vec2 w1 = 1.0 + f * f * (-2.5 + 1.5*f);
    vec2 w2 = f * ( 0.5 + f * (2.0 - 1.5*f) );
    vec2 w3 = f * f * (-0.5 + 0.5 * f);

    // Work out weighting factors and sampling offsets that will let us use bilinear filtering to
    // simultaneously evaluate the middle 2 samples from the 4x4 grid.
    vec2 w12 = w1 + w2;
    vec2 offset12 = w2 / w12;

    // Compute the final UV coordinates we'll use for sampling the texture
    vec2 texPos0 = texPos1 - vec2(1.0);
    vec2 texPos3 = texPos1 + vec2(2.0);
    vec2 texPos12 = texPos1 + offset12;

    texPos0 /= texSize;
    texPos3 /= texSize;
    texPos12 /= texSize;

    vec4 result = vec4(0.0);
    result += texture( iChannel0, vec2(texPos0.x,  texPos0.y)) * w0.x * w0.y;
    result += texture( iChannel0, vec2(texPos12.x, texPos0.y)) * w12.x * w0.y;
    result += texture( iChannel0, vec2(texPos3.x,  texPos0.y)) * w3.x * w0.y;

    result += texture( iChannel0, vec2(texPos0.x,  texPos12.y)) * w0.x * w12.y;
    result += texture( iChannel0, vec2(texPos12.x, texPos12.y)) * w12.x * w12.y;
    result += texture( iChannel0, vec2(texPos3.x,  texPos12.y)) * w3.x * w12.y;

    result += texture( iChannel0, vec2(texPos0.x,  texPos3.y)) * w0.x * w3.y;
    result += texture( iChannel0, vec2(texPos12.x, texPos3.y)) * w12.x * w3.y;
    result += texture( iChannel0, vec2(texPos3.x,  texPos3.y)) * w3.x * w3.y;

    return result;
}

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    vec2 uv = fragCoord.xy / iResolution.xy;
    fragColor = SampleTextureCatmullRom(uv, iChannelResolution[0].xy );

//    fragColor = BicubicHermiteTextureSample(uv);
}
