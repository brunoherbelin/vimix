uniform float Amount; 

vec4 sharp(sampler2D sampler, vec2 fc, vec2 re)
{
    float strength = mix( 2.0, 20.0, Amount);
        vec2 uv = fc / re;
        vec4 c0 = texture(sampler,uv);
        vec4 c1 = texture(sampler,uv-vec2( 1./re.x,.0));
        vec4 c2 = texture(sampler,uv+vec2( 1./re.x,.0));
        vec4 c3 = texture(sampler,uv-vec2(.0, 1./re.y));
        vec4 c4 = texture(sampler,uv+vec2(.0, 1./re.y));
        vec4 c5 = c0+c1+c2+c3+c4;
        c5*=0.2;
        vec4 mi = min(c0,c1); mi = min(mi,c2); mi = min(mi,c3); mi = min(mi,c4);
        vec4 ma = max(c0,c1); ma = max(ma,c2); ma = max(ma,c3); ma = max(ma,c4);
        return clamp(mi,(strength+1.0)*c0-c5*strength,ma);
}

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
        fragColor = sharp(iChannel0, fragCoord.xy, iResolution.xy);
}

