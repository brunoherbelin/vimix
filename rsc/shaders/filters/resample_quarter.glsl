
void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    vec2 uv = fragCoord.xy / iResolution.xy;
    fragColor = texture( iChannel0, uv );

////    vec2 uv = fragCoord.xy / iChannelResolution[0].xy;

////        float textureResolution = iChannelResolution[0].x;
////        uv = uv*textureResolution + 0.5;
////        vec2 iuv = floor( uv );
////        vec2 fuv = fract( uv );
////        uv = iuv + fuv*fuv*(3.0-2.0*fuv); // fuv*fuv*fuv*(fuv*(fuv*6.0-15.0)+10.0);;
////        uv = (uv - 0.5)/textureResolution;

//    vec4 col2  = texture(iChannel1, uv);

//    // optimized lowpass 2X downsampling filter.
//    vec4 col = vec4(0.0);
////    col += 0.37487566 * texture(iChannel0, uv + vec2(-0.75777156,-0.75777156)/iChannelResolution[0].xy);
////    col += 0.37487566 * texture(iChannel0, uv + vec2(0.75777156,-0.75777156)/iChannelResolution[0].xy);
////    col += 0.37487566 * texture(iChannel0, uv + vec2(0.75777156,0.75777156)/iChannelResolution[0].xy);
////    col += 0.37487566 * texture(iChannel0, uv + vec2(-0.75777156,0.75777156)/iChannelResolution[0].xy);
////    col += -0.12487566 * texture(iChannel0, uv + vec2(-2.90709914,0.0)/iChannelResolution[0].xy);
////    col += -0.12487566 * texture(iChannel0, uv + vec2(2.90709914,0.0)/iChannelResolution[0].xy);
////    col += -0.12487566 * texture(iChannel0, uv + vec2(0.0,-2.90709914)/iChannelResolution[0].xy);
////    col += -0.12487566 * texture(iChannel0, uv + vec2(0.0,2.90709914)/iChannelResolution[0].xy);

//col = texture( iChannel0, uv );

//    fragColor = 0.5 * col + 0.5 * col2;
}
