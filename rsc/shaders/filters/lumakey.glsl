/*
 * This file is part of vimix - video live mixer
 * https://github.com/brunoherbelin/vimix
 * (C) 2019-2022 Bruno Herbelin <bruno.herbelin@gmail.com>
 * Distributed under GNU GPL3+ License
**/
uniform float Luminance;
uniform float Threshold;
uniform float Tolerance;


float sRGBtoLin( in float v ) {
    // Send this function a decimal sRGB gamma encoded color value
    // between 0.0 and 1.0, and it returns a linearized value.
    if ( v <= 0.04045 ) {
        return v / 12.92;
    } else {
        return pow((( v + 0.055)/1.055),2.4);
    }
}

// returns L* which is "perceptual lightness"
float lightness ( in vec3 color )
{
    float Y = 0.2126 * sRGBtoLin(color.r)
            + 0.7152 * sRGBtoLin(color.g)
            + 0.0722 * sRGBtoLin(color.b);

    if ( Y <= (216./24389.) ) {     // The CIE standard states 0.008856 but 216/24389 is the intent for 0.008856451679036
        Y =  Y * (24389./27.);      // The CIE standard states 903.3, but 24389/27 is the intent, making 903.296296296296296
    } else {
        Y = pow(Y,(1./3.)) * 116. - 16.;
    }

    return 0.01 * Y;
}

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    vec4 texColor0 = texture(iChannel0, fragCoord.xy / iResolution.xy);

    // RGB is alpha-premultiplied : revert
    texColor0 = vec4( texColor0.rgb / max(texColor0.a, 0.001), texColor0.a);

    // modify alpha by the lightness of the image
    float L = lightness( texColor0.rgb );
    texColor0.a *= smoothstep( Threshold, Threshold + Tolerance * Tolerance, abs(Luminance - L) );

    fragColor = texColor0;
}
