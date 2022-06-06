uniform float Tolerance;


//float sRGBtoLin( in float v ) {
//    // Send this function a decimal sRGB gamma encoded color value
//    // between 0.0 and 1.0, and it returns a linearized value.
//    if ( v <= 0.04045 ) {
//        return v / 12.92;
//    } else {
//        return pow((( v + 0.055)/1.055),2.4);
//    }
//}

//// returns L* which is "perceptual lightness"
//float lightness ( in vec3 color )
//{
//    float Y = 0.2126 * sRGBtoLin(color.r)
//            + 0.7152 * sRGBtoLin(color.g)
//            + 0.0722 * sRGBtoLin(color.b);

//    if ( Y <= (216./24389.) ) {     // The CIE standard states 0.008856 but 216/24389 is the intent for 0.008856451679036
//        Y =  Y * (24389./27.);      // The CIE standard states 903.3, but 24389/27 is the intent, making 903.296296296296296
//    } else {
//        Y = pow(Y,(1./3.)) * 116. - 16.;
//    }

//    return 0.01 * Y;
//}

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    vec2 fragPos =  fragCoord.xy / iResolution.xy;
    vec3 RGB = texture(iChannel0, fragPos).rgb;


    float L = dot(RGB, vec3(0.299, 0.587, 0.114));
//    float L = lightness( RGB );

    fragColor = vec4( RGB, smoothstep( 0.0, Tolerance, L ) );
}
